/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_OPENROUTER_TRANSCRIBE_H
#define DF0_OPENROUTER_TRANSCRIBE_H

#include "audit/auditor_ref.hpp"
#include "audit/entry.hpp"
#include "audit/scope.hpp"
#include "audit/with_auditor.hpp"
#include "base64.h"
#include "data_source.h"
#include "document_elements.h"
#include "error_tags.h"
#include "make_error.h"
#include "named.h"
#include "pipeline/element_base.hpp"
#include "pipeline/input_element.hpp"
#include "pipeline/output_element.hpp"
#include "post.h"
#include <boost/json.hpp>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace df0 {
namespace openrouter {

template <typename auditor_type = audit::null_auditor>
class transcribe
    : public pipeline::transformer_element<transcribe<auditor_type>>,
      private audit::with_auditor<auditor_type> {
public:
  /**
   * Available only for stateless (default-constructible) auditors.
   * @param api_key OpenRouter API key. Never written to audit output.
   * @param model Defaults to `whisper_large_v3 (openai/whisper-large-v3)`.
   * @param language Optional ISO-639-1 code (e.g. "en"); empty = auto-detect.
   */
  template <audit::stateless_auditor stateless_auditor_type = auditor_type>
  explicit transcribe(
      std::string api_key,
      std::string model,
      std::string language = "")
      : transcribe(std::move(api_key), std::move(model), std::move(language),
                   audit::auditor_ref<stateless_auditor_type>{}) {}

  explicit transcribe(std::string api_key, std::string model,
                      std::string language,
                      audit::auditor_ref<auditor_type> auditor)
      : audit::with_auditor<auditor_type>(auditor),
        m_api_key(std::move(api_key)), m_model(std::move(model)),
        m_language(std::move(language)) {
    audit::scope audit_scope{this->auditor(), [&] {
      return "model"_v = m_model, "language"_v = m_language;
    }};
  }

  continuation operator()(message_ptr msg,
                          const message_callbacks &emit_message) {
    audit::scope transcribe_audit_scope{this->auditor(), audit::no_context};
    if (!msg->is<data_source>())
      return emit_message(std::move(msg));
    audit::entry(this->auditor(), audit::no_context);
    const data_source &data = msg->get<data_source>();

    std::string text = parse_response(post_request(
        prepare_query(audio_format(data), base64::encode(data.span()))));

    auto cont = emit_message(document::document{});
    if (cont == continuation::stop)
      return cont;
    cont = emit_message(document::text{text});
    if (cont == continuation::stop)
      return cont;
    return emit_message(document::close_document{});
  }

protected:
  using audit::with_auditor<auditor_type>::auditor;

private:
  static std::string audio_format(const data_source &data) {
    static const std::map<std::string, std::string> by_extension{
        {".wav", "wav"}, {".mp3", "mp3"}, {".flac", "flac"}, {".m4a", "m4a"},
        {".ogg", "ogg"}, {".oga", "ogg"}, {".webm", "webm"}, {".aac", "aac"}};
    static const std::map<std::string, std::string> by_mime{
        {"audio/wav", "wav"},     {"audio/wave", "wav"},
        {"audio/x-wav", "wav"},   {"audio/mpeg", "mp3"},
        {"audio/mp3", "mp3"},     {"audio/flac", "flac"},
        {"audio/x-flac", "flac"}, {"audio/mp4", "m4a"},
        {"audio/x-m4a", "m4a"},   {"audio/ogg", "ogg"},
        {"audio/webm", "webm"},   {"audio/aac", "aac"}};
    if (auto ext = data.file_extension())
      if (auto it = by_extension.find(ext->string()); it != by_extension.end())
        return it->second;
    if (auto mt = data.highest_confidence_mime_type())
      if (auto it = by_mime.find(mt->v); it != by_mime.end())
        return it->second;
    throw DF0_MAKE_ERROR("Unable to determine audio format for OpenRouter "
                     "transcription (unknown file extension and MIME type)",
                     errors::uninterpretable_data{});
  }

  std::string prepare_query(const std::string &format,
                            const std::string &audio_base64) const {
    boost::json::object query{
        {"model", m_model},
        {"input_audio",
         boost::json::object{{"data", audio_base64}, {"format", format}}}};
    if (!m_language.empty())
      query["language"] = m_language;
    return boost::json::serialize(query);
  }

  std::string post_request(const std::string &query) const {
    audit::scope audit_scope{this->auditor(), audit::no_context};
    std::ostringstream response_stream{};
    try {
      data_source{query, mime_type{"application/json"}, confidence::highest} |
          http::post<auditor_type>(
              "https://openrouter.ai/api/v1/audio/transcriptions", m_api_key,
              http::ssl_verify_peer{true}, this->auditor()) |
          response_stream;
    } catch (const std::exception &) {
      std::throw_with_nested(DF0_MAKE_ERROR("Error during transcription"));
    }
    return response_stream.str();
  }

  std::string parse_response(const std::string &response) const {
    audit::scope audit_scope{this->auditor(), audit::no_context};
    try {
      boost::json::object obj = boost::json::parse(response).as_object();
      if (obj.contains("error")) {
        const boost::json::value &err = obj.at("error");
        std::string message =
            err.is_object() && err.as_object().contains("message")
                ? std::string{err.as_object().at("message").as_string().c_str()}
                : "unknown error";
        throw DF0_MAKE_ERROR("OpenRouter API returned an error", message);
      }
      return obj.at("text").as_string().c_str();
    } catch (const std::exception &) {
      std::throw_with_nested(DF0_MAKE_ERROR(response));
    }
  }

  std::string m_api_key;
  std::string m_model;
  std::string m_language;
};

} // namespace openrouter
} // namespace df0

#endif // DF0_OPENROUTER_TRANSCRIBE_H

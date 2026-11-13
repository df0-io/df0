/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_OPENROUTER_TEXT_TO_SPEECH_H
#define DF0_OPENROUTER_TEXT_TO_SPEECH_H

#include "audit/auditor_ref.hpp"
#include "audit/scope.hpp"
#include "audit/with_auditor.hpp"
#include "make_error.h"
#include "named.h"
#include "pipeline/element_base.hpp"
#include "pipeline/input_element.hpp"
#include "pipeline/output_element.hpp"
#include "post.h"
#include "serialization_message.h" // IWYU pragma: keep
#include <boost/json.hpp>
#include <sstream>
#include <string>

namespace df0 {
namespace openrouter {

template <typename auditor_type = audit::null_auditor>
class text_to_speech
    : public pipeline::transformer_element<text_to_speech<auditor_type>>,
      private audit::with_auditor<auditor_type> {
public:

  template <audit::stateless_auditor stateless_auditor_type = auditor_type>
  explicit text_to_speech(
      const std::string &api_key,
      const std::string &model,
      const std::string &voice,
      const std::string &response_format = "mp3")
      : text_to_speech(api_key, model, voice, response_format,
                       audit::auditor_ref<stateless_auditor_type>{}) {}

  explicit text_to_speech(const std::string &api_key, const std::string &model,
                          const std::string &voice,
                          const std::string &response_format,
                          audit::auditor_ref<auditor_type> auditor)
      : audit::with_auditor<auditor_type>(auditor), m_api_key(api_key),
        m_model(model), m_voice(voice), m_response_format(response_format) {

    audit::scope audit_scope{this->auditor(), [&] {
      return "model"_v = model, "voice"_v = voice,
             "response_format"_v = response_format;
    }};
  }

  continuation operator()(message_ptr msg,
                          const message_callbacks &emit_message) {
    audit::scope tts_audit_scope{this->auditor(), audit::no_context};
    if (!msg->is<data_source>())
      return emit_message(std::move(msg));
    const data_source &data = msg->get<data_source>();
    std::string data_str = data.string();
    std::string content = post_request(
        prepare_query(data_str, m_model, m_voice, m_response_format));
    return emit_message(data_source{std::move(content), file_extension{"." + m_response_format}});
  }

protected:
  using audit::with_auditor<auditor_type>::auditor;

private:
  std::string m_api_key;
  std::string m_model;
  std::string m_voice;
  std::string m_response_format;

  std::string prepare_query(const std::string &input, const std::string &model,
                            const std::string &voice,
                            const std::string &response_format) const {
    audit::scope audit_scope{this->auditor(), [&] {
      return "input"_v = input, "model"_v = model, "voice"_v = voice,
             "response_format"_v = response_format;
    }};
    boost::json::object query{{"model", model},
                              {"input", input},
                              {"voice", voice},
                              {"response_format", response_format}};
    return boost::json::serialize(query);
  }

  std::string post_request(const std::string &query) const {
    audit::scope audit_scope{this->auditor(), [&] { return "query"_v = query; }};
    std::ostringstream response_stream{};
    try {
      data_source{query, mime_type{"application/json"}, confidence::highest} |
          http::post<auditor_type>("https://openrouter.ai/api/v1/audio/speech",
                                   m_api_key, http::ssl_verify_peer{true},
                                   this->auditor()) |
          response_stream;
    } catch (const std::exception &e) {
      std::throw_with_nested(DF0_MAKE_ERROR(query));
    }
    return response_stream.str();
  }
};

} // namespace openrouter
} // namespace df0

#endif // DF0_OPENROUTER_TEXT_TO_SPEECH_H

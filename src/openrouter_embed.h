/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_OPENROUTER_EMBED_H
#define DF0_OPENROUTER_EMBED_H

#include "ai_elements.h"
#include "audit/auditor_ref.hpp"
#include "audit/entry.hpp"
#include "audit/scope.hpp"
#include "audit/with_auditor.hpp"
#include "error_tags.h"
#include "make_error.h"
#include "named.h"
#include "pipeline/element_base.hpp"
#include "pipeline/input_element.hpp"
#include "pipeline/output_element.hpp"
#include "post.h"
#include "serialization_message.h" // IWYU pragma: keep
#include "throw_if.h"
#include <boost/json.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace df0 {
namespace openrouter {

template <typename auditor_type = audit::null_auditor>
class embed : public pipeline::transformer_element<embed<auditor_type>>,
              private audit::with_auditor<auditor_type> {
public:
/**
   * @param api_key OpenRouter API key. Never written to audit output.
   * @param model OpenRouter embedding model slug (required), e.g.
   * "openai/text-embedding-3-small". OpenRouter has no router alias for
   * embeddings and the catalog changes over time; see
   * https://openrouter.ai/collections/embedding-models
 */
  template <audit::stateless_auditor stateless_auditor_type = auditor_type>
  explicit embed(const std::string &api_key,
                 const std::string &model)
      : embed(api_key, model, audit::auditor_ref<stateless_auditor_type>{}) {}

  explicit embed(const std::string &api_key, const std::string &model,
                 audit::auditor_ref<auditor_type> auditor)
      : audit::with_auditor<auditor_type>(auditor), m_api_key(api_key),
        m_model(model) {

    audit::scope audit_scope{this->auditor(), [&] { return "model"_v = model; }};
  }

  continuation operator()(message_ptr msg,
                          const message_callbacks &emit_message) {
    audit::scope embed_audit_scope{this->auditor(),
                                   [&] { return "msg"_v = msg; }};
    if (!msg->is<data_source>())
      return emit_message(std::move(msg));
    const data_source &data = msg->get<data_source>();
    DF0_THROW_IF(
        !data.has_highest_confidence_mime_type_in({mime_type{"text/plain"}}),
        errors::program_logic{});
    std::string data_str = data.string();
    std::vector<double> embedding_vector =
        parse_response(post_request(prepare_query(data_str)));
    return emit_message(ai::embedding{std::move(embedding_vector)});
  }

protected:
  using audit::with_auditor<auditor_type>::auditor;

private:
  std::string m_api_key;
  std::string m_model;

  std::string prepare_query(const std::string &input) const {
    audit::scope audit_scope{this->auditor(), [&] {
      return "input"_v = input, "model"_v = m_model;
    }};
    boost::json::object query{{"model", m_model}, {"input", input}};
    return boost::json::serialize(query);
  }

  std::string post_request(const std::string &query) const {
    audit::scope audit_scope{this->auditor(), [&] { return "query"_v = query; }};
    std::ostringstream response_stream{};
    try {
      data_source{query, mime_type{"application/json"}, confidence::highest} |
          http::post<auditor_type>("https://openrouter.ai/api/v1/embeddings",
                                   m_api_key, http::ssl_verify_peer{true},
                                   this->auditor()) |
          response_stream;
    } catch (const std::exception &e) {
      std::throw_with_nested(DF0_MAKE_ERROR(query));
    }
    return response_stream.str();
  }

  std::vector<double> parse_response(const std::string &response) const {
    audit::scope audit_scope{this->auditor(),
                             [&] { return "response"_v = response; }};
    try {
      boost::json::value response_val = boost::json::parse(response);
      boost::json::object response_obj = response_val.as_object();
      if (response_obj.contains("error")) {
        const boost::json::object &err = response_obj["error"].as_object();
        std::string message = err.contains("message")
                                  ? err.at("message").as_string().c_str()
                                  : "unknown error";
        throw DF0_MAKE_ERROR("OpenRouter API returned an error", message);
      }
      const auto &embedding_array = response_val.as_object()
                                        .at("data")
                                        .as_array()
                                        .at(0)
                                        .as_object()
                                        .at("embedding")
                                        .as_array();

      std::vector<double> embedding;
      embedding.reserve(embedding_array.size());
      for (const auto &val : embedding_array)
        embedding.push_back(val.to_number<double>());
      return embedding;
    } catch (const std::exception &e) {
      std::throw_with_nested(DF0_MAKE_ERROR(response));
    }
  }
};

} // namespace openrouter
} // namespace df0

#endif // DF0_OPENROUTER_EMBED_H

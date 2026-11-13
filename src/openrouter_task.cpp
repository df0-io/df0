/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#include "openrouter_task.h"

#include "audit/auditor_ref.hpp"
#include "audit/entry.hpp"
#include "audit/json_stream_auditor.hpp"
#include "audit/scope.hpp"
#include "audit/with_auditor.hpp"
#include "base64.h"
#include "error_tags.h"
#include "make_error.h"
#include "named.h"
#include "pipeline/input_element.hpp"
#include "pipeline/output_element.hpp"
#include "post.h"
#include "serialization_enum.h"    // IWYU pragma: keep
#include "serialization_message.h" // IWYU pragma: keep
#include <boost/json.hpp>
#include <sstream>

namespace {

std::string describe_error(const boost::json::value &error) {
  if (const auto *obj = error.if_object())
    if (const auto *msg = obj->if_contains("message"); msg && msg->is_string())
      return std::string{msg->as_string()};
  return "unknown error";
}

std::string snippet(const std::string &s, std::size_t max = 512) {
  return s.size() <= max
             ? s
             : s.substr(0, max) + "...[" + std::to_string(s.size()) +
                   " bytes total]";
}
} // anonymous namespace

namespace df0 {

using namespace openrouter;

template <typename auditor_type>
struct pimpl_impl<openrouter::task<auditor_type>>
    : pimpl_impl_base, private audit::with_auditor<auditor_type> {
  using audit::with_auditor<auditor_type>::auditor;

  explicit pimpl_impl(const std::string &system_message,
                      const std::string &api_key, const std::string &model,
                      float temperature, image_detail image_detail,
                      audit::auditor_ref<auditor_type> auditor, bool allow_truncated)
      : audit::with_auditor<auditor_type>{auditor},
        m_system_message(system_message), m_api_key(api_key), m_model(model),
        m_temperature(temperature), m_image_detail(image_detail),
        m_allow_truncated(allow_truncated) {}

  std::string m_system_message;
  std::string m_api_key;
  std::string m_model;
  float m_temperature;
  image_detail m_image_detail;
  bool m_allow_truncated;
};

} // namespace df0

namespace df0 {
namespace openrouter {

template <typename auditor_type>
task<auditor_type>::task(const std::string &system_message,
                         const std::string &api_key, const std::string &model,
                         float temperature, image_detail image_detail,
                         audit::auditor_ref<auditor_type> auditor, bool allow_truncated)
    : with_pimpl<task<auditor_type>>(system_message, api_key, model,
                                     temperature, image_detail,
                                     auditor, allow_truncated),
      audit::with_auditor<auditor_type>(auditor) {
  // api_key intentionally NOT audited (see DESIGN-AUDIT-UNIFIED, "Secrets").
  audit::scope audit_scope{this->auditor(), [&] {
                             return "system_message"_v = system_message,
                                    "model"_v = model,
                                    "temperature"_v = temperature,
                                    "image_detail"_v = image_detail;
                           }};
}

namespace {

std::string image_detail_to_string(openrouter::image_detail image_detail) {
  switch (image_detail) {
  case openrouter::image_detail::low:
    return "low";
  case openrouter::image_detail::high:
    return "high";
  case openrouter::image_detail::automatic:
    return "auto";
  default:
    throw DF0_MAKE_ERROR("Unexpected image detail value", image_detail,
                         errors::program_logic{});
  }
}

enum class user_msg_type { text, image_url };

template <typename auditor_type>
std::string prepare_query(const std::string &system_msg,
                          openrouter::user_msg_type user_msg_type,
                          const std::string &user_msg, const std::string &model,
                          float temperature, image_detail image_detail,
                          audit::auditor_ref<auditor_type> auditor) {
  audit::scope audit_scope{auditor, [&] {
                             return "system_msg"_v = system_msg,
                                    "user_msg"_v = user_msg, "model"_v = model,
                                    "temperature"_v = temperature,
                                    "image_detail"_v = image_detail;
                           }};
  boost::json::object query{
      {"model", model},
      {"messages",
       boost::json::array{
           boost::json::object{{"role", "system"}, {"content", system_msg}},
           boost::json::object{
               {"role", "user"},
               {"content", user_msg_type == openrouter::user_msg_type::text
                               ? boost::json::value(user_msg)
                               : boost::json::array{boost::json::object{
                                     {"type", "image_url"},
                                     {"image_url",
                                      boost::json::object{
                                          {"url", user_msg},
                                          {"detail", image_detail_to_string(
                                                         image_detail)}}}}}}}}},
      {"temperature", temperature}};
  return boost::json::serialize(query);
}

template <typename auditor_type>
std::string post_request(const std::string &query, const std::string &api_key,
                         audit::auditor_ref<auditor_type> auditor) {
  audit::scope audit_scope{auditor, [&] { return "query"_v = query; }};
  std::ostringstream response_stream{};
  try {
    data_source{query, mime_type{"application/json"}, confidence::highest} |
        http::post<auditor_type>(
            "https://openrouter.ai/api/v1/chat/completions", api_key,
            http::ssl_verify_peer{true}, auditor) |
        response_stream;
  } catch (const std::exception &e) {
    std::throw_with_nested(DF0_MAKE_ERROR("OpenRouter HTTP request failed", query.size()));
  }
  return response_stream.str();
}

template <typename auditor_type>
std::string parse_response(const std::string &response, bool allow_truncated,
                           audit::auditor_ref<auditor_type> auditor) {
  audit::scope audit_scope{auditor, [&] { return "response"_v = response; }};
  try {
    boost::json::value response_val = boost::json::parse(response);
    boost::json::object *response_obj = response_val.if_object();
    if (!response_obj) {
      throw DF0_MAKE_ERROR("OpenRouter response is not a JSON object");
    }

    if (const auto *error = response_obj->if_contains("error");
        error && !error->is_null()) {
      throw DF0_MAKE_ERROR("OpenRouter API returned an error",
                           describe_error(*error));
    }
    const boost::json::array *choices = nullptr;
    if (const auto *c = response_obj->if_contains("choices")) {
      choices = c->if_array();
    }
    if (!choices || choices->empty()) {
      throw DF0_MAKE_ERROR("OpenRouter response contains no choices");
    }
    const boost::json::object *choice = (*choices)[0].if_object();
    if (!choice) {
      throw DF0_MAKE_ERROR("OpenRouter response has a malformed choice");
    }
    if (const auto *error = choice->if_contains("error");
        error && !error->is_null()) {
      throw DF0_MAKE_ERROR("OpenRouter choice returned an error",
                           describe_error(*error));
    }

    if (const auto *fr = choice->if_contains("finish_reason");
        fr && fr->is_string()) {
      const std::string reason{fr->as_string()};
      if (reason == "length" && !allow_truncated)
        throw DF0_MAKE_ERROR("OpenRouter output truncated", reason);
      if (reason == "content_filter")
        throw DF0_MAKE_ERROR("OpenRouter output blocked by content filter",
                             reason);
      if (reason == "error")
        throw DF0_MAKE_ERROR("OpenRouter generation failed", reason);
      if (reason == "tool_calls")
        throw DF0_MAKE_ERROR("OpenRouter returned tool calls, which this "
                             "element does not support",
                             reason);
    }

    const boost::json::object *message = nullptr;
    if (const auto *m = choice->if_contains("message"))
      message = m->if_object();
    if (!message)
      throw DF0_MAKE_ERROR("OpenRouter choice has no message");

    const auto *content = message->if_contains("content");
    if (!content || !content->is_string())
      throw DF0_MAKE_ERROR("OpenRouter message has no text content");

    return std::string{content->as_string()};
  } catch (const std::exception &e) {
    std::throw_with_nested(DF0_MAKE_ERROR(snippet(response)));
  }
}

} // anonymous namespace

template <typename auditor_type>
continuation
task<auditor_type>::operator()(message_ptr msg,
                               const message_callbacks &emit_message) {
  audit::scope task_audit_scope{this->auditor(), [&] { return "msg"_v = msg; }};

  if (!msg->is<data_source>())
    return emit_message(std::move(msg));

  audit::entry(this->auditor(), audit::no_context);
  const data_source &data = msg->get<data_source>();
  openrouter::user_msg_type user_msg_type;
  std::string data_str;

  if (data.has_highest_confidence_mime_type_in({mime_type{"text/plain"}})) {
    audit::scope text_audit_scope{this->auditor(), audit::no_context};
    user_msg_type = openrouter::user_msg_type::text;
    data_str = data.string();
  } else if (data.has_highest_confidence_mime_type_in(
                 {mime_type{"image/jpeg"}, mime_type{"image/png"},
                  mime_type{"image/gif"}, mime_type{"image/webp"}})) {
    audit::scope image_audit_scope{this->auditor(), audit::no_context};
    user_msg_type = openrouter::user_msg_type::image_url;
    std::span<const std::byte> input_data = data.span();
    std::string base64Encoded = base64::encode(input_data);
    data_str = std::string{"data:image/*;base64,"} + base64Encoded;
  } else {
    throw DF0_MAKE_ERROR("Unsupported MIME type for OpenRouter Chat",
                         data.highest_confidence_mime_type()->v,
                         errors::program_logic{});
  }

  auto aud = this->auditor();
  std::string content =
      parse_response(
          post_request(prepare_query(impl().m_system_message, user_msg_type,
                                     data_str, impl().m_model,
                                     impl().m_temperature,
                                     impl().m_image_detail, aud),
                       impl().m_api_key, aud),
          impl().m_allow_truncated, aud) +
      '\n';
  return emit_message(
      data_source{content, mime_type{"text/plain"}, confidence::high});
}

template class DF0_OPENROUTER_EXPORT task<df0::audit::null_auditor>;
template class DF0_OPENROUTER_EXPORT task<df0::audit::json_stream_auditor>;

} // namespace openrouter
} // namespace df0

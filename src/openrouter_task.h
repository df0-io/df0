/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_OPENROUTER_TASK_H
#define DF0_OPENROUTER_TASK_H

#include "audit/auditor_ref.hpp"
#include "audit/with_auditor.hpp"
#include "openrouter_export.h"
#include "pimpl.h"
#include "pipeline/element_base.hpp"
#include <concepts>
#include <string>

namespace df0 {
namespace openrouter {

enum class image_detail { low, high, automatic };

/**
 * @brief Chat chain element backed by OpenRouter (https://openrouter.ai), a
 * single API that routes requests to hundreds of models across many providers
 * (OpenAI, Anthropic, Google, Meta, Mistral, DeepSeek, and more).
 *
 * @tparam auditor_type Injected auditor type. Defaults to the zero-overhead
 * audit::null_auditor.
 */
template <typename auditor_type = audit::null_auditor>
class DF0_OPENROUTER_EXPORT task
    : public pipeline::transformer_element<task<auditor_type>>,
      public with_pimpl<task<auditor_type>>,
      private audit::with_auditor<auditor_type> {
public:
  /**
   * @brief Constructs the component with the default stateless auditing policy.
   *
   * Available only when the configured auditor is stateless (default-constructible).
   *
   * @param system_message System prompt sent with every request.
   * @param api_key OpenRouter API key (create one at
   * https://openrouter.ai/keys). Never written to audit output.
   * @param model OpenRouter model slug, e.g. "openai/gpt-5" or
   * "anthropic/claude-sonnet-5". Defaults to "openrouter/auto", which lets
   * OpenRouter choose the model.
   * @param temperature Sampling temperature.
   * @param allow_truncated If false (default), a response cut off by the output.
   * @param image_detail Vision hint forwarded to models that support image
   * input.
   */
  template <audit::stateless_auditor stateless_auditor_type = auditor_type>
  task(const std::string &system_message, const std::string &api_key,
       const std::string &model = "openrouter/auto", float temperature = 0.5,
       image_detail image_detail = image_detail::automatic, bool allow_truncated = false)
      : task(system_message, api_key, model, temperature, image_detail,
             audit::auditor_ref<stateless_auditor_type>{}, allow_truncated) {}

  /// Constructs the component with an explicitly injected auditor.
  explicit task(const std::string &system_message, const std::string &api_key,
                const std::string &model, float temperature,
                image_detail image_detail,
                audit::auditor_ref<auditor_type> auditor, bool allow_truncated = false);

  /**
   * @brief Executes transform operation for given node data.
   * @param msg The input message to process.
   * @param emit_message Callback invoked to emit produced messages.
   */
  continuation operator()(message_ptr msg,
                          const message_callbacks &emit_message);

protected:
  using audit::with_auditor<auditor_type>::auditor;

private:
  using with_pimpl<task<auditor_type>>::impl;
};

// Deduction guide for CTAD

task(const std::string &, const std::string &, const std::string &, float,
     image_detail) -> task<audit::null_auditor>;

task(const std::string &, const std::string &, const std::string &, float,
     image_detail, bool) -> task<audit::null_auditor>;

} // namespace openrouter
} // namespace df0

#endif // DF0_OPENROUTER_TASK_H

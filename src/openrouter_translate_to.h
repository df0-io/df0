/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_OPENROUTER_TRANSLATE_TO_H
#define DF0_OPENROUTER_TRANSLATE_TO_H

#include "audit/auditor_ref.hpp"
#include "audit/scope.hpp"
#include "named.h"
#include "openrouter_task.h"
#include "serialization_enum.h" // IWYU pragma: keep
#include <string>

namespace df0 {
namespace openrouter {

template <typename auditor_type = audit::null_auditor>
class translate_to : public pipeline::transformer_element<translate_to<auditor_type>>,
                     public task<auditor_type> {
public:

  template <audit::stateless_auditor stateless_auditor_type = auditor_type>
  explicit translate_to(const std::string &language, const std::string &api_key,
              const std::string &model = "openrouter/auto", float temperature = 0,
              image_detail image_detail = image_detail::automatic)
      : translate_to(language, api_key, model, temperature, image_detail,
           audit::auditor_ref<stateless_auditor_type>{}) {}

  explicit translate_to(const std::string &language, const std::string &api_key, const std::string &model,
              float temperature, image_detail image_detail,
              audit::auditor_ref<auditor_type> auditor)
      : task<auditor_type>("Your task is to translate every message to " + language +
                                " language.",
                           api_key, model, temperature, image_detail, auditor) {

    audit::scope audit_scope{this->auditor(), [&] {
      return "language"_v = language, "model"_v = model, "temperature"_v = temperature,
             "image_detail"_v = image_detail;
    }};
  }
};

} // namespace openrouter
} // namespace df0

#endif // DF0_OPENROUTER_TRANSLATE_TO_H

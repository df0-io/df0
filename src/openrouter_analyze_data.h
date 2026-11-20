/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_OPENROUTER_ANALYZE_DATA_H
#define DF0_OPENROUTER_ANALYZE_DATA_H

#include "audit/auditor_ref.hpp"
#include "audit/scope.hpp"
#include "named.h"
#include "openrouter_task.h"
#include "serialization_enum.h" // IWYU pragma: keep
#include <string>

namespace df0 {
namespace openrouter {

template <typename auditor_type = audit::null_auditor>
class analyze_data : public pipeline::transformer_element<analyze_data<auditor_type>>,
                     public task<auditor_type> {
public:
  template <audit::stateless_auditor stateless_auditor_type = auditor_type>
  explicit analyze_data(const std::string &api_key,
                        const std::string &model = "openrouter/auto",
                        float temperature = 0,
                        image_detail image_detail = image_detail::automatic)
      : analyze_data(api_key, model, temperature, image_detail,
                     audit::auditor_ref<stateless_auditor_type>{}) {}

  explicit analyze_data(const std::string &api_key, const std::string &model,
                        float temperature, image_detail image_detail,
                        audit::auditor_ref<auditor_type> auditor)
      : task<auditor_type>(
            "Your task is analyze data in every message and create a summary "
            "highlighting the most important insights, trends, key patterns, "
            "statistics, findings and other revelant information. Include "
            "conclusions that are usually made for type of data you will find "
            "in message.",
            api_key, model, temperature, image_detail, auditor) {
    audit::scope audit_scope{this->auditor(), [&] {
      return "model"_v = model, "temperature"_v = temperature,
             "image_detail"_v = image_detail;
    }};
  }
};

} // namespace openrouter
} // namespace df0

#endif // DF0_OPENROUTER_ANALYZE_DATA_H

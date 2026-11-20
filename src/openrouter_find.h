/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_OPENROUTER_FIND_H
#define DF0_OPENROUTER_FIND_H

#include "audit/auditor_ref.hpp"
#include "audit/scope.hpp"
#include "named.h"
#include "openrouter_task.h"
#include "serialization_enum.h" // IWYU pragma: keep
#include <string>

namespace df0 {
namespace openrouter {

template <typename auditor_type = audit::null_auditor>
class find : public pipeline::transformer_element<find<auditor_type>>,
             public task<auditor_type> {
public:

  template <audit::stateless_auditor stateless_auditor_type = auditor_type>
  explicit find(const std::string &what, const std::string &api_key,
              const std::string &model = "openrouter/auto", float temperature = 0,
              image_detail image_detail = image_detail::automatic)
      : find(what, api_key, model, temperature, image_detail,
           audit::auditor_ref<stateless_auditor_type>{}) {}

  explicit find(const std::string &what, const std::string &api_key, const std::string &model,
              float temperature, image_detail image_detail,
              audit::auditor_ref<auditor_type> auditor)
      : task<auditor_type>("Your task is to find all occurrences of \"" + what +
                               "\" in the provided text or image.\n\n"
                               "1. On the first line, write only the total count of "
                               "occurrences found. If none are found, write 0.\n"
                               "2. On each subsequent line, provide a brief, one-sentence "
                               "description of where each occurrence is located. For example: "
                               "'A red car is parked near the building on the left.' or 'The "
                               "word \"processing\" appears in the first paragraph.'\n\n"
                               "Do not describe the entire image or text. Focus only on the "
                               "location of what you were asked to find.",
                           api_key, model, temperature, image_detail, auditor) {

    audit::scope audit_scope{this->auditor(), [&] {
      return "what"_v = what, "model"_v = model, "temperature"_v = temperature,
             "image_detail"_v = image_detail;
    }};
  }
};

} // namespace openrouter
} // namespace df0

#endif // DF0_OPENROUTER_FIND_H

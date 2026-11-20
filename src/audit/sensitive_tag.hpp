/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_AUDIT_SENSITIVE_TAG_H
#define DF0_AUDIT_SENSITIVE_TAG_H

#include <string_view>

namespace df0::audit
{
struct sensitive
{
    static constexpr std::string_view string() { return "sensitive"; }
};

} // namespace df0::audit

#endif

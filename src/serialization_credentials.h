/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_SERIALIZATION_CREDENTIALS_H
#define DF0_SERIALIZATION_CREDENTIALS_H

#include "credentials.h"
#include "serialization_base.h"

namespace df0::serialization
{
template <typename tag>
struct serializer<credentials::secret<tag>>
{
    value full(const credentials::secret<tag>&) const { return std::string{"*****"}; }

    value typed_summary(const credentials::secret<tag>& s) const
    {
        return decorate_with_typeid(full(s), type_name::pretty<credentials::secret<tag>>());
    }
};

} // namespace df0::serialization

#endif // DF0_SERIALIZATION_CREDENTIALS_H

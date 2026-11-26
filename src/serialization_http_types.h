/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_SERIALIZATION_HTTP_TYPES_H
#define DF0_SERIALIZATION_HTTP_TYPES_H

#include "http_types.h"
#include "serialization_base.h"

namespace df0::serialization {
template <> struct serializer<http::request> {
  value full(const http::request &q) const { return q.value(); }

  value typed_summary(const http::request &q) const {
    return decorate_with_typeid(full(q), type_name::pretty<http::request>());
  }
};

template <> struct serializer<http::response> {
  value full(const http::response &r) const { return r.value(); }

  value typed_summary(const http::response &r) const {
    return decorate_with_typeid(full(r), type_name::pretty<http::response>());
  }
};

} // namespace df0::serialization

#endif // DF0_SERIALIZATION_HTTP_TYPES_H

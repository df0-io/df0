/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_CREDENTIALS_H
#define DF0_CREDENTIALS_H

#include <string>
#include <type_traits>
#include <utility>

namespace df0::credentials {
template <typename tag> class secret {
public:
  explicit secret(std::string value) noexcept(
      std::is_nothrow_move_constructible_v<std::string>)
      : m_value(std::move(value)) {}

  const std::string &reveal() const noexcept { return m_value; }

private:
  std::string m_value;
};

struct api_key_tag {};
struct bearer_token_tag {};
struct password_tag {};

using api_key = secret<api_key_tag>;
using bearer_token = secret<bearer_token_tag>;
using password = secret<password_tag>;

} // namespace df0::credentials

#endif // DF0_CREDENTIALS_H

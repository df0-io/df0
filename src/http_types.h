/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_HTTP_TYPES_H
#define DF0_HTTP_TYPES_H

#include <string>
#include <utility>

namespace df0::http {
class request {
public:
  explicit request(std::string body) noexcept(
      std::is_nothrow_move_constructible_v<std::string>)
      : m_body(std::move(body)) {}

  const std::string &value() const noexcept { return m_body; }

private:
  std::string m_body;
};

class response {
public:
  explicit response(std::string body) noexcept(
      std::is_nothrow_move_constructible_v<std::string>)
      : m_body(std::move(body)) {}

  const std::string &value() const noexcept { return m_body; }

private:
  std::string m_body;
};

} // namespace df0::http

#endif

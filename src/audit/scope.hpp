/**************************************************************************/
/*                                                                        */
/*  DF-0: Data processing Framework with Zero-overhead                    */
/*                                                                        */
/*  Copyright (c) DF-0 Team               https://github.com/df0-io/df0   */
/*  SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-DF0-Commercial   */
/*                                                                        */
/**************************************************************************/

#ifndef DF0_AUDIT_SCOPE_H
#define DF0_AUDIT_SCOPE_H

#include "auditor_ref.hpp"
#include "scope_tags.hpp"
#include "tags.hpp"
#include "../source_location.h"

#include <type_traits>
#include <utility>

namespace df0::audit
{

template<typename auditor_type, typename context_fn_type, audit_tag... extra_tags>
class scope
{
    using stored_context_fn_type = std::decay_t<context_fn_type>;

public:
    scope(auditor_ref<auditor_type> auditor, context_fn_type&& context_fn,
          tags<extra_tags...> = tags<extra_tags...>{},
          source_location location = source_location::current())
        : m_auditor(auditor),
          m_context_fn(std::forward<context_fn_type>(context_fn)),
          m_location(location)
    {
        record(df0::audit::scope_enter{});
    }

    ~scope()
    {
        record(df0::audit::scope_exit{});
    }

    scope(const scope&) = delete;
    scope& operator=(const scope&) = delete;

private:
    template<typename phase_tag>
    void record(phase_tag phase)
    {
        m_auditor.auditor().record(m_location, phase, extra_tags{}..., m_context_fn);
    }
    auditor_ref<auditor_type> m_auditor;
    stored_context_fn_type m_context_fn;
    source_location m_location;
};

template<typename context_fn_type, audit_tag... extra_tags>
class scope<null_auditor, context_fn_type, extra_tags...>
{
public:
    constexpr scope(auditor_ref<null_auditor>, context_fn_type&&,
        tags<extra_tags...> = tags<extra_tags...>{},
        source_location = source_location::current()) noexcept {}
    ~scope() noexcept {}
};

template<typename auditor_type, typename context_fn_type>
scope(auditor_ref<auditor_type>, context_fn_type&&) -> scope<auditor_type, context_fn_type>;


template<typename auditor_type, typename context_fn_type, audit_tag... extra_tags>
scope(auditor_ref<auditor_type>, context_fn_type&&, tags<extra_tags...>)
    -> scope<auditor_type, context_fn_type, extra_tags...>;

template<auditor auditor_type, typename context_fn_type>
scope(auditor_type&, context_fn_type&&) -> scope<auditor_type, context_fn_type>;

template<auditor auditor_type, typename context_fn_type, audit_tag... extra_tags>
scope(auditor_type&, context_fn_type&&, tags<extra_tags...>)
    -> scope<auditor_type, context_fn_type, extra_tags...>;

} // namespace df0::audit

#endif

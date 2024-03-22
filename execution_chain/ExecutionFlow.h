#pragma once

#include "ExecutionChain.h"
#include <utility>

namespace chain
{

namespace detail {

template<class T, class... Args>
static inline constexpr bool returns_bool = std::is_same_v<bool, std::invoke_result_t<T, Args&...>>;

template<class Callable, class... Args>
constexpr bool Call(Callable&& callable, Args&&... args)
{
    if constexpr (returns_bool<std::decay_t<Callable>, Args...>)
    {
        return callable(args...);
    }
    else
    {
        callable(args...);
        return true;
    }
}
}

template<class If_t, class Then_t, class Else_t>
struct IfThenElse : ChainStep::LogicFlow
{
    template<class... Args>
    constexpr bool Execute(Args&... args) const
    {
        if (m_predicate(args...))
        {
            return detail::Call(m_then, args...);
        }

        return detail::Call(m_elseThen, args...);
    }

    template<class... Args>
    constexpr bool operator()(Args&... args) const
    {
        return Execute(args...);
    }

private:
    constexpr IfThenElse(If_t&& pred,
        Then_t&& then_,
        Else_t&& else_)
        : m_predicate(pred)
        , m_then(then_)
        , m_elseThen(else_)
    {}

    If_t m_predicate;
    Then_t m_then;
    Else_t m_elseThen;

    template<class I, class T>
    friend struct IfThen;
};

template<class If_t, class Then_t>
struct IfThen : ChainStep::LogicFlow
{
    template<class... Args>
    constexpr bool Execute(Args&... args) const
    {
        if (!m_predicate(args...))
        {
            return true; // continue
        }

        return detail::Call(m_then, args...);
    }

    template<class... Args>
    constexpr bool operator()(Args&... args) const
    {
        return Execute(args...);
    }

    template<class Else_t>
    constexpr auto Else(Else_t&& elseThen)
    {
        return IfThenElse<If_t, Then_t, Else_t> {
                std::forward<If_t>(m_predicate),
                std::forward<Then_t>(m_then),
                std::forward<Else_t>(elseThen)
        };
    }

private:
    constexpr IfThen(If_t&& pred,
        Then_t&& then_)
        : m_predicate(pred)
        , m_then(then_)
    {}

    If_t m_predicate;
    Then_t m_then;

    template<class I>
    friend struct If;
};

template<class If_t>
struct If
{
    constexpr If(If_t&& logic) : m_logic(logic) {}

    template<class Then_t>
    constexpr auto Then(Then_t&& then)
    {
        return IfThen<If_t, Then_t> {
            std::forward<If_t>(m_logic),
            std::forward<Then_t>(then)
        };
    }

private:
    If_t m_logic;
};

template<class Try_t, class Fallback_t>
struct TryFallback : ChainStep::LogicFlow
{
    template<class... Args>
    constexpr bool Execute(Args&... args)
    {
        return detail::Call(m_try, args...) || detail::Call(m_fallback, args...);
    }

    template<class... Args>
    constexpr bool operator()(Args&... args)
    {
        return Execute(args...);
    }

private:
    constexpr TryFallback(Try_t&& try_, Fallback_t&& fallback)
        : m_try(try_)
        , m_fallback(fallback)
    {}

    Try_t m_try;
    Fallback_t m_fallback;

    template<class T>
    friend struct Try;
};

template<class Try_t>
struct Try
{
    constexpr Try(Try_t&& try_) : m_try(try_) {}

    template<class Fallback_t>
    constexpr auto Fallback(Fallback_t&& fallback)
    {
        return TryFallback<Try_t, Fallback_t> {
            std::forward<Try_t>(m_try),
            std::forward<Fallback_t>(fallback)
        };
    }

    Try_t m_try;
};

} // namespace chain
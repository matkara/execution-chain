// see : http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0201r5.html

/*
Copyright (c) 2016 Jonathan B. Coe
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cassert>
#include <exception>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace chain
{

#ifdef __cpp_lib_constexpr_typeinfo
#define CONSTEXPR23 constexpr
#else
#define CONSTEXPR23
#endif

#if __cplusplus >= 202002L
#define CONSTEXPR20 constexpr
#else
#define CONSTEXPR20
#endif

// from c++20
template <class T>
struct remove_cvref
{
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
};

template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

namespace detail
{

////////////////////////////////////////////////////////////////////////////
// Implementation detail classes
////////////////////////////////////////////////////////////////////////////

template <class T>
struct default_copy
{
    CONSTEXPR20 T* operator()(const T& t) const
    {
        return new T(t);
    }
};

template <class T>
struct control_block
{
    constexpr control_block() = default;

    CONSTEXPR20 virtual ~control_block() = default;

    CONSTEXPR20 virtual std::unique_ptr<control_block> clone() const = 0;

    CONSTEXPR20 virtual T* ptr() = 0;

    CONSTEXPR23 virtual const std::type_info& type() const = 0;
};

template <class T, class U>
class direct_control_block : public control_block<T>
{
    static_assert(!std::is_reference_v<U>, "");
    U u_;

public:
    template <class... Ts>
    constexpr explicit direct_control_block(Ts&&... args)
        : u_(std::forward<Ts>(args)...)
    {
    }

    constexpr explicit direct_control_block(U&& data)
        : u_(std::forward<U>(data))
    {
    }

    CONSTEXPR20 ~direct_control_block() override = default;

    CONSTEXPR20 std::unique_ptr<control_block<T>> clone() const override
    {
        return std::make_unique<direct_control_block>(*this);
    }

    CONSTEXPR20 T* ptr() override
    {
        return std::addressof(u_);
    }

    CONSTEXPR20 const std::type_info& type() const override
    {
        return typeid(u_);
    }
};

template <class T, class U, class Copier = default_copy<U>, class Deleter = std::default_delete<U>>
class pointer_control_block : public control_block<T>, public Copier
{
    std::unique_ptr<U, Deleter> p_;

public:
    constexpr explicit pointer_control_block(U* pData, Copier copier = Copier{}, Deleter deleter = Deleter{})
        : Copier(std::move(copier))
        , p_(pData, std::move(deleter))
    {
    }

    constexpr explicit pointer_control_block(std::unique_ptr<U, Deleter> pData, Copier copier = Copier{})
        : Copier(std::move(copier))
        , p_(std::move(pData))
    {
    }

    CONSTEXPR20 ~pointer_control_block() override = default;

    CONSTEXPR20 std::unique_ptr<control_block<T>> clone() const override
    {
        assert(p_ && "polymorphic_value : pointer_control_block does not encapsulate data.");
        return std::make_unique<pointer_control_block>(Copier::operator()(*p_), static_cast<const Copier&>(*this),
                                                       p_.get_deleter());
    }

    CONSTEXPR20 T* ptr() override
    {
        return p_.get();
    }

    CONSTEXPR23 const std::type_info& type() const override
    {
        return p_ ? typeid(*p_) : typeid(void);
    }
};

template <class T, class U>
class delegating_control_block : public control_block<T>
{
    std::unique_ptr<control_block<U>> delegate_;

public:
    constexpr explicit delegating_control_block(std::unique_ptr<control_block<U>> b)
        : delegate_(std::move(b))
    {
    }

    CONSTEXPR20 ~delegating_control_block() override = default;

    CONSTEXPR20 std::unique_ptr<control_block<T>> clone() const override
    {
        return std::make_unique<delegating_control_block>(delegate_->clone());
    }

    CONSTEXPR20 T* ptr() override
    {
        return delegate_->ptr();
    }

    CONSTEXPR23 const std::type_info& type() const override
    {
        return delegate_ ? delegate_->type() : typeid(void);
    }
};

} // end namespace detail

class bad_polymorphic_value_construction : public std::exception
{
public:
    bad_polymorphic_value_construction() noexcept = default;

    const char* what() const noexcept override
    {
        return "Dynamic and static type mismatch in polymorphic_value "
               "construction";
    }
};

template <class T>
class polymorphic_value;

template <class T>
struct is_polymorphic_value : std::false_type
{
};

template <class T>
struct is_polymorphic_value<polymorphic_value<T>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_polymorphic_value_v = is_polymorphic_value<T>::value;

template <typename Child, typename Parent>
inline constexpr bool is_child_of_v = !std::is_same_v<Parent, Child> && std::is_convertible_v<Child*, Parent*>;

template <class T>
struct polymorphic_value_type
{
    using type = void;
};

template <class T>
struct polymorphic_value_type<polymorphic_value<T>>
{
    using type = T;
};

template <class T>
using polymorphic_value_type_t = typename polymorphic_value_type<T>::type;

template <class T, class U>
struct is_polymorphic_base : std::false_type
{
};

template <class Base, class Derived>
struct is_polymorphic_base<polymorphic_value<Base>, Derived>
{
    inline static constexpr bool value = std::is_base_of_v<Base, Derived>;
};

template <typename Base, typename Derived>
inline constexpr bool is_polymorphic_base_v = is_polymorphic_base<Base, Derived>::value;

/**
 * \brief polymorphic_value is a container offering value-like semantic to polymorphic objects dynamically allocated.
 *
 *  -# polymorphic_value<IBase> may hold an object of a class publicly derived from IBase and perform a copy of that
 * object when polymorphic_value<IBase> is copied without the need of implementing abstract copy-like methods often
 * called "clone" : \code{.cpp} polymorphic_value<IBase> pvalue = Derived(); auto copy = pvalue;
 *     EXPECT_EQ(typeid(Derived), typeid(*copy));
 * \endcode
 *  polymorphic_value relies instead on copy constructors of the derived types to perform the copy. <br>
 *
 *  -# The underlying polymorphic object is owned by polymorphic_value and shares the same lifetime. <br>
 *
 *  -# Liskov-like substitution principle should be applicable to polymorphic_value :
 *   any polymorphic_value<Derived> should be usable as if it was a polymorphic_value<IBase>
 * \code{.cpp}
 *     auto lambda = [&](const polymorphic_value<IBase>& pvalue) { };
 *     polymorphic_value<Derived> pvalue = Derived();
 *     lambda(pvalue); // OK
 * \endcode
 *
 *  -# The default polymorphic_value constructor does not assign any value. Therefore :
 *  \code{.cpp}
 *     polymorphic_value<IBase> pvalue;
 *     EXPECT_EQ(nullptr, pvalue);
 *  \endcode
 *
 *  -# polymorphic_value can be assigned a nullptr :
 *   \code{.cpp}
 *     polymorphic_value<IBase> pvalue = Derived();
 *     pvalue = nullptr; // OK
 *     EXPECT_EQ(nullptr, pvalue);
 *  \endcode
 *
 *  -# polymorphic_value can be constructed from a rvalue pointer :
 *  \code{.cpp}
 *    polymorphic_value<IBase> pvalue = polymorphic_value<IBase>(new Derived()); // OK
 *  \endcode
 *
 *  -# polymorphic_value CANNOT be assigned a lvalue pointer (undetermined ownership) :
 *  \code{.cpp}
 *    // pDerived is a lvalue pointer
 *    polymorphic_value<IBase> pvalue = pDerived; // NOK, does not compile
 *  \endcode
 *
 *  -# polymorphic_value can be constructed from a rvalue unique_ptr :
 *  \code{.cpp}
 *    auto pDerived = std::make_unique<Derived>();
 *    polymorphic_value<IBase> pvalue = std::move(pDerived); // OK
 *  \endcode
 *
 *  -# when polymorphic_value is used for comparison, a valid comparison operator must exist :
 *  \code{.cpp}
 *    polymorphic_value<IBase> p1 = new Derived();
 *    polymorphic_value<IBase2> p2 = new OtherDerived();
 *    auto equals = (p1 == p2); // compiles only if an equality operator between IBase and IBase2 objects exists
 *  \endcode
 *
 */
template <class T>
class polymorphic_value
{
    static_assert(!std::is_union<T>::value, "");
    static_assert(std::is_class<T>::value, "");

    template <class U>
    friend class polymorphic_value;

    template <class T_, class U, class... Ts>
    friend CONSTEXPR23 polymorphic_value<T_> make_polymorphic_value(Ts&&... ts);
    template <class T_, class... Ts>
    friend CONSTEXPR23 polymorphic_value<T_> make_polymorphic_value(Ts&&... ts);

    std::unique_ptr<detail::control_block<T>> cb_;

public:
    //
    // Destructor
    //

    CONSTEXPR20 ~polymorphic_value() = default;

    //
    // Constructors
    //

    constexpr polymorphic_value() noexcept
    {
    }

    constexpr polymorphic_value(std::nullptr_t) noexcept
    {
    }

    template <class U, class C = detail::default_copy<U>, class D = std::default_delete<U>,
              class V = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    CONSTEXPR23 explicit polymorphic_value(U*&& u, C copier = C{}, D deleter = D{})
    {
        if (u == nullptr)
        {
            return;
        }

        if (std::is_same<D, std::default_delete<U>>::value && std::is_same<C, detail::default_copy<U>>::value &&
            typeid(*u) != typeid(U))
        {
            throw bad_polymorphic_value_construction();
        }

        std::unique_ptr<U, D> p(u, std::move(deleter));

        cb_ = std::make_unique<detail::pointer_control_block<T, U, C, D>>(std::move(p), std::move(copier));
    }

    template <class U, class C = detail::default_copy<U>, class D = std::default_delete<U>,
              class V = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    CONSTEXPR23 polymorphic_value(std::unique_ptr<U, D> u, C copier = C{})
    {
        if (!u)
        {
            return;
        }

        if (std::is_same_v<D, std::default_delete<U>> && std::is_same_v<C, detail::default_copy<U>> &&
            typeid(*u) != typeid(U))
        {
            throw bad_polymorphic_value_construction();
        }

        cb_ = std::make_unique<detail::pointer_control_block<T, U, C, D>>(std::move(u), std::move(copier));
    }

    template <class U, class V = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    CONSTEXPR23 polymorphic_value(U&& u)
    {
        if (typeid(u) != typeid(U))
        {
            throw bad_polymorphic_value_construction();
        }

        cb_ = std::make_unique<detail::direct_control_block<T, remove_cvref_t<U>>>(std::forward<U>(u));
    }

    template <class U, class V = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    CONSTEXPR23 polymorphic_value(const U& u)
    {
        if (typeid(u) != typeid(U))
        {
            throw bad_polymorphic_value_construction();
        }

        cb_ = std::make_unique<detail::direct_control_block<T, U>>(u);
    }

    //
    // Copy-constructors
    //

    CONSTEXPR23 polymorphic_value(const polymorphic_value& p)
        : cb_(p ? p.cb_->clone() : nullptr)
    {
    }

    //
    // Move-constructors
    //

    CONSTEXPR23 polymorphic_value(polymorphic_value&& p) noexcept = default;

    //
    // Converting constructors
    //

    template <class U, std::enable_if_t<is_child_of_v<U, T>, bool> = true>
    explicit CONSTEXPR23 polymorphic_value(const polymorphic_value<U>& p)
        : cb_(std::make_unique<detail::delegating_control_block<T, U>>(polymorphic_value<U>(p).cb_))
    {
    }

    template <class U, std::enable_if_t<is_child_of_v<U, T>, bool> = true>
    explicit CONSTEXPR23 polymorphic_value(polymorphic_value<U>&& p)
        : cb_(std::make_unique<detail::delegating_control_block<T, U>>(std::move(p.cb_)))
    {
    }

    //
    // In-place constructor
    //

    template <class U,
              class V = std::enable_if_t<std::is_convertible<std::decay_t<U>*, T*>::value &&
                                         !is_polymorphic_value<std::decay_t<U>>::value>,
              class... Ts>
    CONSTEXPR23 explicit polymorphic_value(std::in_place_type_t<U>, Ts&&... ts)
        : cb_(std::make_unique<detail::direct_control_block<T, remove_cvref_t<U>>>(std::forward<Ts>(ts)...))
    {
    }

    //
    // Assignment
    //

    CONSTEXPR23 polymorphic_value& operator=(const polymorphic_value& p)
    {
        if (std::addressof(p) == this)
        {
            return *this;
        }

        if (!p)
        {
            cb_.reset();
            return *this;
        }

        cb_ = p.cb_->clone();
        return *this;
    }

    template <typename U, std::enable_if_t<is_child_of_v<U, T>, bool> = true>
    CONSTEXPR23 polymorphic_value<T>& operator=(const polymorphic_value<U>& p)
    {
        if (!p)
        {
            cb_.reset();
            return *this;
        }

        cb_ = std::make_unique<detail::delegating_control_block<T, U>>(p.cb_->clone());
        return *this;
    }

    template <typename U, std::enable_if_t<is_child_of_v<U, T>, bool> = true>
    CONSTEXPR23 polymorphic_value<T>& operator=(polymorphic_value<U>&& p)
    {
        if (!p)
        {
            cb_.reset();
            return *this;
        }

        cb_ = std::make_unique<detail::delegating_control_block<T, U>>(std::move(p.cb_));
        return *this;
    }

    CONSTEXPR23 polymorphic_value<T>& operator=(std::nullptr_t)
    {
        cb_.reset();
        return *this;
    }

    template <
        typename U,
        std::enable_if_t<!is_polymorphic_value_v<U> && !std::is_pointer_v<U> && !std::is_same_v<T, remove_cvref_t<U>> &&
                             (std::is_base_of_v<T, remove_cvref_t<U>> || std::is_convertible_v<remove_cvref_t<U>, T>),
                         bool> = true>
    CONSTEXPR23 polymorphic_value& operator=(U&& p)
    {
        *this = polymorphic_value<T>(std::in_place_type<U>, std::forward<U>(p));
        return *this;
    }

    template <typename U, std::enable_if_t<!is_polymorphic_value_v<U> && !std::is_pointer_v<U> &&
                                               std::is_same_v<T, remove_cvref_t<U>>,
                                           bool> = true>
    CONSTEXPR23 polymorphic_value& operator=(U&& p)
    {
        *this = make_polymorphic_value<T>(std::forward<U>(p));
        return *this;
    }

    template <class U, class V = std::enable_if_t<std::is_convertible<U*, T*>::value>>
    CONSTEXPR23 polymorphic_value& operator=(U*&& rhs)
    {
        *this = polymorphic_value<T>(std::forward<U*>(rhs));
        return *this;
    }

    //
    // Move-assignment
    //

    CONSTEXPR23 polymorphic_value& operator=(polymorphic_value&& p) noexcept = default;

    //
    // Modifiers
    //

    CONSTEXPR23 void swap(polymorphic_value& p) noexcept
    {
        std::swap(cb_, p.cb_);
    }

    //
    // Observers
    //

    CONSTEXPR23 explicit operator bool() const
    {
        return bool(cb_);
    }

    CONSTEXPR23 const T& value() const&
    {
        return *get();
    }

    CONSTEXPR23 T& value() &
    {
        return *get();
    }

    CONSTEXPR23 const T&& value() const&&
    {
        return *get();
    }

    CONSTEXPR23 T&& value() &&
    {
        return *get();
    }

    template <class U>
    CONSTEXPR23 const T& value_or(U&& default_value) const&
    {
        if (get() == nullptr)
        {
            return std::forward<U>(default_value);
        }
        return *get();
    }

    template <class U>
    CONSTEXPR23 T&& value_or(U&& default_value) &&
    {
        if (get() == nullptr)
        {
            return std::forward<U>(default_value);
        }
        return *get();
    }

    CONSTEXPR23 T* get() const& noexcept
    {
        return cb_ ? cb_->ptr() : nullptr;
    }

    CONSTEXPR23 T* get() & noexcept
    {
        return cb_ ? cb_->ptr() : nullptr;
    }

    template <typename U, std::enable_if_t<!std::is_pointer_v<U>, bool> = true>
    CONSTEXPR23 U* get_as() const& noexcept
    {
        return dynamic_cast<U*>(get());
    }

    CONSTEXPR23 const T* operator->() const
    {
        assert(get() != nullptr && "Accessing nullptr in polymorphic_value");
        return get();
    }

    CONSTEXPR23 const T& operator*() const
    {
        return *get();
    }

    CONSTEXPR23 T* operator->()
    {
        assert(get() != nullptr && "Accessing nullptr in polymorphic_value");
        return get();
    }

    CONSTEXPR23 T& operator*()
    {
        return *get();
    }

    CONSTEXPR23 const std::type_info& type() const
    {
        return cb_ ? cb_->type() : typeid(void);
    }

    template <typename U, std::enable_if_t<is_child_of_v<T, U>, bool> = true>
    operator polymorphic_value<U>&()
    {
        return reinterpret_cast<polymorphic_value<U>&>(*this);
    }

    template <typename U, std::enable_if_t<is_child_of_v<T, U>, bool> = true>
    operator const polymorphic_value<U>&() const
    {
        return reinterpret_cast<const polymorphic_value<U>&>(*this);
    }

    template <typename U, std::enable_if_t<!std::is_pointer_v<U>, bool> = true>
    CONSTEXPR23 bool is() const
    {
        return get_as<U>() != nullptr;
    }
};

//
// polymorphic_value creation
//
template <class T, class... Ts>
CONSTEXPR23 polymorphic_value<T> make_polymorphic_value(Ts&&... ts)
{
    polymorphic_value<T> p;
    p.cb_ = std::make_unique<detail::direct_control_block<T, T>>(std::forward<Ts>(ts)...);
    return p;
}
template <class T, class U, class... Ts>
CONSTEXPR23 polymorphic_value<T> make_polymorphic_value(Ts&&... ts)
{
    polymorphic_value<T> p;
    p.cb_ = std::make_unique<detail::direct_control_block<T, U>>(std::forward<Ts>(ts)...);
    return p;
}

template <typename... Args>
constexpr bool all(Args... args)
{
    return (... && args);
}

template <typename... Args>
constexpr bool any(Args... args)
{
    return (... || args);
}

template <typename... Args>
struct all_polymorphic_value
{
    inline static constexpr bool value = all(is_polymorphic_value_v<remove_cvref_t<Args>>...);
};

template <typename... T>
inline constexpr bool all_polymorphic_value_v = all_polymorphic_value<T...>::value;

template <typename... Args>
struct any_polymorphic_value
{
    inline static constexpr bool value = any(is_polymorphic_value_v<remove_cvref_t<Args>>...);
};

template <typename... T>
inline constexpr bool any_polymorphic_value_v = any_polymorphic_value<T...>::value;

//
// non-member swap
//
template <class T>
CONSTEXPR23 void swap(polymorphic_value<T>& t, polymorphic_value<T>& u) noexcept
{
    t.swap(u);
}

namespace polymorphic_value_detail
{

template <typename T>
constexpr bool is_valid(const T&)
{
    return true;
}

inline constexpr bool is_valid(std::nullptr_t)
{
    return false;
}

template <typename T>
CONSTEXPR23 bool is_valid(const polymorphic_value<T>& pvalue)
{
    return bool(pvalue);
}

template <
    typename T,
    std::enable_if_t<!is_polymorphic_value_v<remove_cvref_t<T>> && !std::is_pointer_v<remove_cvref_t<T>>, bool> = true>
constexpr const T& value(const T& v)
{
    return v;
}

template <
    typename T,
    std::enable_if_t<!is_polymorphic_value_v<remove_cvref_t<T>> && !std::is_pointer_v<remove_cvref_t<T>>, bool> = true>
constexpr T& value(T& v)
{
    return v;
}

template <typename T, std::enable_if_t<is_polymorphic_value_v<remove_cvref_t<T>>, bool> = true>
CONSTEXPR23 auto& value(T&& v)
{
    return v.value();
}

template <typename T, std::enable_if_t<!is_polymorphic_value_v<remove_cvref_t<T>>, bool> = true>
constexpr const T& data(const T& v)
{
    return v;
}

template <typename T, std::enable_if_t<!is_polymorphic_value_v<remove_cvref_t<T>>, bool> = true>
constexpr T& data(T& v)
{
    return v;
}

template <typename T, std::enable_if_t<is_polymorphic_value_v<remove_cvref_t<T>>, bool> = true>
CONSTEXPR23 auto data(T&& v)
{
    return v.get();
}

} // namespace polymorphic_value_detail

template <class L, class R, std::enable_if_t<any_polymorphic_value_v<L, R>, bool> = true>
CONSTEXPR23 bool operator==(L&& lhs, R&& rhs)
{
    using namespace polymorphic_value_detail;

    if (!is_valid(std::forward<L>(lhs)) && !is_valid(std::forward<R>(rhs)))
    {
        return true;
    }

    if constexpr (std::is_null_pointer_v<std::decay_t<L>> || std::is_null_pointer_v<std::decay_t<R>>)
    {
        return false;
    }
    else
    {
        return (is_valid(std::forward<L>(lhs)) && is_valid(std::forward<R>(rhs))) ? value(lhs) == value(rhs) : false;
    }
}

template <class L, class R, std::enable_if_t<any_polymorphic_value_v<L, R>, bool> = true>
CONSTEXPR23 bool operator!=(L&& lhs, R&& rhs)
{
    return !(lhs == rhs);
}

} // namespace chain

#pragma once
#include <type_traits>
namespace Oobe
{
    /// This section of nullptr checking template type is humbly borrowed from GSL <https://github.com/microsoft/GSL>
    // Copyright (c) 2015 Microsoft Corporation. All rights reserved. Licensed under the MIT License (MIT).
    template <typename T, typename = void> struct is_comparable_to_nullptr : std::false_type
    { };

    template <typename T>
    struct is_comparable_to_nullptr<
      T, std::enable_if_t<std::is_convertible<decltype(std::declval<T>() != nullptr), bool>::value>> : std::true_type
    { };

    //
    // not_null
    //
    // Restricts a pointer or smart pointer to only hold non-null values.
    //
    // Has zero size overhead over T.
    //
    // If T is a pointer (i.e. T == U*) then
    // - allow construction from U*
    // - disallow construction from nullptr_t
    // - disallow default construction
    // - ensure construction from null U* fails
    // - allow implicit conversion to U*
    //
    template <class T> class not_null
    {
      public:
        static_assert(is_comparable_to_nullptr<T>::value, "T cannot be compared to nullptr.");

        template <typename U, typename = std::enable_if_t<std::is_convertible<U, T>::value>>
        constexpr not_null(U&& u) : ptr_(std::forward<U>(u))
        {
            assert(ptr_ != nullptr);
        }

        template <typename = std::enable_if_t<!std::is_same<std::nullptr_t, T>::value>>
        constexpr not_null(T u) : ptr_(std::move(u))
        {
            assert(ptr_ != nullptr);
        }

        template <typename U, typename = std::enable_if_t<std::is_convertible<U, T>::value>>
        constexpr not_null(const not_null<U>& other) : not_null(other.get())
        { }

        not_null(const not_null& other) = default;
        not_null& operator=(const not_null& other) = default;
        constexpr std::conditional_t<std::is_copy_constructible<T>::value, T, const T&> get() const
        {
            assert(ptr_ != nullptr);
            return ptr_;
        }

        constexpr operator T() const
        {
            return get();
        }
        constexpr decltype(auto) operator->() const
        {
            return get();
        }
        constexpr decltype(auto) operator*() const
        {
            return *get();
        }

        // prevents compilation when someone attempts to assign a null pointer constant
        not_null(std::nullptr_t) = delete;
        not_null& operator=(std::nullptr_t) = delete;

        // unwanted operators...pointers only point to single objects!
        not_null& operator++() = delete;
        not_null& operator--() = delete;
        not_null operator++(int) = delete;
        not_null operator--(int) = delete;
        not_null& operator+=(std::ptrdiff_t) = delete;
        not_null& operator-=(std::ptrdiff_t) = delete;
        void operator[](std::ptrdiff_t) const = delete;

      private:
        T ptr_;
    };

    template <class T> auto make_not_null(T&& t) noexcept
    {
        return not_null<std::remove_cv_t<std::remove_reference_t<T>>>{std::forward<T>(t)};
    }
    // end of not_null
}

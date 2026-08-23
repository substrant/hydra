#pragma once

#include <cstdint>
#include <concepts>
#include <compare>

namespace hy {
    struct ptr;

    /* Hydra address concepts for type safety and implicit conversions */

    namespace dtl {
        template <class T>
        concept IntegralPtr =
            (std::integral<std::remove_cvref_t<T>> && !std::same_as<std::remove_cvref_t<T>, bool>) ||
            std::convertible_to<std::remove_cvref_t<T>, std::uintptr_t>;

        template <class T>
        concept PrimitivePtr =
            std::same_as<std::remove_cvref_t<T>, std::nullptr_t> ||
            std::same_as<std::remove_cvref_t<T>, std::uintptr_t> ||
            std::is_pointer_v<std::remove_cvref_t<T>>;

        template <class T>
        concept HydraPtr = std::same_as<std::remove_cvref_t<T>, ptr>;

        template <class T>
        concept PrimitiveOrHydraPtr = PrimitivePtr<T> || HydraPtr<T>;

        /// Anything ptr should transparently interoperate with:
        /// pointers, nullptr, ptr itself, and any integral type.
        template <class T>
        concept HydraPtrOperand = PrimitiveOrHydraPtr<T> || IntegralPtr<T>;
    }

#   pragma pack(push, 1) // 'ptr' must be 8 bytes and non-polymorphic

    /// Represents an agnostic address or pointer (including integral) type.
    /// Used to simplify Hydra's API.
    struct ptr {
        union {
            // Represents an integer pointer
            std::uintptr_t i;

            // Represents a byte pointer
            std::uint8_t* p;

            // Represents a pointer difference
            std::ptrdiff_t d;

            // Represents a pointer to anything
            void* u;
        };

        /// Default constructor for ptr.
        /// Initializes to nullptr.
        constexpr ptr() : p(nullptr) {}

        /// Normalize various types to uintptr_t via the union, not reinterpret_cast.
        /// Converts address-like or integral types to uintptr_t.
        template <class T>
        static constexpr std::uintptr_t normalize(const T& value) {
            using U = std::remove_cvref_t<T>;
            if      constexpr (std::same_as<U, ptr>)             return value.i;
            else if constexpr (std::same_as<U, std::uintptr_t>)  return value;
            else if constexpr (std::same_as<U, std::nullptr_t>)  return 0;
            else if constexpr (std::same_as<U, bool>)            return static_cast<std::uintptr_t>(value);
            else if constexpr (std::is_pointer_v<U>) {
                union { std::uintptr_t as_int; U as_ptr; } cvt;
                cvt.as_ptr = value;
                return cvt.as_int;
            }
            else if constexpr (dtl::IntegralPtr<T>)           return static_cast<std::uintptr_t>(value);
            else                                                  static_assert([] { return false; }(), "Unsupported source type for address arithmetic");
            return 0;
        }

        /// Convert uintptr_t to various types via the union.
        /// Converts integer address to ptr, pointer, or integer type.
        template <class Rt>
        static constexpr Rt convert(std::uintptr_t value) {
            using U = std::remove_cvref_t<Rt>;
            if      constexpr (std::same_as<U, ptr>)             return ptr(value);
            else if constexpr (std::same_as<U, std::uintptr_t>)  return value;
            else if constexpr (std::same_as<U, std::nullptr_t>)  return nullptr;
            else if constexpr (std::same_as<U, bool>)            return value != 0;
            else if constexpr (std::is_pointer_v<U>) {
                union { std::uintptr_t as_int; Rt as_ptr; } cvt;
                cvt.as_int = value;
                return cvt.as_ptr;
            }
            else if constexpr (dtl::IntegralPtr<Rt>)          return static_cast<Rt>(value);
            else                                                  static_assert([] { return false; }(), "Unsupported return type for address arithmetic");
            return Rt{};
        }

        /// Construct ptr from anything address-like - pointer, ptr, nullptr,
        /// or any integral type (int, long, size_t, etc.), not just uintptr_t.
        template <dtl::HydraPtrOperand T>
        constexpr ptr(T value) noexcept /* NOLINT: Implicit construction expected */ : i(normalize(value)) {}

        /// Implicit conversion to address-like or integral type.
        /// Converts ptr to pointer, integer, or ptr type.
        template <dtl::HydraPtrOperand T>
            requires (!std::same_as<std::nullptr_t, std::remove_cvref_t<T>> &&
        !std::same_as<bool, std::remove_cvref_t<T>>)
            constexpr operator T() const noexcept /* NOLINT: Implicit conversion expected */ { return convert<T>(i); }

        /// Implicit conversion to boolean.
        /// Returns true if address is truthy.
        constexpr operator bool() const noexcept /* NOLINT: Implicit conversion expected */ { return i != 0; }

        /// Pre-increment operator for ptr.
        /// Increments address by one.
        constexpr ptr& operator++() noexcept { ++i; return *this; }

        /// Post-increment operator for ptr.
        /// Increments address by one, returns previous value.
        constexpr ptr operator++(int) noexcept { const ptr tmp = *this; ++i; return tmp; }

        /// Pre-decrement operator for ptr.
        /// Decrements address by one.
        constexpr ptr& operator--() noexcept { --i; return *this; }

        /// Post-decrement operator for ptr.
        /// Decrements address by one, returns previous value.
        constexpr ptr operator--(int) noexcept { const ptr tmp = *this; --i; return tmp; }

        /// Add position to ptr.
        /// Increases address by position - ptr, pointer, or any integral type.
        template <dtl::HydraPtrOperand T>
        constexpr ptr& operator+=(const T& off) noexcept { i += normalize(off); return *this; }

        /// Subtract position from ptr.
        /// Decreases address by position - ptr, pointer, or any integral type.
        template <dtl::HydraPtrOperand T>
        constexpr ptr& operator-=(const T& off) noexcept { i -= normalize(off); return *this; }

        /// Addition operator for ptr arithmetic.
        /// Handles ptr + ptr, ptr + integral, integral + ptr, ptr + pointer, pointer + ptr
        template <dtl::HydraPtrOperand Lhs, dtl::HydraPtrOperand T>
            requires (dtl::HydraPtr<Lhs> || dtl::HydraPtr<T>)
        friend constexpr ptr operator+(const Lhs& lhs, const T& rhs) {
            ptr result;
            result.i = normalize(lhs) + normalize(rhs);
            return result;
        }

        /// Subtraction operator for ptr arithmetic.
        /// Handles ptr - ptr, ptr - integral, integral - ptr, ptr - pointer, pointer - ptr
        template <dtl::HydraPtrOperand Lhs, dtl::HydraPtrOperand T>
            requires (dtl::HydraPtr<Lhs> || dtl::HydraPtr<T>)
        friend constexpr ptr operator-(const Lhs& lhs, const T& rhs) {
            ptr result;
            result.i = normalize(lhs) - normalize(rhs);
            return result;
        }

        /// Multiplication operator for ptr arithmetic.
        /// Handles ptr * ptr, ptr * integral, integral * ptr, ptr * pointer, pointer * ptr
        template <dtl::HydraPtrOperand Lhs, dtl::HydraPtrOperand T>
            requires (dtl::HydraPtr<Lhs> || dtl::HydraPtr<T>)
        friend constexpr ptr operator*(const Lhs& lhs, const T& rhs) {
            ptr result;
            result.i = normalize(lhs) * normalize(rhs);
            return result;
        }

        /// Division operator for ptr arithmetic.
        /// Handles ptr / ptr, ptr / integral, integral / ptr, ptr / pointer, pointer / ptr
        template <dtl::HydraPtrOperand Lhs, dtl::HydraPtrOperand T>
            requires (dtl::HydraPtr<Lhs> || dtl::HydraPtr<T>)
        friend constexpr ptr operator/(const Lhs& lhs, const T& rhs) {
            ptr result;
            result.i = normalize(lhs) / normalize(rhs);
            return result;
        }

        /// Three-way comparison operator for ptr.
        /// Compares two addresses or address-like values.
        template <dtl::HydraPtrOperand Lhs, dtl::HydraPtrOperand T>
            requires (dtl::HydraPtr<Lhs> || dtl::HydraPtr<T>)
        friend constexpr std::strong_ordering operator<=>(const Lhs& lhs, const T& rhs) {
            const auto li = normalize(lhs);
            const auto ri = normalize(rhs);

            if (li == ri) return std::strong_ordering::equal;
            return (li < ri) ? std::strong_ordering::less : std::strong_ordering::greater;
        }

        /// Equality comparison operator for ptr.
        /// Returns true if addresses are equal.
        template <dtl::HydraPtrOperand Lhs, dtl::HydraPtrOperand T>
            requires (dtl::HydraPtr<Lhs> || dtl::HydraPtr<T>)
        friend constexpr bool operator==(const Lhs& lhs, const T& rhs) {
            return normalize(lhs) == normalize(rhs);
        }
    };

#   pragma pack(pop)
}
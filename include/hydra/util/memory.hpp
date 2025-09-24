#pragma once

#include <phnt_windows.h>

#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>
#include <stdexcept>
#include <utility>
#include <concepts>
#include <type_traits>

#include "hydra/detail/generator.hpp"
#include "hydra/detail/noncopyable.hpp"

namespace hydra::mem {
    struct addr;

    template <class T>
    concept PointerType = std::is_pointer_v<std::remove_cvref_t<T>> || std::same_as<std::remove_cvref_t<T>, std::uintptr_t>;

    template <class T>
    concept HydraAddress = std::same_as<std::remove_cvref_t<T>, addr>;

    template <class T>
    concept AddressLike =
        std::same_as<std::remove_cvref_t<T>, addr> ||
        std::same_as<std::remove_cvref_t<T>, std::uintptr_t> ||
        std::is_pointer_v<std::remove_cvref_t<T>>;

    template <class T>
    concept AddressArithmeticOperand = AddressLike<T> || std::is_integral_v<T>;

    // This class gives you automatic casting magic shit
    // Very useful for exploit development where you reinterpret addresses almost constantly
#   pragma pack(push, 1)
    struct addr {
        union {
            // Represents an integer pointer
            std::uintptr_t i;

            // Represents a byte pointer
            std::uint8_t* p;

            // Represents a pointer difference
            std::ptrdiff_t d;
        };

        // Implicit constructors
        constexpr addr() : p(nullptr) { }
        constexpr addr(void* ptr) : p(static_cast<std::uint8_t*>(ptr)) { }
        constexpr addr(const std::uintptr_t addr) : i(addr) { }
        constexpr addr(std::nullptr_t) : p(nullptr) { }
        constexpr addr(std::uint8_t* ptr) : p(ptr) { }

        // Mutating arithmetic operators
        
        template <class T = addr> requires AddressLike<T>
        T& operator++() { i++; return static_cast<T&>(*this); }

        template <class T = addr> requires AddressLike<T>
        T operator++(int) { const addr tmp = *this; ++(*this); return tmp; }

        template <class T = addr> requires AddressLike<T>
        T& operator--() { i--; return static_cast<T&>(*this); }

        template <class T = addr> requires AddressLike<T>
        T operator--(int) { const addr tmp = *this; --(*this); return tmp; }

        addr& operator+=(const std::size_t off) { i += off; return *this; }
        addr& operator-=(const std::size_t off) { i -= off; return *this; }

        template <class T>
        static constexpr std::uintptr_t normalize(const T& value) {
            if      constexpr (std::same_as<std::remove_cvref_t<T>, addr>)               return value.i;
            else if constexpr (std::same_as<std::remove_cvref_t<T>, std::uintptr_t>)     return value;
            else if constexpr (std::is_pointer_v<std::remove_cvref_t<T>>)                return reinterpret_cast<std::uintptr_t>(value);
            else if constexpr (std::is_integral_v<std::remove_cvref_t<T>>)               return static_cast<std::uintptr_t>(value);
            else                                                                         static_assert([]{ return false; }(), "Unsupported source type for address arithmetic");
        }

        template <class Rt>
        static constexpr Rt convert(std::uintptr_t value) {
            if      constexpr (std::same_as<std::remove_cvref_t<Rt>, addr>)               return addr(value);
            else if constexpr (std::same_as<std::remove_cvref_t<Rt>, std::uintptr_t>)     return value;
            else if constexpr (std::is_pointer_v<std::remove_cvref_t<Rt>>)                return reinterpret_cast<Rt>(value);
            else                                                                          static_assert([]{ return false; }(), "Unsupported return type for address arithmetic");
        }

        // Automatically implicitly cast to any type but default integer type
        template <class T> requires AddressLike<T>
        constexpr operator T() const {
            return convert<T>(i);
        }

        // Arithmetic operators
        template <class T> requires AddressArithmeticOperand<T>
        friend constexpr addr operator+(addr a, const T& rhs) {
            a.i += normalize(rhs);
            return a;
        }

        // Subtraction: addr - something
        template <class T> requires AddressArithmeticOperand<T>
        friend constexpr addr operator-(addr a, const T& rhs) {
            a.i -= normalize(rhs);
            return a;
        }

        // Comparisons
        template <class T, class Lhs> requires AddressArithmeticOperand<T> && HydraAddress<Lhs>
        friend constexpr std::strong_ordering operator<=>(const Lhs& lhs, const T& rhs) {
            const auto li = lhs.i;
            const auto ri = normalize(rhs);

            if (li == ri) return std::strong_ordering::equal;
            return (li < ri) ? std::strong_ordering::less : std::strong_ordering::greater;
        }

        template <class T> requires AddressArithmeticOperand<T>
        friend constexpr bool operator==(const addr& lhs, const T rhs) {
            return lhs.i == normalize(rhs);
        }

        bool operator!() const { return p == nullptr; }
    };
#   pragma pack(pop)


    namespace detail {
        inline bool match_aob(const addr base, const std::uint8_t* pattern, const char* mask, const std::size_t size) {
            for (std::size_t off = 0; off < size; off++) {
                if (mask[off] != '?' && pattern[off] != *static_cast<std::uint8_t*>(base + off))
                    return false;
            }
            return true;
        }

        inline hydra::detail::generator<addr> scan_aob(const addr start_addr, const addr end_addr, const std::uint8_t* pattern, const char* mask, const std::size_t size) {
            for (auto addr = start_addr; addr < end_addr; ++addr) {
                if (match_aob(addr, pattern, mask, size))
                    co_yield addr;
            }
        }
    }
    class buffer {
        std::uint8_t* m_base = nullptr;
        std::size_t m_size = 0;
        bool m_owner;

        explicit buffer(std::uint8_t* data, const std::size_t size)
            : m_base(data), m_size(size), m_owner(false) { }

        explicit buffer(const std::size_t size)
            : m_size(size), m_owner(true) { alloc(size); }

        void alloc(const std::size_t size) {
            free(); // Free old data if needed

            m_base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            if (!m_base) throw std::runtime_error("VirtualAlloc failed");

            m_size = size;
            m_owner = true;
        }

        void free() {
            if (!m_base) return;
            if (m_owner) VirtualFree(m_base, 0, MEM_RELEASE);
            m_base = nullptr;
            m_size = 0;
        }

    public:
        explicit buffer() : m_owner(false) { }

        template <std::size_t Size, class T>
        static constexpr buffer from_array(T (&data)[Size]) {
            return buffer{data, Size};
        }

        template <class T>
        static constexpr buffer from_ref(T* ref) {
            if constexpr (std::is_same_v<T, std::uint8_t>) return buffer{ref, sizeof(T)};
            return buffer{reinterpret_cast<std::uint8_t*>(ref), sizeof(T)};
        }

        static buffer from_base(const addr addr, const std::size_t size) {
            return buffer{addr.p, size};
        }

        static buffer from_bounds(const addr start, const addr end) {
            return buffer{start.p, end.i - start.i};
        }

        static buffer create(const std::size_t size) {
            return buffer{size};
        }

        addr rebase(const addr virtual_addr, const addr offset = 0ull) const {
            return (virtual_addr - m_base) + offset;
        }

        buffer(const buffer& other) : m_base(other.m_base), m_size(other.m_size), m_owner(false) { }

        buffer& operator=(const buffer& other) {
            if (this != &other) {
                free();
                m_base  = other.m_base;
                m_size  = other.m_size;
                m_owner = false;
            }
            return *this;
        }

        buffer(buffer&& other) noexcept :
            m_base(std::exchange(other.m_base, nullptr)),
            m_size(std::exchange(other.m_size, 0)),
            m_owner(std::exchange(other.m_owner, false)) { }

        buffer& operator=(buffer&& other) noexcept {
            if (this != &other) {
                free();
                m_base  = std::exchange(other.m_base, nullptr);
                m_size  = std::exchange(other.m_size, 0);
                m_owner = std::exchange(other.m_owner, false);
            }
            return *this;
        }

        ~buffer() {
            free();
        }

        addr start() const { return m_base; }

        addr end() const { return m_base + m_size; }

        bool contains(const addr addr) const { return addr <= end(); }

        hydra::detail::generator<addr> scan_aob(const std::uint8_t* pattern, const char* mask, std::size_t size = 0) const {
            if (!size) size = strlen(mask);
            return detail::scan_aob(start(), end(), pattern, mask, size);
        }

        template <int Size>
        constexpr hydra::detail::generator<addr> scan_aob(const std::uint8_t* pattern, const char (&mask)[Size]) const {
            return detail::scan_aob(start(), end(), pattern, mask, Size);
        }

        void resize(const std::size_t new_size) {
            if (new_size <= m_size) return; // No shrinking for now

            buffer temp(new_size);
            if (m_base) std::memcpy(temp.m_base, m_base, m_size);

            *this = std::move(temp);
        }

        addr data() const { return m_base; }
        std::size_t size() const { return m_size; }

        void set_bounds(const addr start, const addr end) {
            if (end <= m_base) throw std::runtime_error("End is before start of buffer");
            m_base = start;
            m_size = end - m_base;
        }

        std::uint8_t& operator[](const std::size_t index) { return m_base[index]; }
        const std::uint8_t& operator[](const std::size_t index) const { return m_base[index]; }

        operator bool() const { return m_base != nullptr; }
        operator void*() const { return m_base; }
        operator std::uintptr_t() const { return reinterpret_cast<std::uintptr_t>(m_base); }

        std::uintptr_t offset(const addr addr) const { return addr.i - reinterpret_cast<std::uintptr_t>(m_base); }

        // Shift buffer to the right
        template <class T> requires AddressArithmeticOperand<T>
        friend constexpr buffer operator+(buffer buf, const T off) {
            const auto normalized = addr::normalize(off);

            buf.m_owner = false;
            buf.m_base += normalized;
            buf.m_size -= normalized;

            return buf;
        }
    };

    /* OS Constants */

    constexpr int page_size = 0x1000;

    const buffer um_bounds = buffer::from_bounds(0x000000000000ull, 0x7FFFFFFFFFFFull);
    const buffer km_bounds = buffer::from_bounds(0x800000000000ull, 0xFFFFFFFFFFFFull);
}

#pragma once

#include <cstdint>
#include <concepts>
#include <compare>
#include <stdexcept>
#include <generator>

#include <phnt_windows.h>
#include <phnt.h>

#include <hydra/detail.hpp>
#include <hydra/syscall.hpp>

namespace hy {
    struct addr;
    class region;

    /* Hydra address concepts for type safety and implicit conversions */

    namespace detail {
        template <class T>
        concept HydraPointer = std::same_as<std::remove_cvref_t<T>, addr>;

        template <class T>
        concept NativeOrHydraPointer = NativePointer<T> || HydraPointer<T>;

        template <class T>
        concept HydraPointerOperand = NativeOrHydraPointer<T> || IntegralPointer<T>;
    }

#   pragma pack(push, 1) // 'addr' must be 8 bytes and non-polymorphic, region must pack at 1

    /// Represents an agnostic address or pointer (including integral) type.
    /// Used to simplify Hydra's API.
    struct addr {
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

        /// Default constructor for addr.
        /// Initializes to nullptr.
        constexpr addr() : p(nullptr) { }

        /// Pre-increment operator for addr.
        /// Increments address by one.
        template <detail::NativeOrHydraPointer T = addr>
        T& operator++() { i++; return static_cast<T&>(*this); }

        /// Post-increment operator for addr.
        /// Increments address by one, returns previous value.
        template <detail::NativeOrHydraPointer T = addr>
        T operator++(int) { const addr tmp = *this; ++*this; return tmp; }

        /// Pre-decrement operator for addr.
        /// Decrements address by one.
        template <detail::NativeOrHydraPointer T = addr>
        T& operator--() { i--; return static_cast<T&>(*this); }

        /// Post-decrement operator for addr.
        /// Decrements address by one, returns previous value.
        template <detail::NativeOrHydraPointer T = addr>
        T operator--(int) { const addr tmp = *this; --*this; return tmp; }

        /// Add offset to addr.
        /// Increases address by offset.
        addr& operator+=(const std::size_t off) { i += off; return *this; }

        /// Subtract offset from addr.
        /// Decreases address by offset.
        addr& operator-=(const std::size_t off) { i -= off; return *this; }

        /// Normalize various types to uintptr_t.
        /// Converts address-like or integral types to uintptr_t.
        template <class T>
        static constexpr std::uintptr_t normalize(const T& value) {
            if      constexpr (std::same_as<std::remove_cvref_t<T>, addr>)              return value.i;
            else if constexpr (std::same_as<std::remove_cvref_t<T>, std::uintptr_t>)    return value;
            else if constexpr (std::same_as<std::remove_cvref_t<T>, std::nullptr_t>)    return 0;
            else if constexpr (std::is_pointer_v<std::remove_cvref_t<T>>)               return reinterpret_cast<std::uintptr_t>(value);
            else if constexpr (detail::IntegralPointer<T>)                              return static_cast<std::uintptr_t>(value);
            else                                                                        static_assert([]{ return false; }(), "Unsupported source type for address arithmetic");
            return 0;
        }

        /// Convert uintptr_t to various types.
        /// Converts integer address to addr, pointer, or integer type.
        template <class Rt>
        static constexpr Rt convert(std::uintptr_t value) {
            if      constexpr (std::same_as<std::remove_cvref_t<Rt>, addr>)             return addr(value);
            else if constexpr (std::same_as<std::remove_cvref_t<Rt>, std::uintptr_t>)   return value;
            else if constexpr (std::same_as<std::remove_cvref_t<Rt>, std::nullptr_t>)   return nullptr;
            else if constexpr (std::is_pointer_v<std::remove_cvref_t<Rt>>)              return reinterpret_cast<Rt>(value);
            else if constexpr (detail::IntegralPointer<Rt>)                             return static_cast<Rt>(value);
            else                                                                        static_assert([]{ return false; }(), "Unsupported return type for address arithmetic");
            return 0;
        }

        /// Construct addr from address-like type.
        /// Accepts pointer, integer, or addr types.
        template <detail::NativeOrHydraPointer T>
        constexpr addr(T ptr) noexcept /* NOLINT: Implicit construction expected */ : i(normalize(ptr)) { }

        /// Implicit conversion to address-like type.
        /// Converts addr to pointer, integer, or addr type.
        template <detail::NativeOrHydraPointer T> requires (!std::is_same_v<std::nullptr_t, std::remove_cvref_t<T>>)
        constexpr operator T() const noexcept /* NOLINT: Implicit conversion expected */ { return convert<T>(i); }

        /// Implicit conversion to boolean.
        /// Returns true if address is truthy.
        constexpr operator bool() const noexcept /* NOLINT: Implicit conversion expected */ { return i != 0; }

        /// Addition operator for addr arithmetic.
        /// Handles addr + addr, addr + integral, addr + pointer
        template <detail::HydraPointerOperand T>
        friend constexpr addr operator+(const addr& a, const T& rhs) {
            addr result = a;
            result.i += normalize(rhs);
            return result;
        }

        /// Subtraction operator for addr arithmetic.
        /// Handles addr - addr, addr - integral, addr - pointer
        template <detail::HydraPointerOperand T>
        friend constexpr addr operator-(const addr& a, const T& rhs) {
            addr result = a;
            result.i -= normalize(rhs);
            return result;
        }

        /// Multiplication operator for addr arithmetic.
        /// Handles addr * addr, addr * integral, addr * pointer
        template <detail::HydraPointerOperand T>
        friend constexpr addr operator*(const addr& a, const T& rhs) {
            addr result = a;
            result.i *= normalize(rhs);
            return result;
        }

        /// Division operator for addr arithmetic.
        /// Handles addr / addr, addr / integral, addr / pointer
        template <detail::HydraPointerOperand T>
        friend constexpr addr operator/(const addr& a, const T& rhs) {
            addr result = a;
            result.i /= normalize(rhs);
            return result;
        }

        /// Three-way comparison operator for addr.
        /// Compares two addresses or address-like values.
        template <detail::HydraPointerOperand T, detail::HydraPointerOperand Lhs>
        friend constexpr std::strong_ordering operator<=>(const Lhs& lhs, const T& rhs) {
            const auto li = addr(lhs).i;
            const auto ri = normalize(rhs);

            if (li == ri) return std::strong_ordering::equal;
            return (li < ri) ? std::strong_ordering::less : std::strong_ordering::greater;
        }

        /// Equality comparison operator for addr.
        /// Returns true if addresses are equal.
        template <detail::HydraPointerOperand T>
        friend constexpr bool operator==(const addr& lhs, const T rhs) { // NOLINT: Comparison with convertible type
            return lhs.i == normalize(rhs);
        }
    };

    /// Represents a region of memory. It can either be created, inherited from,
    /// or point to an unmanaged memory range.
    class region { // TODO: implement virtual memory mapping and shared memory regions, rename to alloc_local/alloc_shared opt name
        friend class process;

        addr         m_base = nullptr; // 0x00
        std::size_t  m_size = 0;       // 0x08
        bool         m_owner;          // 0x10
        bool         m_zero = false;   // 0x11

        bool allocate(std::size_t size) {
            // Update ownership status
            m_owner = true;

            const auto status = syscall::NtAllocateVirtualMemory(
                NtCurrentProcess(), &m_base.u, 0, &size,
                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE
            );

            // Verify allocation
            if (!NT_SUCCESS(status) || !m_base) {
                release();
                return false;
            }

            // Zero memory if applicble
            if (m_zero) RtlZeroMemory(m_base, size);

            return true;
        }

        bool release() {
            if (m_owner && m_base) {
                const auto status = syscall::NtFreeVirtualMemory(NtCurrentProcess(), &m_base.u, &m_size, MEM_RELEASE);
                if (!NT_SUCCESS(status)) return false;
            }

            // Clear region data
            m_base = nullptr;
            m_size = 0;
            m_owner = false;

            return true;
        }

    public:
        /// Default constructor for buffer.
        /// Initializes empty buffer, not owned.
        explicit region() : m_owner(false) { }

        /// Allocates a buffer of given size.
        /// Buffer owns its memory.
        static region alloc_local(const std::size_t size, const bool zero = false) {
            region buf;

            buf.m_size = size;
            buf.m_owner = true;
            buf.m_zero = zero;

            buf.allocate(size);
            return buf;
        }

        /// Wraps an existing address as buffer.
        /// Buffer does not own memory.
        region(const addr base, const std::size_t size = 0) : m_base(base.p), m_size(size), m_owner(false) { } // NOLINT

        /// Wraps an array as buffer.
        /// Buffer does not own memory.
        template <std::size_t Size, class T>
        explicit constexpr region(T (&data)[Size]) : region(data, Size) { }

        /// Wraps a pointer as buffer.
        /// Buffer does not own memory.
        template <class T>
        constexpr explicit region(T* ref) : m_size(sizeof(T)), m_owner(false) {
            if constexpr (std::is_same_v<T, std::uint8_t>) {
                m_base = ref;
                return;
            }
            m_base = reinterpret_cast<std::uint8_t*>(ref);
        }

        /// Copy constructor for buffer.
        /// Buffer does not own memory.
        region(const region& other) : m_base(other.m_base), m_size(other.m_size), m_owner(false) { }

        /// Move constructor for buffer.
        /// Transfers ownership and memory.
        region(region&& other) noexcept :
            m_base(std::exchange(other.m_base, nullptr)),
            m_size(std::exchange(other.m_size, 0)),
            m_owner(std::exchange(other.m_owner, false)) { }

        /// Destructor for buffer.
        /// Frees owned memory and clears buffer.
        ~region() {
            release();
        }

        /// Checks if buffer is valid.
        /// Returns true if buffer has memory.
        operator bool() const { return m_base != nullptr; } // NOLINT

        /// Implicit conversion to addr.
        /// Returns base address as addr.
        operator addr() const { return m_base; } // NOLINT

        /// Copy assignment for buffer.
        /// Buffer does not own memory.
        region& operator=(const region& other) {
            if (this != &other) {
                release();
                m_base = other.m_base;
                m_size = other.m_size;
                m_owner = false;
            }
            return *this;
        }

        /// Move assignment for buffer.
        /// Transfers ownership and memory.
        region& operator=(region&& other) noexcept {
            if (this != &other) {
                release();
                m_base = std::exchange(other.m_base, nullptr);
                m_size = std::exchange(other.m_size, 0);
                m_owner = std::exchange(other.m_owner, false);
            }
            return *this;
        }

        /// Equality operator for buffer.
        /// Checks equality of base/size.
        bool operator==(const region& other) const {
            return other.m_base == this->m_base && other.m_size == this->m_size;
        }

        /// Returns base address of buffer.
        /// Address of first byte.
        [[nodiscard]] addr base() const { return m_base; }

        /// Sets base address of buffer.
        /// Address of first byte.
        void base(const addr base) { m_base = base;  }

        /// Returns size of buffer.
        /// Number of bytes in buffer.
        [[nodiscard]] std::size_t size() const { return m_size; }

        /// Sets size of buffer.
        /// Number of bytes in buffer.
        void size(const std::size_t size) { m_size = size; }

        /// Returns end address of buffer.
        /// Address at last byte.
        [[nodiscard]] addr end() const { return m_base + m_size - 1; }

        /// Checks if address is within buffer.
        /// Returns true if address is in range.
        [[nodiscard]] bool contains(const addr addr) const { return addr <= end(); }

        /// Rebase a virtual address to a new base.
        /// Returns rebased address.
        [[nodiscard]] addr rebase(const addr offset, const addr base = 0ull) const {
            return (offset - m_base) + base;
        }

        /// Resize the buffer.
        /// Changes buffer size and reallocates memory.
        void resize(const std::size_t size) {
            if (!m_owner) throw std::runtime_error("Cannot resize non-owned buffer");

            region temp(size);
            if (m_base) std::memcpy(temp.m_base, m_base, size);

            *this = std::move(temp);
        }

        /// Change the buffer size without allocating memory.
        /// This function can be dangerous.
        void override_size(const std::size_t size) {
            m_size = size;
        }

        /// Fill the buffer with a byte value.
        /// Returns void.
        void fill(const std::uint8_t value) const {
            std::memset(m_base, value, m_size);
        }
        
        /// Match a specific pattern given a pattern and mask.
        /// Returns boolean that determines if match succeeded.
        static bool match_aob(const addr at, const detail::Byte auto* in_pattern, const detail::Byte auto* in_mask, const std::size_t size) {
            const auto pattern = reinterpret_cast<const char*>(in_pattern);
            const auto mask = reinterpret_cast<const char*>(in_mask);

            for (std::size_t i = 0; i < size; ++i) {
                if (mask[i] == '?') continue;
                if (at.p[i] != pattern[i]) return false;
            }

            return true;
        }

        /// Match a specific pattern given a pattern and mask.
        /// Returns generator of matching addresses.
        static std::generator<addr> scan_aob(const addr base, std::size_t size, const detail::Byte auto* in_pattern, const detail::Byte auto* in_mask) {
            if (!in_pattern || !in_mask || !base || !size)
                co_return;

            if (!size) {
                const auto mask = reinterpret_cast<const char*>(in_mask);
                size = std::strlen(mask);
            }

            const auto end = base + size;
            for (addr at = base; at <= end; ++at) {
                // TODO: Let's have fun here someday. Multithreading? SIMD? Sick of scanning huge binaries.
                if (region::match_aob(at, in_pattern, in_mask, size))
                    co_yield at;
            }
        }

        /// Scan buffer for pattern using dynamic mask array.
        /// Returns generator of matching addresses.
        std::generator<addr> scan_aob(const detail::Byte auto* in_pattern, const detail::Byte auto* in_mask, std::size_t size = 0) const {
            if (!in_pattern || !in_mask || !m_base || !m_size)
                co_return;

            for (const auto& match : region::scan_aob(m_base, m_size, in_pattern, in_mask))
                co_yield match;
        }

        /// Scan buffer for pattern using static mask array.
        /// Returns generator of matching addresses.
        template <int Size>
        std::generator<addr> scan_aob(const detail::Byte auto (&pattern)[Size], const detail::Byte auto (&mask)[Size]) const {
            return scan_aob(pattern, mask, Size);
        }
    };

#   pragma pack(pop) // 'addr' must be 8 bytes and non-polymorphic, region must pack at 1

    /* OS Constants */

    /// OS page size constant.
    /// Typically 4096 bytes.
    constexpr std::size_t page_size = PAGE_SIZE; // see ntdef.h

    /// User-mode address bounds buffer.
    /// Range: 0x0000000000000000 - 7FFFFFFFFFFFFFFF.
    static auto umregion = region({ 0x0000000000000000ull, 0x7FFFFFFFFFFFFFFFull });

    /// Kernel-mode address bounds buffer.
    /// Range: 0x8000000000000000 - 0xFFFFFFFFFFFFFFFF.
    static auto kmregion = region({ 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull });

    /// Null region constant.
    /// Range: 0x0 - 0x0.
    static auto nullregion = region();
}

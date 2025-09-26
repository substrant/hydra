#pragma once

#include "detail/pch.hpp"
#include "detail/generator.hpp"

namespace hy::mem {
    /// Represents a generic address type.
    /// Supports automatic casting and arithmetic.
    struct addr;

    namespace detail {
        template <class T>
        concept PrimitiveObject = std::default_initializable<T>;

        template <class T>
        concept PointerType = std::is_pointer_v<std::remove_cvref_t<T>> || std::same_as<std::remove_cvref_t<T>, std::uintptr_t>;

        template <class T>
        concept HydraAddress = std::same_as<std::remove_cvref_t<T>, addr>;

        template <class T>
        concept AddressLike =
            std::same_as<std::remove_cvref_t<T>, addr> ||
            std::same_as<std::remove_cvref_t<T>, std::nullptr_t> ||
            std::same_as<std::remove_cvref_t<T>, std::uintptr_t> ||
            std::is_pointer_v<std::remove_cvref_t<T>>;

        template <class T>
        concept AddressPrimitive = AddressLike<T> && !std::same_as<std::remove_cvref_t<T>, addr>;

        template <class T>
        concept IntegralLike = std::is_integral_v<std::remove_cvref_t<T>> || std::convertible_to<T, std::uintptr_t>;

        template <class T>
        concept AddressArithmeticOperand = AddressLike<T> || IntegralLike<T>;
    }

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

        /// Default constructor for addr.
        /// Initializes to nullptr.
        constexpr addr() : p(nullptr) { }

        /// Pre-increment operator for addr.
        /// Increments address by one.
        template <detail::AddressLike T = addr>
        T& operator++() { i++; return static_cast<T&>(*this); }

        /// Post-increment operator for addr.
        /// Increments address by one, returns previous value.
        template <detail::AddressLike T = addr>
        T operator++(int) { const addr tmp = *this; ++(*this); return tmp; }

        /// Pre-decrement operator for addr.
        /// Decrements address by one.
        template <detail::AddressLike T = addr>
        T& operator--() { i--; return static_cast<T&>(*this); }

        /// Post-decrement operator for addr.
        /// Decrements address by one, returns previous value.
        template <detail::AddressLike T = addr>
        T operator--(int) { const addr tmp = *this; --(*this); return tmp; }

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
            if      constexpr (std::same_as<std::remove_cvref_t<T>, addr>)               return value.i;
            else if constexpr (std::same_as<std::remove_cvref_t<T>, std::uintptr_t>)     return value;
            else if constexpr (std::same_as<std::remove_cvref_t<T>, std::nullptr_t>)     return 0;
            else if constexpr (std::is_pointer_v<std::remove_cvref_t<T>>)                return reinterpret_cast<std::uintptr_t>(value);
            else if constexpr (detail::IntegralLike<T>)                                  return static_cast<std::uintptr_t>(value);
            else                                                                         static_assert([]{ return false; }(), "Unsupported source type for address arithmetic");
            return 0;
        }

        /// Convert uintptr_t to various types.
        /// Converts integer address to addr, pointer, or integer type.
        template <class Rt>
        static constexpr Rt convert(std::uintptr_t value) {
            if      constexpr (std::same_as<std::remove_cvref_t<Rt>, addr>)               return addr(value);
            else if constexpr (std::same_as<std::remove_cvref_t<Rt>, std::uintptr_t>)     return value;
            else if constexpr (std::same_as<std::remove_cvref_t<Rt>, std::nullptr_t>)     return nullptr;
            else if constexpr (std::is_pointer_v<std::remove_cvref_t<Rt>>)                return reinterpret_cast<Rt>(value);
            else if constexpr (detail::IntegralLike<Rt>)                                  return static_cast<Rt>(value);
            else                                                                          static_assert([]{ return false; }(), "Unsupported return type for address arithmetic");
            return 0;
        }

        /// Construct addr from address-like type.
        /// Accepts pointer, integer, or addr types.
        template <detail::AddressLike T>
        constexpr addr(T ptr) : i(normalize(ptr)) { }

        /// Implicit conversion to address-like type.
        /// Converts addr to pointer, integer, or addr type.
        template <detail::AddressLike T> requires (!std::is_same_v<std::nullptr_t, std::remove_cvref_t<T>>)
        constexpr operator T() const {
            return convert<T>(i);
        }

        /// Checks if addr is not null.
        /// Returns true if address is not null.
        operator bool() const {
            return p != nullptr;
        }

        /// Addition operator for addr arithmetic.
        /// Handles addr + addr, addr + integral, addr + pointer
        template <detail::AddressArithmeticOperand T>
        friend constexpr addr operator+(const addr& a, const T& rhs) {
            addr result = a;
            result.i += normalize(rhs);
            return result;
        }

        /// Subtraction operator for addr arithmetic.
        /// Handles addr - addr, addr - integral, addr - pointer
        template <detail::AddressArithmeticOperand T>
        friend constexpr addr operator-(const addr& a, const T& rhs) {
            addr result = a;
            result.i -= normalize(rhs);
            return result;
        }

        /// Three-way comparison operator for addr.
        /// Compares two addresses or address-like values.
        template <detail::AddressArithmeticOperand T, detail::AddressArithmeticOperand Lhs>
        friend constexpr std::strong_ordering operator<=>(const Lhs& lhs, const T& rhs) {
            const auto li = addr(lhs).i;
            const auto ri = normalize(rhs);

            if (li == ri) return std::strong_ordering::equal;
            return (li < ri) ? std::strong_ordering::less : std::strong_ordering::greater;
        }

        /// Equality comparison operator for addr.
        /// Returns true if addresses are equal.
        template <detail::AddressArithmeticOperand T>
        friend constexpr bool operator==(const addr& lhs, const T rhs) {
            return lhs.i == normalize(rhs);
        }
    };
#   pragma pack(pop)

    namespace detail {
        template <typename T>
        concept AddressCastable = requires(T t, addr a) {
            { addr{ t } };
            { static_cast<T>(a) };
        };

        inline bool match_aob(const addr base, const std::uint8_t* pattern, const char* mask, const std::size_t size) {
            for (std::size_t off = 0; off < size; off++) {
                if (mask[off] != '?' && pattern[off] != *static_cast<std::uint8_t*>(base + off))
                    return false;
            }
            return true;
        }

        inline hy::detail::generator<addr> scan_aob(const addr start_addr, const addr end_addr, const std::uint8_t* pattern, const char* mask, const std::size_t size) {
            for (auto addr = start_addr; addr < end_addr; ++addr) {
                if (match_aob(addr, pattern, mask, size))
                    co_yield addr;
            }
        }
    }

    /// Represents a memory buffer with ownership and utility functions.
    /// Provides allocation, scanning, and address utilities.
    class buffer {
        std::uint8_t* m_base = nullptr;
        std::size_t   m_size = 0;
        bool          m_owner;
        bool          m_zero = false;

        void alloc(const std::size_t size) {
            clear(); // Free old data if needed

            m_base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            if (!m_base) throw std::runtime_error("VirtualAlloc failed");
            if (m_zero) std::memset(m_base, 0, size);

            m_size = size;
            m_owner = true;
        }

        void clear() {
            if (!m_base) return;
            if (m_owner) VirtualFree(m_base, 0, MEM_RELEASE);
            m_base = nullptr;
            m_size = 0;
        }

    public:
        /// Default constructor for buffer.
        /// Initializes empty buffer, not owned.
        explicit buffer() : m_owner(false) { }

        /// Allocates a buffer of given size.
        /// Buffer owns its memory.
        static buffer create(const std::size_t size, const bool zero = false) {
            buffer buf;

            buf.m_size = size;
            buf.m_owner = true;
            buf.m_zero = zero;

            buf.alloc(size);
            return buf;
        }

        /// Wraps an existing address as buffer.
        /// Buffer does not own memory.
        buffer(const addr base, const std::size_t size = 0) : m_base(base.p), m_size(size), m_owner(false) { }

        /// Wraps an array as buffer.
        /// Buffer does not own memory.
        template <std::size_t Size, class T>
        constexpr buffer(T (&data)[Size]) : buffer(data, Size) { }

        /// Wraps a pointer as buffer.
        /// Buffer does not own memory.
        template <class T>
        constexpr explicit buffer(T* ref) : m_size(sizeof(T)), m_owner(false) {
            if constexpr (std::is_same_v<T, std::uint8_t>) {
                m_base = ref;
                return;
            }
            m_base = reinterpret_cast<std::uint8_t*>(ref);
        }

        /// Wraps a tuple of address bounds as buffer.
        /// Buffer does not own memory.
        explicit buffer(const addr start, const addr end) : m_base(start.p), m_owner(false) {
            const auto diff = (end - m_base).d;
            m_size = (diff < 0) ? diff : throw std::runtime_error("End is before start of buffer");
        }

        /// Copy constructor for buffer.
        /// Buffer does not own memory.
        buffer(const buffer& other) : m_base(other.m_base), m_size(other.m_size), m_owner(false) { }

        /// Move constructor for buffer.
        /// Transfers ownership and memory.
        buffer(buffer&& other) noexcept :
            m_base(std::exchange(other.m_base, nullptr)),
            m_size(std::exchange(other.m_size, 0)),
            m_owner(std::exchange(other.m_owner, false)) { }

        /// Destructor for buffer.
        /// Frees owned memory and clears buffer.
        ~buffer() {
            clear();
        }

        /// Checks if buffer is valid.
        /// Returns true if buffer has memory.
        operator bool() const {
            return m_base != nullptr;
        }

        /// Implicit conversion to addr.
        /// Returns base address as addr.
        operator addr() const {
            return m_base;
        }

        /// Copy assignment for buffer.
        /// Buffer does not own memory.
        buffer& operator=(const buffer& other) {
            if (this != &other) {
                clear();
                m_base = other.m_base;
                m_size = other.m_size;
                m_owner = false;
            }
            return *this;
        }

        /// Move assignment for buffer.
        /// Transfers ownership and memory.
        buffer& operator=(buffer&& other) noexcept {
            if (this != &other) {
                clear();
                m_base = std::exchange(other.m_base, nullptr);
                m_size = std::exchange(other.m_size, 0);
                m_owner = std::exchange(other.m_owner, false);
            }
            return *this;
        }

        /// Returns base address of buffer.
        /// Address of first byte.
        addr base() const { return m_base; }

        /// Returns size of buffer.
        /// Number of bytes in buffer.
        std::size_t size() const { return m_size; }

        /// Returns end address of buffer.
        /// Address after last byte.
        addr end() const { return m_base + m_size; }

        /// Checks if address is within buffer.
        /// Returns true if address is in range.
        bool contains(const addr addr) const { return addr <= end(); }

        /// Rebase a virtual address to a new base.
        /// Returns rebased address.
        addr rebase(const addr virtual_addr, const addr base = 0ull) const {
            return (virtual_addr - m_base) + base;
        }

        /// Resize the buffer.
        /// Changes buffer size and reallocates memory.
        void resize(const std::size_t size) {
            if (!m_owner)
                throw std::runtime_error("Cannot resize non-owned buffer");

            buffer temp(size);
            if (m_base)
                std::memcpy(temp.m_base, m_base, size);

            *this = std::move(temp);
        }

        /// Change the buffer size without allocating memory.
        /// This function can be dangerous.
        void override_size(const std::size_t size) {
            m_size = size;
        }

        /// Scan buffer for pattern using mask.
        /// Returns generator of matching addresses.
        hy::detail::generator<addr> scan_aob(const std::uint8_t* pattern, const char* mask, std::size_t size = 0) const {
            if (!size) size = strlen(mask);
            return detail::scan_aob(m_base, end(), pattern, mask, size);
        }

        /// Scan buffer for pattern using mask array.
        /// Returns generator of matching addresses.
        template <int Size>
        constexpr hy::detail::generator<addr> scan_aob(const std::uint8_t* pattern, const char (&mask)[Size]) const {
            return detail::scan_aob(m_base, end(), pattern, mask, Size);
        }
    };

    /* Memory Helpers */

    template <class Ret = addr, class T> requires detail::AddressCastable<Ret> && (!detail::AddressPrimitive<Ret>) && detail::PointerType<T>
    Ret ref(T* value) { return static_cast<addr>(value); }

    /* OS Constants */

    /// OS page size constant.
    /// Typically 4096 bytes.
    constexpr uint32_t page_size = 0x1000;

    /// User-mode address bounds buffer.
    /// Range: 0x000000000000 - 0x7FFFFFFFFFFF.
    static auto um_bounds = buffer({ 0x000000000000ull, 0x7FFFFFFFFFFFull });

    /// Kernel-mode address bounds buffer.
    /// Range: 0x800000000000 - 0xFFFFFFFFFFFF.
    static auto km_bounds = buffer({ 0x800000000000ull, 0xFFFFFFFFFFFFull });
}

#pragma once

#include <generator>

#include <hydra/dtl.hpp>
#include <hydra/ptr.hpp>

namespace hy {

#   pragma pack(push, 1)

    /// Represents a block of memory. It can either be created, inherited from,
    /// or point to an unmanaged memory range.
    class blk {
    public:
        ptr base = nullptr;
        std::size_t size = 0;

    private:
        bool m_owner;

        void allocate(const std::size_t new_size, const bool zero) {
            m_owner = true;
            size = new_size;
            base = std::malloc(new_size);

            if (zero)
                std::memset(base, 0, size);
        }

        bool release() {
            if (m_owner && base)
                std::free(base);

            base = nullptr;
            size = 0;
            m_owner = false;

            return true;
        }

    public:
        /// Default constructor for block. Block does not own memory.
        explicit blk() : m_owner(false) { }

        /// Allocate a block of memory. Block owns memory.
        explicit blk(const std::size_t size, const bool zero = false) {
            allocate(size, zero);
        }

        /// Wraps an existing block of memory. Block does not own memory.
        explicit blk(const ptr base, const std::size_t size = 0)
            : base(base.p), size(size), m_owner(false) { }

        /// Wraps an array as block. Block does not own array.
        template <std::size_t Size, class T>
        constexpr explicit blk(T(&data)[Size]) : blk(data, Size) { }

        /// Wraps a pointer as a block. Block does not own pointer.
        template <class T>
        constexpr explicit blk(T* ref) : size(sizeof(T)), m_owner(false) {
            if constexpr (std::is_same_v<T, std::uint8_t>)
                base = ref;
            else
                base = reinterpret_cast<std::uint8_t*>(ref);
        }

        /// Copy constructor for block. Block does not own memory.
        blk(const blk& other) : base(other.base), size(other.size), m_owner(false) { }

        /// Move constructor for buffer.
        /// Transfers ownership and memory.
        blk(blk&& other) noexcept :
            base(std::exchange(other.base, nullptr)),
            size(std::exchange(other.size, 0)),
            m_owner(std::exchange(other.m_owner, false)) { }

        /// Destructor for buffer.
        /// Frees owned memory and clears buffer.
        virtual ~blk() {
            release();
        }

        /// Checks if buffer is valid.
        /// Returns true if buffer has memory.
        operator bool() const { return base != nullptr; } // NOLINT

        /// Implicit conversion to ptr.
        /// Returns base address as ptr.
        operator ptr() const { return base; } // NOLINT

        /// Copy assignment for buffer.
        /// Buffer does not own memory.
        blk& operator=(const blk& other) {
            if (this != &other) {
                release();
                base = other.base;
                size = other.size;
                m_owner = false;
            }
            return *this;
        }

        /// Move assignment for buffer.
        /// Transfers ownership and memory.
        blk& operator=(blk&& other) noexcept {
            if (this != &other) {
                release();
                base = std::exchange(other.base, nullptr);
                size = std::exchange(other.size, 0);
                m_owner = std::exchange(other.m_owner, false);
            }
            return *this;
        }

        /// Equality operator for buffer.
        /// Checks equality of base/length.
        bool operator==(const blk& other) const {
            return other.base == this->base && other.size == this->size;
        }

        /// Returns end address of buffer.
        /// Address at last byte.
        [[nodiscard]] ptr end() const { return base + size - 1; }

        /// Checks if address is within buffer.
        /// Returns true if address is in range.
        [[nodiscard]] bool contains(const ptr ptr) const { return ptr <= end(); }

        /// Rebase a virtual address to a new base.
        /// Returns rebased address.
        [[nodiscard]] ptr rebase(const ptr offset, const std::ptrdiff_t base_offset = 0ull) const {
            return (offset - base_offset) + base;
        }

        /// Resize the buffer.
        /// Changes buffer length and reallocates memory. Returns false if not owned.
        bool resize(const std::size_t size) {
            if (!m_owner) return false;

            blk temp(size);
            if (base) std::memcpy(temp.base, base, size);

            *this = std::move(temp);
            return true;
        }

        /// Fill the buffer with a byte value.
        /// Returns void.
        void fill(const std::uint8_t value) const {
            std::memset(base, value, size);
        }
        
        /// Scan buffer for pattern using dynamic mask array.
        /// Returns generator of matching addresses.
        std::generator<ptr> scan_aob(const dtl::Byte auto* in_pattern, const dtl::Byte auto* in_mask, std::size_t size = 0) {
            if (!in_pattern || !in_mask || !base || !size)
                co_return;

            for (const auto& match : scan_aob(base, size, in_pattern, in_mask))
                co_yield match;
        }

        /// Scan buffer for pattern using static mask array.
        /// Returns generator of matching addresses.
        template <int Size>
        std::generator<ptr> scan_aob(const dtl::Byte auto (&pattern)[Size], const dtl::Byte auto (&mask)[Size]) {
            return scan_aob(pattern, mask, Size);
        }
    };

#   pragma pack(pop)

}

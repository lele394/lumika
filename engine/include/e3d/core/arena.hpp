#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <utility>

namespace e3d {

/// Bump allocator for load-time and per-frame data. Never frees individual
/// allocations; `reset()` recycles everything at once.
class Arena final {
public:
    explicit Arena(std::size_t block_size = 64 * 1024) : block_size_(block_size) {}

    ~Arena() { release(); }

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void* alloc(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) {
        if (size == 0) size = 1;
        if (alignment < 1) alignment = 1;

        Block* block = current_;
        if (block != nullptr) {
            const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(block->data());
            const std::uintptr_t aligned = (base + block->used + alignment - 1) & ~(alignment - 1);
            const std::size_t padding = aligned - base - block->used;
            if (block->used + padding + size <= block->capacity) {
                block->used += padding + size;
                total_used_ += padding + size;
                return reinterpret_cast<void*>(base + block->used - size);
            }
        }

        const std::size_t capacity = size + alignment > block_size_ ? size + alignment : block_size_;
        auto* fresh = static_cast<Block*>(std::malloc(sizeof(Block) + capacity));
        if (fresh == nullptr) {
            std::abort();
        }
        fresh->next = blocks_;
        fresh->used = 0;
        fresh->capacity = capacity;
        blocks_ = fresh;
        current_ = fresh;

        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(fresh->data());
        const std::uintptr_t aligned = (base + alignment - 1) & ~(alignment - 1);
        const std::size_t padding = aligned - base;
        fresh->used = padding + size;
        total_used_ += padding + size;
        return reinterpret_cast<void*>(base + fresh->used - size);
    }

    template <typename T, typename... Args>
    T* create(Args&&... args) {
        void* memory = alloc(sizeof(T), alignof(T));
        return new (memory) T(static_cast<Args&&>(args)...);
    }

    /// Frees every block. The arena remains usable afterwards.
    void release() noexcept {
        Block* block = blocks_;
        while (block != nullptr) {
            Block* next = block->next;
            std::free(block);
            block = next;
        }
        blocks_ = nullptr;
        current_ = nullptr;
        total_used_ = 0;
    }

    [[nodiscard]] std::size_t used() const noexcept { return total_used_; }
    [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }

private:
    struct Block {
        Block* next;
        std::size_t used;
        std::size_t capacity;
        unsigned char* data() noexcept { return reinterpret_cast<unsigned char*>(this + 1); }
    };

    Block* blocks_ = nullptr;
    Block* current_ = nullptr;
    std::size_t block_size_;
    std::size_t total_used_ = 0;
};

} // namespace e3d

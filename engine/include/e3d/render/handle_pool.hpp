#pragma once

#include "e3d/core/types.hpp"

#include <utility>
#include <vector>

namespace e3d {

/// Dense handle table with generation checks. Removed slots are reused, and stale
/// handles fail validation instead of aliasing a new resource.
template <typename Tag, typename T>
class HandlePool final {
public:
    Handle<Tag> add(T value) {
        u32 index = 0;
        if (!free_.empty()) {
            index = free_.back();
            free_.pop_back();
        } else {
            index = static_cast<u32>(values_.size());
            values_.emplace_back();
            generations_.push_back(1);
        }

        values_[index] = std::move(value);
        return Handle<Tag>{index, generations_[index]};
    }

    [[nodiscard]] T* get(Handle<Tag> handle) noexcept {
        if (!handle.valid() || handle.index >= values_.size()) return nullptr;
        if (generations_[handle.index] != handle.generation) return nullptr;
        return &values_[handle.index];
    }

    [[nodiscard]] const T* get(Handle<Tag> handle) const noexcept {
        return const_cast<HandlePool*>(this)->get(handle);
    }

    void remove(Handle<Tag> handle) noexcept {
        if (get(handle) == nullptr) return;
        free_.push_back(handle.index);
        ++generations_[handle.index];
    }

    template <typename F>
    void for_each(F&& function) {
        for (T& value : values_) function(value);
    }

    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }

private:
    std::vector<T> values_;
    std::vector<u32> generations_;
    std::vector<u32> free_;
};

} // namespace e3d

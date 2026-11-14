#pragma once

#include "e3d/core/json.hpp"
#include "e3d/core/types.hpp"

namespace e3d {

struct ComponentDesc;

/// Base class for every engine- or project-defined component.
class Component {
public:
    virtual ~Component() = default;

    [[nodiscard]] virtual const ComponentDesc* descriptor() const = 0;

    [[nodiscard]] EntityId entity() const noexcept { return entity_; }
    void bind_entity(EntityId id) noexcept { entity_ = id; }

    bool enabled = true;

    /// Unknown serialized properties, preserved verbatim so older runtimes and
    /// partial editors never delete data they do not understand.
    json::Value extra_props;

private:
    EntityId entity_ = kInvalidEntity;
};

} // namespace e3d

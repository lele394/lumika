#include <doctest/doctest.h>

#include "e3d/core/types.hpp"

TEST_CASE("handles are invalid until assigned") {
    e3d::Handle<struct MeshTag> handle;
    CHECK_FALSE(handle.valid());
    CHECK(handle.index == e3d::kInvalidIndex);

    handle.index = 7;
    CHECK(handle.valid());

    handle.reset();
    CHECK_FALSE(handle.valid());
}

TEST_CASE("guid zero is invalid") {
    e3d::Guid guid;
    CHECK_FALSE(guid.valid());

    guid.lo = 1;
    CHECK(guid.valid());
}

TEST_CASE("vec3 arithmetic") {
    const e3d::Vec3 a{1.0f, 2.0f, 3.0f};
    const e3d::Vec3 b{0.5f, -1.0f, 2.0f};

    const e3d::Vec3 sum = a + b;
    CHECK(sum.x == doctest::Approx(1.5f));
    CHECK(sum.y == doctest::Approx(1.0f));
    CHECK(sum.z == doctest::Approx(5.0f));

    const e3d::Vec3 scaled = a * 2.0f;
    CHECK(scaled.x == doctest::Approx(2.0f));
    CHECK(scaled.z == doctest::Approx(6.0f));
}

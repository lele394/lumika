#include <doctest/doctest.h>

#include "e3d/core/math.hpp"

using namespace e3d;

TEST_CASE("identity and translation") {
    const Vec3 point{1.0f, 2.0f, 3.0f};
    const Vec3 same = transform_point(mat4_identity(), point);
    CHECK(same.x == doctest::Approx(point.x));
    CHECK(same.y == doctest::Approx(point.y));
    CHECK(same.z == doctest::Approx(point.z));

    const Vec3 moved = transform_point(mat4_translation({10.0f, -2.0f, 0.5f}), point);
    CHECK(moved.x == doctest::Approx(11.0f));
    CHECK(moved.y == doctest::Approx(0.0f));
    CHECK(moved.z == doctest::Approx(3.5f));
}

TEST_CASE("yaw rotates x towards -z") {
    const Mat4 rotation = mat4_from_quat(quat_from_euler_xyz_degrees({0.0f, 90.0f, 0.0f}));
    const Vec3 rotated = transform_point(rotation, {1.0f, 0.0f, 0.0f});

    CHECK(rotated.x == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(rotated.y == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(rotated.z == doctest::Approx(-1.0f).epsilon(1e-4));
}

TEST_CASE("scale and composition order") {
    const Mat4 translation = mat4_translation({5.0f, 0.0f, 0.0f});
    const Mat4 scale = mat4_scale({2.0f, 2.0f, 2.0f});

    const Vec3 scaled_then_moved = transform_point(translation * scale, {1.0f, 0.0f, 0.0f});
    CHECK(scaled_then_moved.x == doctest::Approx(7.0f));

    const Vec3 moved_then_scaled = transform_point(scale * translation, {1.0f, 0.0f, 0.0f});
    CHECK(moved_then_scaled.x == doctest::Approx(12.0f));
}

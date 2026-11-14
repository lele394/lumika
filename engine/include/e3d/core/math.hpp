#pragma once

#include "e3d/core/types.hpp"

#include <cmath>

namespace e3d {

struct Quat final {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 1.0f;
};

/// 4x4 matrix, column-major: m[column * 4 + row].
struct Mat4 final {
    f32 m[16] = {};

    [[nodiscard]] f32& at(int row, int column) noexcept { return m[column * 4 + row]; }
    [[nodiscard]] f32 at(int row, int column) const noexcept { return m[column * 4 + row]; }
};

inline Mat4 mat4_identity() noexcept {
    Mat4 r{};
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

inline Mat4 mat4_translation(Vec3 t) noexcept {
    Mat4 r = mat4_identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

inline Mat4 mat4_scale(Vec3 s) noexcept {
    Mat4 r = mat4_identity();
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
}

inline constexpr f32 kDegToRad = 0.017453292519943295f;

inline Mat4 mat4_from_quat(Quat q) noexcept {
    const f32 xx = q.x * q.x;
    const f32 yy = q.y * q.y;
    const f32 zz = q.z * q.z;
    const f32 xy = q.x * q.y;
    const f32 xz = q.x * q.z;
    const f32 yz = q.y * q.z;
    const f32 wx = q.w * q.x;
    const f32 wy = q.w * q.y;
    const f32 wz = q.w * q.z;

    Mat4 r{};
    r.m[0] = 1.0f - 2.0f * (yy + zz);
    r.m[1] = 2.0f * (xy + wz);
    r.m[2] = 2.0f * (xz - wy);
    r.m[4] = 2.0f * (xy - wz);
    r.m[5] = 1.0f - 2.0f * (xx + zz);
    r.m[6] = 2.0f * (yz + wx);
    r.m[8] = 2.0f * (xz + wy);
    r.m[9] = 2.0f * (yz - wx);
    r.m[10] = 1.0f - 2.0f * (xx + yy);
    r.m[15] = 1.0f;
    return r;
}

inline constexpr Quat quat_mul(Quat a, Quat b) noexcept {
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

/// Euler angles in degrees, applied X then Y then Z (extrinsic).
inline Quat quat_from_euler_xyz_degrees(Vec3 degrees) noexcept {
    const f32 hx = degrees.x * kDegToRad * 0.5f;
    const f32 hy = degrees.y * kDegToRad * 0.5f;
    const f32 hz = degrees.z * kDegToRad * 0.5f;

    const Quat qx{std::sin(hx), 0.0f, 0.0f, std::cos(hx)};
    const Quat qy{0.0f, std::sin(hy), 0.0f, std::cos(hy)};
    const Quat qz{0.0f, 0.0f, std::sin(hz), std::cos(hz)};

    return quat_mul(qz, quat_mul(qy, qx));
}

inline Mat4 operator*(const Mat4& a, const Mat4& b) noexcept {
    Mat4 r{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            f32 sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[column * 4 + k];
            }
            r.m[column * 4 + row] = sum;
        }
    }
    return r;
}

inline Vec3 transform_point(const Mat4& mat, Vec3 p) noexcept {
    return {
        mat.m[0] * p.x + mat.m[4] * p.y + mat.m[8] * p.z + mat.m[12],
        mat.m[1] * p.x + mat.m[5] * p.y + mat.m[9] * p.z + mat.m[13],
        mat.m[2] * p.x + mat.m[6] * p.y + mat.m[10] * p.z + mat.m[14],
    };
}

inline Vec3 transform_direction(const Mat4& mat, Vec3 d) noexcept {
    return {
        mat.m[0] * d.x + mat.m[4] * d.y + mat.m[8] * d.z,
        mat.m[1] * d.x + mat.m[5] * d.y + mat.m[9] * d.z,
        mat.m[2] * d.x + mat.m[6] * d.y + mat.m[10] * d.z,
    };
}

inline constexpr f32 dot(Vec3 a, Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline f32 length(Vec3 v) noexcept { return std::sqrt(dot(v, v)); }

inline Vec3 normalize(Vec3 v) noexcept {
    const f32 len = length(v);
    if (len <= 1e-6f) return {0.0f, 0.0f, 0.0f};
    return v * (1.0f / len);
}

} // namespace e3d

// M0 smoke test: proves the devkitARM + libctru + Citro3D/Citro2D path end to end.
// Replaced by the engine platform layer in M2.

#include <3ds.h>
#include <citro2d.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "e3d/core/log.hpp"

namespace {

C3D_RenderTarget* g_top = nullptr;
C3D_RenderTarget* g_bottom = nullptr;
float g_angle = 0.0f;

void debug_sink(e3d::LogLevel level, const char* text) {
    static const char* const names[] = {"D", "I", "W", "E"};
    char line[1200];
    std::snprintf(line, sizeof(line), "[%s] %s", names[static_cast<int>(level)], text);
    svcOutputDebugString(line, static_cast<int>(std::strlen(line)));
}

void draw_top(float angle) {
    constexpr float kCx = 200.0f;
    constexpr float kCy = 120.0f;
    constexpr float kRadius = 72.0f;

    const float a0 = angle;
    const float a1 = angle + 2.0943951f;
    const float a2 = angle + 4.1887902f;

    C2D_DrawTriangle(kCx + std::cos(a0) * kRadius, kCy + std::sin(a0) * kRadius,
                     C2D_Color32(0xFF, 0x5A, 0x5A, 0xFF),
                     kCx + std::cos(a1) * kRadius, kCy + std::sin(a1) * kRadius,
                     C2D_Color32(0x5A, 0xFF, 0x9A, 0xFF),
                     kCx + std::cos(a2) * kRadius, kCy + std::sin(a2) * kRadius,
                     C2D_Color32(0x5A, 0x9A, 0xFF, 0xFF),
                     0.5f);
}

void draw_bottom(float angle) {
    C2D_DrawRectSolid(24.0f, 24.0f, 0.5f, 272.0f, 192.0f, C2D_Color32(0x18, 0x20, 0x30, 0xFF));

    const float x = 160.0f + std::cos(angle) * 96.0f;
    const float y = 120.0f + std::sin(angle) * 64.0f;
    C2D_DrawRectSolid(x - 10.0f, y - 10.0f, 0.4f, 20.0f, 20.0f, C2D_Color32(0xFF, 0xC8, 0x5A, 0xFF));
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    e3d::set_log_sink(&debug_sink);
    E3D_LOGI("e3d M0 smoke: libctru %d.%d", 2, 7);

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    u64 previous = osGetTime();

    while (aptMainLoop()) {
        hidScanInput();
        if ((hidKeysDown() & KEY_START) != 0) {
            break;
        }

        const u64 now = osGetTime();
        const float dt = static_cast<float>(now - previous) / 1000.0f;
        previous = now;
        g_angle += (dt > 0.0f ? dt : 0.016f) * 1.5f;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(g_top, C2D_Color32(0x0A, 0x0E, 0x1E, 0xFF));
        C2D_SceneBegin(g_top);
        draw_top(g_angle);

        C2D_TargetClear(g_bottom, C2D_Color32(0x08, 0x0A, 0x14, 0xFF));
        C2D_SceneBegin(g_bottom);
        draw_bottom(g_angle);

        C3D_FrameEnd(0);
    }

    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}

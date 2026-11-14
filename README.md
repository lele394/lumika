# 3ds-engine

A small, barebones, data-driven game runtime for the Nintendo 3DS / New 3DS, with a desktop editor.
Working title; nothing here is stable yet.

**Status: M3 complete except audio playback verification.** The demo project now loads
`main.scene.json`, resolves mesh/material/audio assets through a cooked manifest, runs C++ script
components (two spinning cubes), and drives both screens at 60 FPS. NDSP is implemented; Azahar
rejects `ndspInit()` without DSP system files (`0xD880A7FA`), so audio playback needs hardware.

## Verified so far

| Check | Result |
| --- | --- |
| 3DS build | `tetris3d`, `scene_check`, `render_check`, `demo3d` via devkitARM r68, libctru 2.7.1 |
| 3DS run | Citro2D smoke app on both screens at 60 FPS; `scene_check` reports `roundtrip=identical` |
| Dual-screen 3D | Two cameras, 1 draw per screen, 16.6 ms/frame; both screens confirmed |
| Textured lit 3D | Cooked `.cmesh` + `.t3x`, per-face lighting, 60 FPS |
| Scene-driven demo | Manifest + material JSON + 3 scripts; 109.8° scripted rotation; 0 warnings |
| Host tests | 41 test cases / 314 assertions pass via CTest |
| Audio | WAV parser and NDSP backend implemented; clip loads, playback blocked by Azahar DSP |

## What works now

- 128-bit GUIDs, generation-checked entity ids, JSON parser/writer, bump arena, Vec/Mat4/Quat math.
- Component registry with typed properties (`E3D_COMPONENT` / `E3D_PROP`), no code generation.
- `World`: entity hierarchy, cycle-safe reparenting, transform composition, component ownership.
- Scene load/save: two-pass entity references, asset GUIDs, unknown data preservation, deterministic
  output.
- Asset pipeline: `meshc` (OBJ → `.cmesh`), `tex3ds` (PNG → `.t3x`), manifest, material JSON, RomFS.
- Renderer: render list, Citro3D device, two targets, per-camera perspective, depth, layer masks,
  `unlit_color` and `lit_textured` shaders, textures, mesh upload with handle pools.
- Runtime: script lifecycle (`on_create/start/enable/update/fixed/disable/destroy`), fixed 60 Hz step,
  portable input state, audio API, deterministic dispatch order.
- Project scripts are plain C++ components compiled into the game; the scene references them by type.

## Layout

| Path | Purpose | Status |
| --- | --- | --- |
| `engine/` | Core: scene, reflection, assets, render list, runtime, audio API | M3 |
| `engine/platform/3ds/` | Citro3D device, NDSP audio, HID, RomFS | M3 |
| `engine/platform/desktop/` | GLFW + OpenGL 3.3 + miniaudio backend | planned |
| `editor/` | Dear ImGui editor | planned (M6) |
| `projects/demo_project/` | Scene, assets, scripts, demo apps | M3 |
| `tools/` | `scene_dump`, `meshc`, `cook.ps1`, build/run scripts | M2 |
| `third_party/` | Vendored small libraries (doctest now; ImGui, cgltf, miniaudio later) | M1 |
| `tests/` | Host-side unit tests | M1 |
| `docs/` | Assessment, architecture, roadmap | done |

## Docs

- `docs/00-assessment.md` — feasibility review of `prelim_design.md`, scope cuts, risks
- `docs/01-architecture.md` — engine design, formats, APIs
- `docs/02-roadmap.md` — milestones with acceptance criteria
- `prelim_design.md` — original notes, kept for history

## Build

The toolchain lives outside the repository (see below). From PowerShell:

```powershell
. .\tools\env.ps1          # host builds only; adds MinGW + devkitPro to PATH
.\tools\build.ps1 -Target desktop
.\tools\build.ps1 -Target 3ds
.\tools\build.ps1 -Target 3ds -Run     # builds, then opens the .3dsx in Azahar
```

Under the hood:

- **Host:** native CMake 4.4 + MinGW GCC 16.2 + Ninja + GLFW.
- **3DS:** MSYS2-native CMake + Ninja + devkitPro's official `$DEVKITPRO/cmake/3DS.cmake`,
  which supplies the arch flags, `3dsx.specs`, libctru includes/libs and the
  `ctr_create_3dsx()` packaging helper. devkitPro's toolchain refuses to run under native Windows
  CMake, so 3DS builds are driven through `bash`.
- Outputs: `build/desktop/.../tetris3d.exe`, `build/3ds/projects/demo_project/tetris3d.3dsx`.

Inspect or round-trip a scene on the host:

```powershell
.\build\desktop\tools\scene_dump.exe .\projects\demo_project\scenes\main.scene.json out.json
```

## Toolchain (installed locally, no admin)

| Component | Version | Location |
| --- | --- | --- |
| MSYS2 (portable) | rolling | `C:\Users\leo\dev\msys64` |
| devkitARM | r68 (GCC 16.1) | `...\opt\devkitpro\devkitARM` |
| libctru / Citro3D / Citro2D | 2.7.1 / 1.7.1 / 1.7.0 | `...\opt\devkitpro\libctru` |
| 3dstools (`3dsxtool`, `tex3ds`, `3dslink`) + `picasso` | 1.3.1 | `...\opt\devkitpro\tools\bin` |
| MinGW-w64 GCC / Ninja / GLFW | 16.2 / 1.13.2 / 3.5.1 | `...\msys64\mingw64` |
| Azahar (portable) | 2126.2 | `C:\Users\leo\dev\azahar` |

## Decisions taken

| Topic | Decision |
| --- | --- |
| Language | C++20 for engine, editor, and project scripts |
| Editor | Dear ImGui on desktop (not C#/Avalonia) |
| Scripts | Compiled C++ component classes registered by name. No interpreter |
| Data | JSON for scenes/materials/prefabs; binary for meshes and textures |
| Rendering | One engine render list, two backends: OpenGL (desktop), Citro3D (device) |
| Physics | **Not provided.** Engine exposes math, transforms, input, draw and audio; projects implement gameplay/collision |
| Audio | In the MVP (NDSP on device, miniaudio on desktop) |
| Test target | Azahar emulator first; real hardware later |
| Build | CMake + Ninja, cross-compiled with devkitARM; `.3dsx` output |

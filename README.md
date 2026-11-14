# 3ds-engine

A small, data-driven game runtime for the Nintendo 3DS / New 3DS, plus a desktop editor.
Working title; nothing here is stable yet.

Current state: **design + toolchain bring-up**. No playable build yet.

## Layout

| Path | Purpose |
| --- | --- |
| `engine/` | Portable C++20 core: scene, reflection, assets, render abstraction, audio, loop |
| `engine/platform/3ds/` | libctru + Citro3D + NDSP backend |
| `engine/platform/desktop/` | GLFW + OpenGL 3.3 + miniaudio backend (editor preview / fast iteration) |
| `editor/` | Dear ImGui desktop editor (links the same engine core) |
| `projects/demo_project/` | Demo project: manifest, scenes, assets, C++ scripts |
| `tools/` | Host-side converters (`meshc`, `fontc`), packaging and run scripts |
| `third_party/` | Vendored single-header / small libraries |
| `tests/` | Host-side unit tests for platform-independent code |
| `docs/` | Assessment, architecture, roadmap |

## Docs

- `docs/00-assessment.md` — feasibility review of `prelim_design.md`, scope cuts, risks
- `docs/01-architecture.md` — proposed engine design, formats, APIs
- `docs/02-roadmap.md` — milestones with acceptance criteria
- `prelim_design.md` — original notes, kept for history

## Decisions already taken

| Topic | Decision |
| --- | --- |
| Language | C++20 for engine, editor, and project scripts |
| Editor | Dear ImGui on desktop (not C#/Avalonia) |
| Scripts | Compiled C++ component classes, registered by name. No interpreter |
| Data | JSON for scenes/materials/prefabs; binary for meshes and textures |
| Rendering | One engine render list, two backends: OpenGL (desktop), Citro3D (device) |
| Physics | **Not provided.** Engine exposes math, transforms, input, draw and audio; projects implement gameplay/collision |
| Audio | Included in MVP (NDSP on device, miniaudio on desktop) |
| Test target | Azahar emulator first; real hardware later |
| Build | CMake + Ninja, cross-compiled with devkitARM; `.3dsx` output |

## Toolchain

Installed locally (no admin) at `C:\Users\leo\dev\msys64`:

- MSYS2 portable + devkitPro pacman repositories
- `3ds-dev`: devkitARM, libctru, Citro3D, Citro2D, 3dstools (3dsxtool, 3dslink, tex3ds), picasso
- MinGW-w64 GCC, CMake, Ninja, GLFW for host builds

Environment is set by `tools/env.ps1`; CMake presets are in `CMakePresets.json`.

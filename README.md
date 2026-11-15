# Lumika

A small, barebones, data-driven game runtime for the Nintendo 3DS / New 3DS, with a desktop editor.
Working title; nothing here is stable yet.

**Status: editor MVP running.** The Tetris demo is playable and the desktop ImGui editor can open the
project, browse the tagged asset index, edit the hierarchy, inspect components, create/instantiate
prefabs, save the scene, and build/launch Azahar.

## Editor

```powershell
.\tools\build.ps1 -Target desktop
.\build\desktop\editor\lumika_editor.exe
```

Unity-style docked panels: a live **Scene** viewport (free-fly camera, ground grid, click selection,
translate gizmos, camera frustum gizmos), a **Game** tab with Top/Bottom camera previews, Hierarchy,
Inspector, a tagged/fuzzy-search **Assets** index, and Console. `F5` plays the scene in the editor
with scripts running (dynamic meshes such as the board update live); `Build -> Playtest` compiles the
3DS target and launches Azahar. See `docs/10-getting-started.md` §4b.

## Audio

Homebrew NDSP needs a `dspfirm.cdc` on the SD card:

- **Azahar**: create a **0-byte** file at `%APPDATA%\azahar\sdmc\3ds\dspfirm.cdc`. HLE audio only
  requires the file to exist.
- **Hardware**: dump it with Luma3DS (Rosalina → Miscellaneous options → Dump DSP firmware) to
  `sdmc:/3ds/dspfirm.cdc`, or use [OpenDSP](https://github.com/Cellenseres/OpenDSP) (BSD-2) renamed
  to `dspfirm.cdc`.

Without it `ndspInit()` fails with `0xD880A7FA` and the engine runs silently.

## Controls

| Input | Action |
| --- | --- |
| D-pad / Circle Pad | Move |
| Down | Soft drop |
| Up | Hard drop |
| A / B | Rotate clockwise / counter-clockwise |
| Start | Pause |
| Select | Quit |

## Verified so far

| Check | Result |
| --- | --- |
| 3DS build | `tetris3d`, `scene_check`, `render_check`, `demo3d` via devkitARM r68, libctru 2.7.1 |
| 3DS run | Citro2D smoke app; `scene_check` round-trips scenes; `render_check` lit textured cube |
| Tetris | Scene + manifest + 4 scripts; 1 piece locked, score 34, 4 board cells, 60 FPS |
| HUD | Bitmap-font text on both screens, additive blending, upright glyphs |
| Host tests | 41 test cases / 314 assertions pass via CTest |
| Audio | NDSP works in Azahar with a 0-byte `dspfirm.cdc`; clip, voice and playback verified |

## What works now

- 128-bit GUIDs, generation-checked entity ids, JSON parser/writer, bump arena, Vec/Mat4/Quat math.
- Component registry with typed properties (`E3D_COMPONENT` / `E3D_PROP`), no code generation.
- `World`: entity hierarchy, cycle-safe reparenting, transform composition, component ownership.
- Scene load/save: two-pass entity references, asset GUIDs, unknown data preservation, deterministic
  output.
- Asset pipeline: `meshc` (OBJ → `.cmesh`), `tex3ds` (PNG → `.t3x`), manifest, material JSON, RomFS.
- Renderer: render list, Citro3D device, two targets, per-camera perspective, depth, layer masks,
  `unlit_color` and `lit_textured` shaders, textures, mesh upload with handle pools.
- Runtime: script lifecycle, fixed 60 Hz step, portable input state, audio API, deterministic dispatch.
- Project scripts are plain C++ components compiled into the game; the scene references them by type.
- 2D HUD: bitmap font atlas, `draw_text`, per-screen sprite batching with additive blending.
- Tetris: 7-bag, rotation with kicks, DAS, soft/hard drop, lock delay, line clears, scoring, pause and
  restart; board and next piece are dynamic meshes; HUD shows score, level and lines.

## Layout

| Path | Purpose | Status |
| --- | --- | --- |
| `engine/` | Core: scene, reflection, assets, render list, runtime, audio API | M3 |
| `engine/platform/3ds/` | Citro3D device, NDSP audio, HID, RomFS | M3 |
| `engine/platform/desktop/` | GLFW + OpenGL 3.3 + miniaudio backend | planned |
| `editor/` | Dear ImGui desktop editor | M6 MVP |
| `projects/demo_project/` | Scene, assets, scripts, playable Tetris demo | M5 |
| `tools/` | `scene_dump`, `meshc`, `cook.ps1`, build/run scripts | M2 |
| `third_party/` | Vendored small libraries (doctest now; ImGui, cgltf, miniaudio later) | M1 |
| `tests/` | Host-side unit tests | M1 |
| `docs/` | Assessment, architecture, roadmap | done |

## Docs

- `docs/00-assessment.md` — feasibility review of `prelim_design.md`, scope cuts, risks
- `docs/01-architecture.md` — engine design, formats, APIs
- `docs/02-roadmap.md` — milestones with acceptance criteria
- `docs/10-getting-started.md` — toolchain, build, project creation, assets
- `docs/11-engine-manual.md` — runtime API, scripts, rendering, audio
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

Note: the RomFS staging is copied at configure time. After changing cooked assets, run
`tools\cook.ps1` and re-run the build; if a `.3dsx` still embeds the old assets, delete it or clean
`build/3ds`.

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

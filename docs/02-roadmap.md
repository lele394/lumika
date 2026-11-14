# Roadmap

The sequence is deliberate: **toolchain → data → rendering → scripts → game → editor**. The editor is
last because it can only be as good as the data model it edits, and the data model can only be judged
by a running game.

Estimates assume one developer, part-time, with heavy AI assistance. They exclude research spikes.
Each milestone ends in something demonstrable — no milestone is "infrastructure only".

| # | Milestone | Outcome | Estimate |
| --- | --- | --- | --- |
| M0 | Toolchain + skeleton | `.3dsx` boots in Azahar, host tests run | 1-2 d |
| M1 | Core data model | Scene JSON round-trips, hierarchy works, tests green | 1 wk |
| M2 | Renderer + assets | Lit, textured meshes on both screens | 1.5-2 wk |
| M3 | Scripts, input, time, audio | Script-driven behaviour + sound on both backends | 1 wk |
| M4 | Tetris demo | Playable game, packaged, 60 fps | 1-1.5 wk |
| M5 | Editor MVP | Author the demo scene without hand-editing JSON | 2 wk |
| M6 | Later | Prefabs, packing, streaming, physics primitives, overlays | — |

## M0 — Toolchain and skeleton

**Status: done and verified.**

**Goal:** prove that the machine can build and run something on both targets, and that the repository
layout and build commands are settled.

Tasks:

- [x] `git init`, `.gitignore` (`build/`, `romfs/`, `*.3dsx`, `*.elf`, `.cache/`).
- [x] 3DS toolchain: devkitPro portable MSYS2 + `3ds-dev`, official `3DS.cmake`, `3dsx.specs`
      supplied by the platform module.
- [x] `CMakePresets.json`: `3ds` and `desktop` presets, Ninja generator.
- [x] `tools/env.ps1`, `tools/build.ps1`, `tools/run_azahar.ps1`.
- [x] Packaging rule: `ctr_create_3dsx()` → SMDH + `.3dsx`.
- [x] 3DS smoke app: Citro2D triangle on the top screen, animated square on the bottom, Start exits.
- [x] Desktop smoke app: GLFW window with clear colour.
- [x] `tests/` with doctest, wired into CTest.

**Acceptance:** `cmake --preset 3ds && cmake --build --preset 3ds` produces a `.3dsx` that boots in
Azahar and shows the triangle; `ctest --preset desktop` passes; a clean checkout builds with two
commands; builds are warning-clean under `-Wall -Wextra`.

**Verified:** `.3dsx` 150 KB, boots in Azahar 2126.2 at 60 FPS (3.5 ms frame), both screens render;
host tests pass. Note: the official devkitPro CMake toolchain only runs under MSYS2's cmake, so the
3DS preset is driven through `tools/build.ps1` → bash.

## M1 — Core data model

**Status: done and verified (host + Azahar).**

**Goal:** a scene can be created in code, saved, reloaded, and inspected without a renderer.

Tasks:

- [x] `core/types.hpp`: fixed-width types, `Guid` (128-bit), `EntityId` (index + generation),
      handle template, `E3D_ASSERT`, `E3D_LOG`.
- [x] `core/arena.hpp` bump allocator; `core/guid` format/parse/generate; `core/math` Vec/Mat4/Quat.
- [x] `core/json`: parser, deterministic writer, preserved object order, no exceptions.
- [x] `reflect/registry`: `ComponentDesc`, `PropDesc`, `E3D_COMPONENT`, `E3D_PROP`, registration.
- [x] Property codecs: bool, int, float, vec2/3/4, colour, enum, string, asset ref, entity ref.
- [x] `scene/world`: entity pool with generations, hierarchy, transforms, component ownership,
      deterministic order, cycle-safe reparenting.
- [x] Scene load/save with a two-pass reference resolver, unknown-key preservation, unknown-component
      preservation, path-accurate errors.
- [x] `tools/scene_dump` host utility: load, print tree, save.
- [x] Tests: JSON, GUID, math, registry, property codecs, world, scene round-trip, dangling refs,
      defaults, flattened props, malformed documents.
- [x] Device check: `projects/demo_project/src/scene_check_3ds.cpp` loads a scene on ARM and writes a
      report to the SD card.

**Acceptance:** two-pass save of a loaded scene is byte-identical to the first save; a component
property changed through its `PropDesc` appears in the output; references resolve in a second pass;
`engine/` has no host-only includes.

**Verified:** 32 test cases / 265 assertions pass. `scene_dump` round-trips `main.scene.json` with
identical output on the second pass. `scene_check.3dsx` in Azahar reports:
`entities=2, target_resolved=1, legacy_preserved=1, asset_preserved=1, child_world_y_milli=2000,
reload_ok=1, roundtrip=identical, warnings=0`.

**Deferred within M1:** `requires` validation, top-level unknown-key preservation (component-level is
done), dirty-subtree transform caching, and a non-linear GUID index. None are needed by the demo.

## M2 — Renderer and asset pipeline

**Goal:** a scene file with meshes, materials and cameras renders correctly on the device.

Tasks:

- [ ] Citro3D device: init/shutdown, command buffer, both render targets, viewport, clear, depth.
- [ ] Shader library via `picasso`: `unlit_color`, `unlit_textured`, `lit_diffuse`, `lit_textured`,
      plus `sprite` and `text`. Fixed attribute/uniform layout shared by all programs.
- [ ] Mesh upload (`linearAlloc`, cache flush), textures from `.t3x`, material JSON.
- [ ] Render list, camera component, `MeshRenderer`, sorting, dynamic VBO triple buffering.
- [ ] `tools/meshc`: OBJ first, glTF via `cgltf`; AABB; u16 indices; validation.
- [ ] `tools/fontc`: TTF → atlas + metrics; 2D sprite/text batcher.
- [ ] Debug overlay: frame time, draw calls, triangles, heap high-water.
- [ ] Optional: stereo camera path behind a flag.

**Acceptance:** a scene with a textured, lit cube on the top screen and a second camera on the bottom
runs at 60 fps in Azahar; changing the clear colour in JSON changes the frame; the debug overlay can
be toggled; the update/render loop performs **zero heap allocations** (counter measured across 600
frames).

**Risk checkpoint:** if shader work overruns, cut to `unlit_textured` + `sprite` and defer lighting to
M3. Do not cut the asset pipeline — it is on the critical path for the demo.

## M3 — Scripts, input, time, audio

**Goal:** project code drives the engine; the engine stops being a scene viewer.

Tasks:

- [ ] `ScriptComponent` lifecycle and dispatch order; enable/disable; destroy deferral.
- [ ] Script property serialization; script components attachable from JSON like any other.
- [ ] `InputState`: buttons, circle pad, C-stick, touch; desktop mapping; `raw_hid` passthrough.
- [ ] Time: variable `dt` (clamped), fixed 60 Hz accumulator, `time_scale`, pause.
- [ ] `AudioDevice`: NDSP backend (8 voices, PCM16), miniaudio desktop backend, WAV loader,
      play/stop/loop/volume/pan/rate, master volume.
- [ ] Raw draw API on `RenderList` for project-generated geometry (the escape hatch).
- [ ] Script-visible context: `World&`, `InputState`, `AudioDevice&`, asset handles.

**Acceptance:** two scripts observe a deterministic order; a script reads input and issues raw
triangle draws; the same WAV plays, loops and stops on both backends; pausing stops simulation but not
audio; no allocation in `on_update` for the demo scripts.

## M4 — Tetris demo

**Goal:** the game that defines the engine. If a feature is not needed here, it does not belong in the
MVP.

### Demo specification (2.5D board, 3D presentation)

- **Top screen (400×240, stereo optional):** 10×20 board of 1×1×1 cubes, angled camera, depth-tested;
  background grid; ghost piece; line-clear flash; hard-drop shake.
- **Bottom screen (320×240):** next tetromino as 4 cubes rotating around Y; HUD with score, level,
  lines; control hint.
- **Input:** D-pad/Circle Pad left/right move, down soft drop, A rotate CW, B rotate CCW, Up hard drop,
  Start pause, HOME quits.
- **Rules:** 7-bag randomiser, gravity table by level, 0.5 s lock delay, line clears, scoring
  (100/300/500/800 × level), game over and restart.
- **Audio:** move, rotate, lock, clear, game-over (short WAVs, fully resident).

### Structure

| File | Type | Responsibility |
| --- | --- | --- |
| `scripts/tetris_game.cpp` | `TetrisGame` | board state, spawning, gravity, clears, score |
| `scripts/board_renderer.cpp` | `BoardRenderer` | rebuilds one dynamic mesh from the board model |
| `scripts/next_piece.cpp` | `NextPieceView` | bottom-screen camera, rotation animation |
| `scripts/hud.cpp` | `Hud` | text layout, score display |
| `scenes/main.scene.json` | data | root env, two cameras, board, next-piece, HUD entities |
| `assets/…` | data | cube mesh, block/material, font atlas, 5 WAVs |

### Acceptance

- [ ] Playable start to game over to restart, with correct scoring.
- [ ] 60 fps top screen with stereo off; ≥ 30 fps with stereo on (Azahar).
- [ ] Top screen ≤ 4 draw calls, bottom ≤ 6; board is one indexed draw.
- [ ] Heap high-water ≤ 16 MB and flat over a 30-minute soak.
- [ ] Audio triggers correctly and survives scene restart.
- [ ] Builds from a clean checkout and packages with assets into RomFS.

**Gate:** the editor does not start until M4 is playable.

## M5 — Editor MVP

**Goal:** author and run the demo without touching JSON by hand.

- [ ] ImGui docking shell, saved layout, console log sink.
- [ ] Project open/recent; asset scan with `.meta` GUIDs.
- [ ] Hierarchy: create, rename, delete, reparent, multi-select.
- [ ] Inspector: schema-driven widgets from `PropDesc`, asset pickers, add/remove components, defaults.
- [ ] Assets: browse, create material, import mesh (runs `meshc`), reveal GUID.
- [ ] Save with unknown-key preservation; undo/redo command stack.
- [ ] Build & Run: Ninja target + Azahar; Play in editor (desktop backend).
- [ ] **M5b (optional):** scene viewport, orbit camera, translate/rotate gizmos, picking.

**Acceptance:** the demo scene can be recreated from an empty project in the editor; a script property
edited in the inspector changes gameplay; save/load is lossless; Build & Run reaches the emulator in
one click; the editor and the runtime agree on every property (same descriptor code).

## M6 — Later, in likely order

1. Prefabs (runtime instantiation first, editor UI later).
2. Cooked asset pack (single file, alignment, optional LZ4) + async load.
3. Audio streaming/ADPCM; music.
4. Collision and physics **primitives** (AABB/sphere/OBB overlap, ray tests, sweep) with no solver —
   enough for projects to build their own.
5. On-device ImGui debug overlay (custom Citro3D backend) and a frame-time graph.
6. Animation (keyframe clips, skinning only if a game needs it).
7. Scene viewport and gizmos if not already done.
8. `.cia` packaging, save data, CI on GitHub Actions (host tests + 3ds build artifact).

## Verification strategy

| Layer | How | When |
| --- | --- | --- |
| Engine core | doctest, host, milliseconds | every change |
| Serialization | round-trip + golden files | every change |
| Rendering | Azahar screenshots + manual checklist per milestone | per milestone |
| Performance | in-game overlay, 600-frame allocation counter, p99 frame time | M2 onward |
| Memory | boot log + high-water + 30 min soak | M4 onward |
| Hardware | smoke list (boot, both screens, audio, sleep/wake, HOME) | when a unit is available |

Cheap automation worth adding early: a `scene_dump` round-trip test in CI, and a script that boots a
`.3dsx` in Azahar for a fixed number of seconds and fails if the process exits non-zero.

## First week, concretely

1. **Day 1:** M0. Toolchain verification, presets, packaging, triangle on the top screen, host tests.
2. **Day 2:** `core/types`, `Guid`, arena, log, handle; repository conventions; `.gitignore`.
3. **Day 3:** JSON parser/writer + property codecs; registry skeleton with one test component.
4. **Day 4:** World, entity pool, hierarchy, transforms; component pools.
5. **Day 5:** Scene load/save + two-pass references; `scene_dump`; round-trip tests.

At the end of week one, a scene file can be loaded, mutated and saved deterministically. That is the
foundation everything else stands on — and it is all testable on the host, without the emulator.

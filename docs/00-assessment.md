# Assessment of the preliminary design

Reviewed: `prelim_design.md` (v1). Target: Nintendo 3DS / New 3DS, devkitPro toolchain.
Verdict written before any code exists, so it is a plan review, not a code review.

## 1. Verdict

| Question | Answer |
| --- | --- |
| Is a data-driven engine of this shape possible on 3DS? | Yes. libctru/Citro3D are mature and the hardware is well documented. |
| Is the full document feasible as one project? | No, not in a useful timeframe. It describes an engine **and** a Unity-class editor **and** five gameplay subsystems. |
| Is a barebones version feasible? | Yes. Roughly 5-6 weeks part-time to a playable demo in an emulator, including renderer, reflection, assets, audio and a minimal editor. |
| Biggest technical risk | PICA200 shader model (no fragment shader programs) and the asset pipeline, not the scene graph. |
| Biggest project risk | Editor scope creep before the game is playable. |

The single most important recommendation: **make the demo game the specification**. Every subsystem
should be justified by something the 3D Tetris demo actually does. Everything else is a later
milestone, not an MVP feature.

## 2. What the document gets right

- **Component/GameObject model.** A GameObject as an ID + transform + component list + children is
  the right primitive. Keeping "scene root" as an ordinary object is also right.
- **Data first.** Building serialization and the object model before the renderer is correct, and it
  is what makes an editor possible at all.
- **JSON as the editable source format, binary for runtime assets.** JSON is good for humans and
  diffs; meshes and textures must be pre-baked for the GPU.
- **Stable GUID references** instead of name-based lookup. Essential once an editor can rename things.
- **Component schemas drive the inspector.** This is the property that makes a small engine feel like
  a real tool.
- **3DS is the primary target**, desktop exists to accelerate development.
- **Separate `game_engine/` from `projects/`** (this review keeps that split, as `engine/` and
  `projects/`).

## 3. Changes I would make

### 3.1 Add the missing foundation: a type/property registry

With C++ scripts there is no interpreter, so the document's two concepts — "components" and
"scripts" — collapse into one: **a registry of named component types with described properties**.
That registry is what JSON loading, script instantiation, the editor inspector, and prefab overrides
all read from. It does not appear in the preliminary design, and everything depends on it.

Concretely, a component type is described by:

- a stable string name (`"TetrisGame"`), hashed to a type id (FNV-1a) so the runtime never compares strings;
- a factory (`Component* (*)(Entity&)`) and destructor;
- a list of properties: name, value type, getter/setter, JSON conversion, and editor hints
  (range, step, tooltip, asset filter);
- optional `requires` list for validation.

This is ~400 lines and it removes the need for code generation, a C# binding layer, or reflection
hacks. See `docs/01-architecture.md` §4.

### 3.2 Unify "components" and "scripts"

The document treats `Script` as a component that points at `scripts/player.lua`. With C++ scripts,
`Script` is not a component type; **every script is a component type**. `TetrisGame` is a component
whose class derives from `ScriptComponent`. This removes a whole indirection layer (`self`,
`get_component`, script binding tables) from the scripting API, and makes scripts first-class in the
editor, serialization and prefabs.

### 3.3 Drop C#/Avalonia for the editor

Avalonia + a C++ core means: a C ABI or P/Invoke layer, two object models, duplicated reflection,
two build systems, and a marshal boundary in the middle of the inspector's hot path. Dear ImGui in
C++ gives one language, one core, one registry, and an inspector that is a loop over property
descriptors. It is less pretty and has no native widget set; for a game tool that is an acceptable
trade. (Decision taken: C++ + ImGui.)

### 3.4 Do not build a second renderer for the desktop

"Desktop Runtime" is the most dangerous line in the document. Two rendering paths means two
material systems, two shader sets, and two sets of visual bugs.

Instead: the engine builds a **render list** (cameras, draw items, 2D overlay commands) and the
backend executes it. Citro3D is the reference backend; an OpenGL backend exists only so the editor
can preview and so gameplay can be iterated without an emulator. The GL backend is allowed to be
imperfect and is not a shipping target.

### 3.5 Shaders are a fixed library, not an authoring feature

PICA200 has **no fragment shader programs**. The fragment stage is a fixed-function TEV combiner
network; only vertex and geometry shaders are programmable, they are assembled offline, have small
instruction budgets, and no general control flow (no loops; conditionals by predication).

Therefore "custom shaders + shader compiler" is a project of its own. The engine should ship a small
library of parameterised shader programs (unlit, vertex-lit diffuse, textured, textured+lit,
vertex-colour, 2D sprite/text) and expose *material parameters*. Custom shader authoring moves to a
later, optional milestone with a narrow, documented PICA subset.

### 3.6 Separate serialized identity from runtime identity

The document says "references use stable UUIDs". Keep that for **files and persisted objects**, but
do not use 128-bit GUIDs inside the runtime:

- serialized entities get a `Guid` (128-bit);
- at load, the world assigns a dense `EntityId` (index + generation);
- a load-time GUID → EntityId map resolves references in a second pass, so component order and
  forward references do not matter.

This is faster, validates stale references, and keeps `EntityId` small enough to store in components.

### 3.7 Small fixes to the example format

- `MeshRenderer` puts `mesh`/`material` at the component level while other components nest under
  `properties`. Pick one: `props` for everything.
- Euler rotation needs units and order stated (degrees, XYZ, extrinsic → quaternion on load).
- `"script": "scripts/player.lua"` becomes `"type": "PlayerController"` + `props`.
- Asset references should be GUIDs, not paths, or renaming a file breaks the scene.

## 4. Hardware reality check

Approximate figures; verify on device with `osGetMemRegionFree` and a frame-time overlay.

| Resource | Old 3DS | New 3DS | Consequence for the engine |
| --- | --- | --- | --- |
| CPU | 4× ARM11, 268 MHz | 4× ARM11, 804 MHz (`osSetSpeedupEnable(true)`) | 2 usable cores typical / 4 on N3DS. Treat as single-threaded + one audio/worker thread. |
| FPU | VFPv2, single precision, no NEON | same | `float`, not `double`, in hot paths. No SIMD vector library. |
| RAM | 128 MB total; title typically 64-80 MB; ~32 MB default linear heap | 256 MB total; ~124 MB+ for titles | Arenas + explicit budgets. Never load a whole texture atlas four times. |
| VRAM | 6 MB total, shared by textures and framebuffers | same | One 400×240 RGBA8 framebuffer ≈ 384 KB; stereo doubles it. Textures must be tiled. |
| GPU | PICA200 @ 133 MHz | PICA200 @ 268 MHz | No MRT, no instancing, no compute, no depth textures, no occlusion queries. |
| Shaders | VSH/GS programmable, fragment = TEV | same | Offline assembly (`picasso`). Small, fixed shader set. |
| Top screen | 400×240 per eye, 800×240 stereo panel | same | Stereo doubles GPU work; make it per-camera opt-in. |
| Bottom screen | 320×240, resistive touch | same | Second render target and second camera; touch via `hidTouchRead`. |
| Audio | NDSP, hardware channels, PCM16/ADPCM | same | Decoded clips must live in linear RAM. Keep clips small, or stream. |
| Storage | RomFS (read-only, embedded in `.3dsx`) + SD | same | Ship cooked assets in RomFS; optional SD override for iteration. |

Performance budgets for the demo: ≤ 100 draw calls, ≤ 30k triangles, ≤ 8 MB of textures,
≤ 16 MB total heap, 60 fps with stereo off. Stereo on: still 60 fps, or drop to 30.

## 5. What "barebones engine" means here

The engine provides **mechanism**, projects provide **policy**.

| In the engine (MVP) | Left to project code | Later / optional |
| --- | --- | --- |
| Math (`Vec2/3/4`, `Mat4`, `Quat`, `AABB`, `Ray`, `Plane`) | Collision detection, spatial partitioning, solvers | Collider components, physics integration |
| World, entities, hierarchy, transforms, component registry | Game rules, state machines, scoring | Animation clips/state machines |
| Reflection, JSON scenes/prefabs, stable GUIDs | Level data authored in the editor | Prefab editor UI, nested variants |
| Render list, 2 backends, fixed shader library, materials, textures, 2D overlay | Custom draw calls via the raw draw API | Shader authoring tool, post-processing |
| Input (buttons, circle pad, touch), time, fixed step | Input mapping, buffering, gestures | Replay system |
| Audio clips, channels, volume/pan/rate, looping | Music sequencing, mixers | ADPCM streaming, DSP effects |
| Logging, asserts, frame stats, heap stats | Debug overlays specific to the game | Profiler timeline |
| Asset loading (RomFS / SD / desktop dir) | Packing decisions | Compressed packs, async streaming |
| Lifecycle dispatch (`on_create/start/update/fixed/enable/destroy`) | Everything inside those hooks | Coroutines, hot reload on device |

Explicit non-goals for the MVP: editor scene viewport with gizmos, on-device ImGui, C# scripting,
network, save-game framework, localization.

## 6. Dependencies

| Dependency | Used by | Why |
| --- | --- | --- |
| devkitARM (GCC) | device builds | Cross compiler; supports C++20 |
| libctru | device | OS, HID, RomFS, NDSP, GSP |
| Citro3D | device render backend | PICA200 abstraction, shader programs, VRAM |
| Citro2D | device 2D/text | Fast sprites and text on top of Citro3D |
| picasso | build | Assembles PICA vertex/geometry shaders to `.shbin` |
| tex3ds | build | PNG → `.t3x` (tiling, ETC1/ETC1A4/RGBA8) |
| 3dsxtool / smdhtool / bannertool / makerom | packaging | `.3dsx` first, `.cia` later |
| Dear ImGui (docking) | editor | Panels, inspector, docking layout |
| GLFW + OpenGL 3.3 | desktop | Window + context + preview viewport |
| miniaudio | desktop | Audio preview parity with NDSP |
| cgltf | tools | glTF import in `meshc` (OBJ fallback for the demo) |
| doctest | tests | Host-side unit tests |
| Azahar | verification | Emulator-first testing (decided) |

## 7. Risk register

| Risk | Impact | Mitigation |
| --- | --- | --- |
| PICA200 shader work blocks rendering | High | Day-1 spike: Citro3D triangle + textured cube before architecture work. Shader library limited to 4-6 programs. |
| Asset pipeline becomes a project of its own | High | One format (`cmesh`), one importer (OBJ + cgltf), no material graph. Convert only what the demo needs. |
| Editor swallows the schedule | High | Editor is milestone 5. Until then, scenes are hand-written JSON. Timebox panels to hierarchy/inspector/assets/console. |
| Memory fragmentation / OOM | Medium | Arena for load-time data, fixed pools for runtime, heap stat overlay, budget checks in CI. |
| Emulator passes, hardware differs | Medium | Keep a hardware smoke test list; avoid emulator-specific assumptions; verify GPU state on real unit later. |
| Two render backends drift | Medium | Render list is the only contract; backend-specific features are opt-in and documented. |
| Losing iteration speed on device builds | Medium | Desktop preview backend + `3dslink`/Azahar launch from the editor. |
| C++ scripts require rebuilds | Low | Ninja incremental builds are fast; desktop hot reload can come later. |

## 8. Effort estimate

Part-time, AI-assisted, one developer, excluding research spikes:

| Milestone | Estimate |
| --- | --- |
| M0 Toolchain + skeleton + `.3dsx` on emulator | 1-2 days |
| M1 Core, reflection, JSON, scene graph | 1 week |
| M2 Renderer + asset pipeline | 1.5-2 weeks |
| M3 Scripts, input, time, audio | 1 week |
| M4 Tetris demo playable | 1-1.5 weeks |
| M5 Editor MVP | 2 weeks |
| **Total to playable demo + editor** | **6-9 weeks** |

The order matters: the demo must be playable before the editor is started. If the editor comes
first, there is no feedback on whether the data format is right.

## 9. Remaining open questions

1. Stereo 3D default for the demo: on for the top screen, off for the bottom? (Cost: 2× top-screen GPU.)
2. Board presentation: 2.5D (classic 10×20 grid rendered as 3D cubes) vs true 3D well. Recommendation:
   2.5D — same visuals, far less gameplay code, and it still exercises the 3D path.
3. Text rendering: embedded bitmap font first, or evaluate `.bcfnt`/Citro2D text early?
4. Whether the editor gets a live 3D viewport in the MVP or after the demo runs (recommendation: after).
5. Hardware availability — currently emulator-only, so budgets are provisional until a real unit is tested.

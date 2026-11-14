# Architecture (proposed)

This is the design the MVP is built against. It is deliberately smaller than `prelim_design.md`:
one engine core, one data model, two thin platform backends.

## 1. Principles

1. **One source of truth per concept.** A component's properties are declared once, in C++, and
   drive JSON loading, the inspector, prefab overrides and defaults.
2. **Mechanism in the engine, policy in the project.** The engine moves data and draws it; gameplay,
   collision and rules live in project scripts.
3. **The 3DS defines the budget.** Every abstraction must be cheap enough for a 268 MHz ARM11 and
   6 MB of VRAM. Convenience layers are only added where they cost nothing at runtime.
4. **The editor is a client of the engine, not a special case.** Same World, same serialization,
   same scripts, compiled for the host.
5. **Data is inspectable and diffable.** JSON in version control, stable GUIDs, deterministic output.

## 2. Repository layout

```
3ds-engine/
  CMakeLists.txt
  CMakePresets.json
  cmake/
    (none yet; devkitPro's 3DS.cmake is used directly)
  engine/
    include/e3d/             # public API (scripts include only this)
      e3d.hpp                # umbrella header
      core/{types,math,log,memory,guid,json}.hpp
      reflect/registry.hpp
      scene/{world,entity,component,transform}.hpp
      assets/{asset_db,mesh,material,texture,audio_clip,font}.hpp
      render/{render_list,camera,material,device}.hpp
      audio/audio_device.hpp
      input/input.hpp
      script/script_component.hpp
    src/
      core/ reflect/ scene/ assets/ render/ audio/ loop/
    platform/
      3ds/                   # libctru, Citro3D, NDSP, RomFS
      desktop/               # GLFW, OpenGL 3.3, miniaudio, filesystem
  editor/
    main.cpp
    panels/{hierarchy,inspector,assets,console,build}.cpp
    editor_state.{hpp,cpp}
  projects/demo_project/
    project.json
    assets/                  # sources + .meta sidecars
      meshes/ textures/ materials/ audio/ fonts/
    scenes/
      main.scene.json
    scripts/
      tetris_game.cpp board_renderer.cpp next_piece.cpp hud.cpp
    romfs/                   # staged output, gitignored
  tools/
    meshc/                   # glTF/OBJ -> .cmesh
    fontc/                   # TTF -> bitmap atlas + metrics
    env.ps1                  # toolchain environment
    run_azahar.ps1
    build.ps1
  third_party/
    imgui/ cgltf/ miniaudio/ doctest/
  tests/
  docs/
```

`engine/` never includes `<3ds.h>`. `engine/platform/3ds/` never includes game code. Project scripts
include `e3d/e3d.hpp` only, which keeps them host-compilable for the editor.

## 3. Layers and frame flow

```
project.json ──► AssetDb ──► World (entities, components) ──► scripts update
                                   │
                                   ▼
                              RenderList  (cameras, draws, text)
                                   │
                    ┌──────────────┴──────────────┐
              Citro3D backend                OpenGL backend
              (top + bottom screen)          (editor / preview)
```

One frame:

1. `platform::poll()` → `InputState` (buttons, circle pad, touch).
2. `World::update(dt)`: dispatch `on_update` / `on_fixed_update` in deterministic order.
3. `Renderer::build(world, list)`: cameras, MeshRenderer components, sprites, text.
4. `device->execute(list)`: backend draws both screens.
5. `platform::present()`.

No global state beyond the `Engine` object; the editor can hold two `World` instances (edit + play).

## 4. Type and property registry

This is the foundation. Everything else reads from it.

```cpp
// engine/include/e3d/reflect/registry.hpp
enum class PropType : uint8_t {
    Bool, Int, Float, Vec2, Vec3, Vec4, Color, Enum, String, AssetRef, EntityRef
};

struct PropMeta {
    float min = 0, max = 0, step = 0.01f;
    const char* tooltip = nullptr;
    const char* const* enum_names = nullptr;   // Enum, null-terminated
    const char* asset_type = nullptr;          // "mesh", "texture", ...
    bool hidden = false;
    bool read_only = false;
};

struct PropDesc {
    const char* name;
    PropType type;
    uint8_t size;                    // sizeof(member); used for enums
    void* (*get)(void* component);   // address of the member
    PropMeta meta;
};

struct ComponentDesc {
    const char* name;          // stable, serialized
    TypeId id;                 // FNV-1a of the name
    size_t size;
    ComponentKind kind;        // Engine or Script
    Component* (*create)();    // the world binds the entity afterwards
    void (*destroy)(Component*);
    const PropDesc* props;
    uint32_t prop_count;
    const char* category;      // editor grouping
};
```

Registration is order-safe; descriptors are function-local statics:

```cpp
#define E3D_COMPONENT(TYPE, BASE)                                           \
    using BaseClass = BASE;                                                 \
    static const ::e3d::ComponentDesc* static_descriptor();                 \
    static constexpr ::e3d::TypeId static_type_id() noexcept {              \
        return ::e3d::hash_name(#TYPE);                                     \
    }                                                                       \
    const ::e3d::ComponentDesc* descriptor() const override {               \
        return static_descriptor();                                         \
    }

#define E3D_IMPLEMENT_COMPONENT(TYPE) /* defines static_descriptor() */
#define E3D_REGISTER_COMPONENT(TYPE)  /* implementation + static registrar */
```

Property declaration uses a member pointer as a template argument, so there are no offsets,
no code generation and no captured lambdas:

```cpp
class TetrisGame final : public ScriptComponent {
public:
    E3D_COMPONENT(TetrisGame, ScriptComponent)

    float fall_interval = 0.8f;
    int   start_level   = 1;
    bool  hard_drop     = true;
    Vec3  spawn_offset  {0, 10, 0};
    AssetRef move_sound;

    static std::span<const PropDesc> properties() {
        static const PropDesc props[] = {
            E3D_PROP(TetrisGame, fall_interval).range(0.05f, 2.0f).step(0.01f),
            E3D_PROP(TetrisGame, start_level).range(1, 20),
            E3D_PROP(TetrisGame, hard_drop),
            E3D_PROP(TetrisGame, spawn_offset),
            E3D_PROP(TetrisGame, move_sound).asset_type("audio"),
        };
        return {props, std::size(props)};
    }
};

E3D_IMPLEMENT_COMPONENT(TetrisGame)
E3D_REGISTER_COMPONENT(TetrisGame)   // scripts self-register; built-ins explicitly
```

`E3D_PROP(TetrisGame, member)` is `make_property<TetrisGame, &TetrisGame::member>("member")`.
It deduces the member type, selects the `PropType`, and installs `member_getter`. The JSON codec
switches on `PropType` once, so adding a property costs one line and updates loading, saving,
defaults and (later) the inspector together.

One deliberate constraint: built-in components register through an explicit
`register_builtin_components()` rather than static initializers, because linkers are free to drop
object files from static libraries. Project scripts can use `E3D_REGISTER_COMPONENT` because their
translation units are always linked into the game binary.

Rules that keep this honest:

- A property is a public data member; no hidden state in the inspector.
- Property names are the serialized keys. Renaming is a format change.
- Unknown JSON keys are preserved verbatim per component (see §6) so an older runtime or a partial
  editor cannot silently delete data.
- Dependencies between components are not enforced in M1. The plan is an optional `requires` list on
  `ComponentDesc`, validated at attach time once script registration lands.

## 5. World, entities, components, transforms

```cpp
using EntityId = uint32_t;                 // index (24 bits) + generation (8 bits)
constexpr EntityId kInvalidEntity = 0;

struct Entity {
    Guid      guid;                        // serialized identity
    EntityId  id;
    EntityId  parent;
    std::string name;
    Transform transform;                   // local TRS
    Mat4      world;                       // cached
    std::vector<EntityId> children;
    std::vector<Component*> components;    // stable addresses
};
```

- `World` owns a dense entity pool with a free list; destroying an entity bumps the generation so stale
  `EntityId`s fail validation instead of aliasing.
- Each entity owns its components (`std::vector<Component*>`), created through the registry descriptor
  and destroyed through it. Pointers are stable; lookups are linear over a handful of components,
  which is the right trade at this scale.
- Transform composition is recomputed top-down on `update_transforms()` (parents before children).
  Local rotation is stored as Euler degrees and converted to a quaternion for the matrix, so authored
  values stay readable and save/load is lossless. Dirty-subtree caching is a later optimization.
- Update order is deterministic: entities in creation order, components in attachment order. Scripts
  do not influence each other's order implicitly.
- `ScriptComponent` adds the lifecycle hooks (`on_create`, `on_awake`, `on_start`, `on_enable`,
  `on_update`, `on_fixed_update`, `on_disable`, `on_destroy`) and reaches its owner through
  `Component::entity()`.
- Fixed step: 60 Hz accumulator for `on_fixed_update`; `on_update` gets the variable frame delta,
  clamped. Both are exposed to scripts; the project decides what uses which (M3).
- Raw access is first-class: `world.entity(id)->transform`, `world.find_component<Camera>(id)`,
  `entity->find_component(type_id)`. No required use of components for rendering or input.

Scripts stay portable: they see `World`, `InputState`, `RenderList`-building APIs, and assets — never
`hidKeysDown`, `C3D_*` or other platform headers.

## 6. Serialization

### Identity

- `Guid`: 128-bit random, generated by the editor or loader when an object/asset has none.
- Entities: `Guid` in JSON, dense `EntityId` in memory.
- Load is two-pass: create every entity and component from the tree, then resolve `$entity` / `$asset`
  references. Forward and cross-scene references therefore load correctly, and a dangling reference is
  a diagnosable error rather than a null pointer.
- Runtime never compares GUIDs in a loop; the map only exists during load/save.

### Scene file v1

```json
{
  "format_version": 1,
  "name": "main",
  "root": {
    "guid": "0f8fad5b-d9cb-469f-a165-70867728950e",
    "name": "Main",
    "transform": { "position": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1] },
    "components": [
      { "type": "Environment", "props": { "clear_color": [0.04, 0.05, 0.08, 1.0] } },
      { "type": "Camera", "props": {
          "screen": "top", "fov": 50, "near": 0.1, "far": 100,
          "target": "bottom", "stereo": true
      } }
    ],
    "children": [
      {
        "guid": "5c1f2b6e-3a84-4d1e-9b2f-1f7c2a8d9e31",
        "name": "Board",
        "transform": { "position": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1] },
        "components": [
          { "type": "MeshRenderer", "props": {
              "mesh": { "$asset": "9a0b...cmesh-guid" },
              "material": { "$asset": "b41e...mat-guid" }
          } },
          { "type": "BoardRenderer", "props": { "width": 10, "height": 20, "depth": 1 } }
        ],
        "children": []
      }
    ]
  }
}
```

- Rotation is Euler degrees, XYZ order, applied extrinsically; stored as a quaternion at runtime.
- Component `type` is the registered name; property keys are the C++ member names.
- Asset refs are `{"$asset": "<guid>"}`; entity refs are `{"$entity": "<guid>"}`.
- Unknown keys inside `props` are kept in a per-component `JsonValue raw` and written back on save.
- Writer output is deterministic: fixed key order, 2-space indent, `%.6g` floats, GUIDs lowercase.

### Prefabs

Format reserved, implementation deferred (M6): a node may carry

```json
{ "$prefab": "<guid of .prefab.json>", "overrides": { "transform": { ... }, "TetrisGame": { "start_level": 3 } } }
```

The runtime instantiates the prefab subtree and applies overrides. Nothing else in the format needs
to change later.

### Project manifest

```json
{
  "format_version": 1,
  "name": "Tetris3D",
  "id": "com.example.tetris3d",
  "title": "Tetris 3D",
  "version": "0.1.0",
  "start_scene": "scenes/main.scene.json",
  "asset_root": "assets",
  "scripts": ["scripts"],
  "targets": {
    "3ds": { "icon": "assets/icon.png", "stereo": true, "romfs": "romfs" }
  }
}
```

## 7. Assets

- `AssetDb` scans `assets/`, reads `*.meta` sidecars, builds `Guid → path` and `path → Guid`.
- `.meta` files are tiny: `{ "guid": "...", "importer": "mesh", "version": 1 }`. They are committed.
- Loaders are synchronous in the MVP; loads happen at scene load, not per frame.
- Handles are generation-checked indices; a stale handle asserts in debug and no-ops in release.

| Format | Contents | Produced by |
| --- | --- | --- |
| `.cmesh` | interleaved vertices + u16 indices + AABB + flags | `tools/meshc` |
| `.t3x` | tiled texture, RGBA8 / ETC1 / ETC1A4 | `tex3ds` wrapped by the build |
| `.mat.json` | shader name, uniforms, texture slots | hand / editor |
| `.scene.json` | entity tree | hand / editor |
| `.wav` | PCM16 mono/stereo | asset |
| `.font` + `.png` | bitmap atlas + glyph metrics | `tools/fontc` |

`cmesh` v1 header:

```
u32 magic 'E3DM'   u32 version=1
u32 vertex_count   u32 index_count
u32 vertex_stride  u32 flags        // bit0 normal, bit1 uv, bit2 color
f32 aabb_min[3]    f32 aabb_max[3]
u32 reserved[2]
vertices...        // pos f32x3; normal f32x3; uv f32x2; color u8x4 (optional)
indices...         // u16, 1 draw call per file
```

Packed normals and 16-bit positions are a later optimisation; correctness first.

## 8. Rendering

The engine emits a **render list**; backends only execute it. That is the whole portability story.

```cpp
struct CameraDesc {
    Mat4 view, projection;
    Rect viewport;
    Screen screen;          // Top | Bottom
    bool  stereo;
    float eye_separation;   // metres, top screen only
    Color clear;
    bool  clear_depth;
};

struct DrawItem {
    MeshHandle mesh;
    MaterialHandle material;
    Mat4 transform;
    uint8_t layer;          // painter's order within a camera
};

class RenderList {
public:
    CameraHandle add_camera(const CameraDesc&);
    void draw_mesh(CameraHandle, MeshHandle, MaterialHandle, const Mat4&);
    void draw_text(CameraHandle, FontHandle, Vec2 pos, float scale, Color, std::string_view);
    void draw_rect(CameraHandle, Rect, Color);
    void draw_custom(CameraHandle, const CustomDraw&);   // raw escape hatch
};
```

- `Renderer::build(world, list, frame_arena)` walks cameras and `MeshRenderer` components, and lets
  scripts append commands (raw API, §5).
- The list is allocated from a per-frame arena; no allocation churn.
- Draw items are sorted by `(material, mesh, layer)` to cut state changes.
- `destroy()`/`create()` of GPU resources is deferred to the backend, never inline in a draw.
- Two cameras is the normal case: one for the top screen, one for the bottom. A camera declares its
  screen and viewport; the backend configures the render target accordingly.
- Stereo is per-camera and opt-in. It renders the camera twice with a horizontal eye offset and
  requires `gfxSet3D(true)`. Cost is roughly 2× GPU for that camera.

Device interface:

```cpp
class RenderDevice {
public:
    virtual ~RenderDevice() = default;
    virtual MeshHandle     create_mesh(const MeshData&) = 0;
    virtual TextureHandle  create_texture(const TextureData&) = 0;
    virtual MaterialHandle create_material(const MaterialDesc&) = 0;
    virtual void destroy(Handle) = 0;
    virtual void execute(const RenderList&) = 0;
};
```

Shader library (MVP): `unlit_color`, `unlit_textured`, `lit_diffuse`, `lit_textured`, `sprite`, `text`.
Each is a `picasso` output `.shbin` plus a fixed attribute/uniform layout; materials choose one and set
parameters. No material graph, no user shader editing.

### 3DS backend notes

- `gfxInitDefault()` / `gfxSet3D(true)` for stereo; `C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)`.
- Render each screen with `C3D_FrameDrawOn(target)`; set viewport and projection per camera.
- Matrices through `C3D_FVUnifMtxNx4`; keep the uniform layout identical across shaders.
- Vertex data in `linearAlloc`'d buffers, flushed with `GSPGPU_FlushDataCache` after writes.
- Textures via `C3D_TexInit`/`C3D_TexUpload`; `t3x` already tiled by `tex3ds`.
- Dynamic geometry (the Tetris board) uses a triple-buffered VBO updated per frame, so the GPU is never
  reading the buffer being written.
- Batching target for the demo: the entire board as **one** indexed draw, not 200 cubes.
- 2D/text via Citro2D on the bottom screen when it is cheaper than custom quads; the text path is
  wrapped behind the render list either way.

### Desktop backend notes

- GLFW + OpenGL 3.3 core, ImGui's GL3 backend for the editor.
- Preview window shows both screens side by side (top 400×240, bottom 320×240, integer-scaled).
- It is a development tool: feature parity is best-effort, and the device is the reference.

## 9. Audio

```cpp
struct PlayParams { float volume = 1.0f, pan = 0.0f, rate = 1.0f; bool loop = false; };
using VoiceHandle = uint32_t;

class AudioDevice {
public:
    virtual VoiceHandle play(ClipHandle, const PlayParams&) = 0;
    virtual void stop(VoiceHandle) = 0;
    virtual void set_params(VoiceHandle, const PlayParams&) = 0;
    virtual bool is_playing(VoiceHandle) const = 0;
    virtual void set_master_volume(float) = 0;
    virtual void update() = 0;
};
```

- Device: `ndspInit()`, fixed pool of voices (start with 8), PCM16, 32000 Hz stereo output; clips are
  uploaded to `linearAlloc` memory and referenced with `ndspChnWaveBufAdd`.
- Desktop: miniaudio with the same semantics, so behaviour matches closely enough to author against.
- `AudioSource` is a thin component over the raw API; scripts can call the device directly.
- Music streaming, ADPCM and DSP effects are later. MVP clips are short and fully resident.

## 10. Input and time

```cpp
enum Button : uint32_t {
    BtnA, BtnB, BtnX, BtnY, BtnL, BtnR, BtnStart, BtnSelect,
    BtnUp, BtnDown, BtnLeft, BtnRight,
    BtnZL, BtnZR, BtnTouch, BtnCStickRight /* ... */
};

struct InputState {
    uint32_t held, pressed, released;
    Vec2 circle_pad;     // -1..1, deadzone applied? no — raw, project decides
    Vec2 c_stick;
    bool touch_down;  Vec2 touch;  // pixels, bottom screen
    uint32_t raw_hid;             // 3DS bitmask, 0 on desktop
};
```

- The 3DS layer fills this from `hidKeysHeld/Down/Up`, `hidCircleRead`, `hidCstickRead`, `hidTouchRead`.
- Desktop maps keyboard/gamepad to the same buttons; `raw_hid` stays 0.
- Deadzones, repeat rates and buffering are project policy, not engine policy.
- Time: `dt` clamped to 100 ms, `time_scale`, `frame_index`, fixed step 1/60 s, `elapsed`.
- Pause/sleep: `aptMainLoop()` + applet hooks on device; `glfwWaitEvents` when unfocused on desktop.

## 11. Platform layer

```cpp
namespace e3d::platform {
    bool init(const Config&);
    void poll();
    bool should_quit();
    void present();
    void shutdown();
    InputState input();
    double now();
    void log(LogLevel, const char* fmt, ...);
    const AssetSource& assets();   // RomFS, SD override, or desktop directory
}
```

- 3DS: entry point owns `C3D_FrameBegin/End`; `osSetSpeedupEnable(true)` on New 3DS; HOME/sleep
  handling; RomFS mounted at boot; optional `sdmc:/3ds/<id>/` override for asset iteration.
- Desktop: GLFW window and GL context, filesystem-backed assets, miniaudio init.
- The engine's `main` is provided by the platform, so project code has no `main()`.

## 12. Editor

- Dear ImGui (docking branch), GLFW + OpenGL3 backends.
- Panels: Hierarchy, Inspector, Assets, Console, Build/Run. Scene viewport and gizmos come after the
  demo runs (M5b).
- Inspector is generated: iterate `ComponentDesc.props`, switch on `PropType`, call the matching
  ImGui widget with `PropMeta` hints. Adding a property updates the UI automatically.
- The editor links the **same project scripts** compiled for the host. It therefore needs no separate
  schema file and no code generation; opening a project cannot disagree with the runtime.
- Undo/redo: property-level command stack (before/after values), not full scene snapshots.
- Save preserves unknown JSON keys and formatting conventions (§6).
- Build & Run: invokes the CMake/Ninja target, then launches Azahar (or `3dslink` on hardware) with the
  resulting `.3dsx`.
- Play mode: an in-process second `World` on the desktop backend. Fast iteration; Azahar and hardware
  remain the truth.

## 13. Build, packaging, tooling

- CMake + Ninja, two configure presets: `3ds` (cross) and `desktop`/`tests` (host).
- 3DS builds use devkitPro's official platform module, `$DEVKITPRO/cmake/3DS.cmake`, via MSYS2-native
  CMake. It supplies `CMAKE_SYSTEM_NAME=Nintendo3DS`, arch flags (`-march=armv6k -mtune=mpcore
  -mfloat-abi=hard -mtp=soft`), `-mword-relocations`, `-specs=3dsx.specs`, libctru includes and
  standard libraries.
- Packaging uses the module helpers instead of hand-written rules:
  - `ctr_create_3dsx(target)` → SMDH + `.3dsx` (adds `--romfs=` later);
  - `ctr_add_shader_library(target)` → `picasso` → `.shbin`;
  - `ctr_add_graphics_target(target image|atlas ...)` → `tex3ds` → `.t3x`;
  - `dkp_add_asset_target` / `dkp_install_assets` → staged RomFS.
- Host builds use native Windows CMake + MinGW-w64 + Ninja; the editor uses the same CMake graph with
  a `desktop` preset.
- `tools/meshc` (host C++): OBJ and glTF via `cgltf` → `.cmesh`. `tools/fontc` (host C++): TTF →
  atlas PNG + metrics JSON via `stb_truetype` (already vendored by ImGui).
- `tools/env.ps1` exports `DEVKITPRO`, `DEVKITARM`, `PATH` for host builds; `tools/build.ps1`
  wraps configure/build/test/run for both presets so the editor and CI share one path.
- `.cia` packaging (makerom + bannertool) is a later target; MVP ships `.3dsx`.

## 14. Coding rules and runtime constraints

- C++20 everywhere. `-fno-exceptions -fno-rtti` on device **and** host engine core so behaviour
  matches; the editor may use exceptions internally if a dependency forces it.
- `E3D_ASSERT` in debug, log-and-continue in release. Never `throw` across the engine.
- No iostreams on device. `E3D_LOG` writes to a ring buffer, `svcOutputDebugString`, and optionally
  `sdmc:/3ds/<id>/log.txt`.
- No heap allocation in steady-state update/render. Frame and load data use arenas.
- `float` for all gameplay math; `double` only in tooling and timekeeping.
- Avoid `std::function`, `shared_ptr`, `unordered_map` in hot paths. Use function pointers,
  generation-checked handles, and sorted arrays.
- Fixed-width integer types for anything serialized. Explicit little-endian in file formats.
- Deterministic: no dependence on hash-map iteration order, thread scheduling, or frame timing for
  logic (fixed step for simulation).
- Public headers expose handles and PODs, not STL containers, wherever practical.
- Naming: `PascalCase` types, `snake_case` functions/members, `kConstant`, `E3D_` macros.

## 15. Memory budget

| Item | Budget (demo) |
| --- | --- |
| Engine code + data | ≤ 4 MB |
| Scene/world + scripts | ≤ 1 MB |
| Meshes (all) | ≤ 2 MB |
| Textures | ≤ 8 MB |
| Audio clips | ≤ 2 MB |
| Framebuffers (top stereo + bottom) | ≈ 1.2 MB (VRAM) |
| Total heap | ≤ 16 MB, target 12 MB |

Boot log prints free FCRAM (`osGetMemRegionFree`), linear-heap size and VRAM usage. A debug overlay
shows frame time, draw calls, triangles, heap high-water mark.

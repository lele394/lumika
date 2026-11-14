# Lumika engine manual

This is the API-level companion to `docs/10-getting-started.md`. The public header for project code is
`e3d/e3d.hpp`; the engine uses the `e3d` prefix regardless of the product name.

## 1. Frame flow

```
poll input -> Runtime::update -> world.update_transforms
          -> build_render_list -> render_scripts -> RenderDevice::execute
```

`Runtime` owns time, the fixed-step accumulator, pause and the script context. The app owns the
`World`, the `RenderList` and the device.

```cpp
World world;
InputState input;
RenderList list;

Runtime runtime(world, audio.get());
runtime.set_render_list(&list);
runtime.set_render_device(render.get());

while (aptMainLoop()) {
    hidScanInput();
    poll_input_3ds(input);
    runtime.update(input, dt);
    world.update_transforms();
    build_render_list(world, list);
    render_scripts(world, list);
    render->execute(list);
}
```

## 2. World, entities and components

- `EntityId` is a dense index plus generation; stale ids fail validation.
- `Entity` holds a `Guid`, name, parent, children, local `Transform`, cached world matrix and
  components.
- Components are owned by their entity and destroyed through their descriptor.
- Iteration order is entity creation order, then component attachment order.

```cpp
EntityId e = world.create_entity("Player", parent);
world.entity(e)->transform.position = {0.0f, 1.0f, 0.0f};

Environment* environment = world.add_component<Environment>(e);
Component* any = world.add_component(e, "MeshRenderer");     // by name (data path)
MeshRenderer* renderer = world.find_component<MeshRenderer>(e);
world.remove_component(environment);
```

`Transform` stores Euler degrees (X then Y then Z) so authored data round-trips; the matrix is built
from the corresponding quaternion.

### Built-in components

| Component | Purpose |
| --- | --- |
| `Environment` | Clear colour and ambient value |
| `Camera` | Screen, FOV, near/far, stereo flag, `cull_mask` |
| `MeshRenderer` | Mesh/material asset refs, `layer`, runtime mesh/material handles |
| `AudioSource` | Audio clip asset ref, volume, loop, runtime clip handle |

## 3. Components and properties

```cpp
class Health final : public e3d::Component {
public:
    E3D_COMPONENT(Health, e3d::Component)

    float current = 100.0f;
    float maximum = 100.0f;
    bool invulnerable = false;

    static std::span<const e3d::PropDesc> properties() {
        static const e3d::PropDesc props[] = {
            E3D_PROP(Health, current).range(0.0f, 1000.0f),
            E3D_PROP(Health, maximum).range(1.0f, 1000.0f),
            E3D_PROP(Health, invulnerable),
        };
        return {props, std::size(props)};
    }
};

E3D_IMPLEMENT_COMPONENT(Health)
```

- Declare the class in a header and `E3D_IMPLEMENT_COMPONENT` in one `.cpp`.
- Register once at startup: `e3d::register_component(Health::static_descriptor())`.
- Built-ins register through `register_builtin_components()` (called by `load_scene`).
- Supported property types: `bool`, `int`, `float`, `Vec2/3/4`, `Color`, enums, `std::string`,
  `AssetRef`, `EntityRef`.
- Enums serialize by name when `enum_names` is provided:

  ```cpp
  static const char* const kNames[] = {"idle", "run", "jump", nullptr};
  E3D_PROP(State, state).enum_names(kNames)
  ```

## 4. Scripts and lifecycle

`ScriptComponent` adds hooks called by the script system:

| Hook | When |
| --- | --- |
| `on_create` | First dispatch after the scene is loaded |
| `on_start` | Before the first update (once) |
| `on_enable` / `on_disable` | When `enabled` changes |
| `on_update(dt)` | Every frame |
| `on_fixed_update(dt)` | Fixed 60 Hz steps |
| `on_render(list)` | After the render list is built (HUD/overlays) |
| `on_destroy` | Before the component is destroyed |

`Runtime` calls `initialize_scripts`, which runs `on_create`/`on_start`; the app can also call
`update_scripts`, `fixed_update_scripts`, `render_scripts` and `shutdown_scripts` directly.

### Script context

Set by `Runtime::update`:

| Accessor | Returns |
| --- | --- |
| `e3d::world()` | `World&` |
| `e3d::input()` | `const InputState&` |
| `e3d::audio()` | `AudioDevice&` |
| `e3d::render_list()` | `RenderList&` |
| `e3d::render_device()` | `RenderDevice&` (dynamic meshes) |

Scripts must only include engine headers, never platform SDK headers, so they also compile on the
host for the editor.

## 5. Input and time

```cpp
struct InputState {
    u32 held, pressed, released;
    Vec2 circle_pad;   // -1..1
    Vec2 c_stick;
    bool touch_down;
    Vec2 touch;        // pixels on the bottom screen
    u32 raw_hid;       // 3DS bitmask, 0 on desktop
};
```

`Button` covers A/B/X/Y, L/R, ZL/ZR, Start/Select, D-pad, Touch. Use `is_held`, `is_pressed`,
`is_released`. Deadzones and repeat rates are project policy.

`Runtime` clamps `dt` to 100 ms, accumulates fixed 1/60 s steps (max 4 per frame), and exposes
`set_paused`, `set_time_scale`, `elapsed`, `frame`.

## 6. Rendering

### Render list

```cpp
struct CameraDesc {
    Screen screen;                 // Top | Bottom
    Vec3 position, target, up;
    float field_of_view, near_plane, far_plane;
    Color clear;
    bool clear_depth;
    bool stereo;
    u32 cull_mask;
};

list.draw(camera_index, mesh, material, model_matrix);
list.draw_sprite(SpriteItem{...});
```

Backends build the projection themselves: Citro3D uses `Mtx_PerspTilt`, desktop will use a standard
perspective.

### Materials

```cpp
MaterialDesc material;
material.shader = ShaderId::LitTextured;   // or UnlitColor / UnlitTextured
material.tint = {1, 1, 1, 1};
material.ambient = 0.25f;
material.diffuse = 0.75f;
material.light_direction = {-0.4f, 0.8f, 0.6f};
material.texture = texture_handle;
MaterialHandle handle = device.create_material(material);
```

Available shaders: `unlit_color` (vertex colour), `lit_textured` (normals + texture), `unlit_textured`
(sprites/text). Shaders are compiled offline by `picasso` at build time; there is no runtime shader
compiler.

### Meshes

```cpp
MeshData mesh;                 // std::vector<MeshVertex> + std::vector<u16>
MeshHandle handle = device.create_mesh(mesh);
device.update_mesh(handle, new_mesh);   // rebalances only on growth
device.destroy_mesh(handle);
```

`MeshVertex` is 48 bytes: position, colour, uv, normal. The board and next piece in the Tetris demo
are dynamic meshes rebuilt from game state.

### Sprites and text

`e3d/render/text.hpp`:

```cpp
Font font;
font.texture = font_atlas_texture;
font.texture_width = 128;  font.texture_height = 128;
font.glyph_width = 8;      font.glyph_height = 12;
font.columns = 16;         font.first_char = 32;

draw_text(list, font, Screen::Bottom, 8, 8, 1.0f, Color{1,1,1,1}, "SCORE 1000");
```

Sprites are drawn after 3D using an orthographic pass. The current implementation composites
additively over a black-backed atlas; alpha-blended UI can replace it later.

## 7. Assets

- `AssetDb` loads `manifest.json`: `guid`, `type` (`mesh`, `texture`, `material`, `audio`, `font`),
  `path`.
- `AssetManager` resolves GUIDs to device handles and caches them.
- `resolve_scene_assets` walks `MeshRenderer`/`AudioSource` and fills runtime handles.
- Loaders are synchronous; do not load during `on_update` in the steady state.
- Cooked formats: `.cmesh` (see `load_cmesh`), `.t3x` (tex3ds), `.mat.json`, PCM16 `.wav`.

## 7b. Prefabs

```cpp
std::string node = save_entity_json(world, selected);          // subtree as JSON
EntityId copy = instantiate_prefab(world, node, parent, error); // fresh GUIDs, refs remapped
```

`instantiate_prefab` also accepts a full scene document with a `root` node. Internal `$entity`
references are rewritten to the new GUIDs; external references are left alone.

## 8. Audio

```cpp
ClipHandle clip = audio.load_wav(bytes.data(), bytes.size());
VoiceHandle voice = audio.play(clip, PlayParams{0.8f, 0.0f, 1.0f, false});
audio.stop(voice);
audio.set_master_volume(0.9f);
```

- 8 NDSP channels, PCM16 mono/stereo, per-voice volume/pan/rate/loop.
- `update()` reclaims finished voices; `Runtime` calls it each frame.
- Clips live in linear memory and must not be destroyed while playing.

## 9. Conventions

- Right-handed coordinates; cameras look down -Z, +Y is up.
- Top screen 400x240, bottom 320x240; bottom-screen text coordinates are pixels with a top-left
  origin.
- Serialized entity GUIDs are stable; runtime `EntityId`s are not.
- Angles in JSON are degrees.
- `cull_mask` is a 32-bit layer mask; `MeshRenderer.layer` selects the bit.

## 10. Memory and performance rules

- No heap allocation in steady-state update/render; use fixed pools and dynamic meshes.
- `-fno-exceptions -fno-rtti` on device; no `throw`, no `dynamic_cast`.
- Prefer `float`, not `double`.
- Batch by material/texture; the Tetris board is one mesh and one draw.
- Measure with the SD reports and, later, the debug overlay.

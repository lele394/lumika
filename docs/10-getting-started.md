# Getting started with Lumika

Lumika is a small, data-driven game runtime for the Nintendo 3DS / New 3DS, plus host tools. Project
content is JSON (scenes, materials, manifest) plus cooked meshes/textures, and behaviour is plain C++
components compiled into the game.

## 1. Requirements

| Tool | Purpose |
| --- | --- |
| devkitPro `3ds-dev` (devkitARM, libctru, Citro3D, Citro2D, 3dstools, picasso) | Device build |
| CMake 3.21+ and Ninja | Build system |
| MinGW-w64 GCC (host) | Unit tests and tools |
| Azahar | Emulator testing |
| Luma3DS / hardware | Optional real-device testing |

On Windows the supported setup is a portable MSYS2 under `C:\Users\leo\dev\msys64` with the devkitPro
pacman repositories added. `tools/env.ps1` points at it. Details are in the repository README.

The devkitPro CMake toolchain only runs under MSYS2's native CMake, so the 3DS preset is driven through
`tools/build.ps1` (which invokes bash). Host builds use the native CMake.

## 2. Build and run the demo

```powershell
cd C:\Users\leo\Downloads\3ds-engine
. .\tools\env.ps1

# Host tests and tools
.\tools\build.ps1 -Target desktop

# Device build -> build\3ds\projects\demo_project\demo3d.3dsx
.\tools\build.ps1 -Target 3ds

# Launch the .3dsx in Azahar
.\tools\run_azahar.ps1 -Rom .\build\3ds\projects\demo_project\demo3d.3dsx
```

`run_azahar.ps1` defaults to `tetris3d.3dsx`; pass `-Rom` to choose another app. The emulator lives at
`C:\Users\leo\dev\azahar`.

### Audio (once per machine)

Homebrew NDSP needs a DSP firmware file:

- **Azahar**: create a 0-byte file at `%APPDATA%\azahar\sdmc\3ds\dspfirm.cdc`. HLE only needs it to
  exist.
- **Hardware**: Luma3DS → Rosalina → Miscellaneous options → *Dump DSP firmware* to
  `sdmc:/3ds/dspfirm.cdc`, or use [OpenDSP](https://github.com/Cellenseres/OpenDSP) renamed to
  `dspfirm.cdc`.

Without it `ndspInit()` fails (`0xD880A7FA`) and the game runs silently.

## 3. Cook assets

Meshes and textures are converted on the host:

```powershell
.\tools\cook.ps1
```

It runs `meshc` (OBJ → `.cmesh`), `tex3ds` (PNG → `.t3x`), and copies materials, audio and
`manifest.json` into `projects/demo_project/assets/cooked/`.

The 3DS CMake stages `assets/cooked/` into RomFS at **configure** time. If a `.3dsx` still contains old
assets, delete it (or `build/3ds`) and rebuild.

## 4. Create a project

There is no project generator yet; copy the demo as the template:

```powershell
Copy-Item -Recurse .\projects\demo_project .\projects\my_game
```

Then:

1. **Add it to the build** in the root `CMakeLists.txt`:

   ```cmake
   add_subdirectory(projects/my_game)
   ```

2. **Rename the executables** in `projects/my_game/CMakeLists.txt` (`demo3d` → `my_game`) and keep the
   engine libraries `e3d_core`, `e3d_render_3ds`, `e3d_audio_3ds`, `e3d_io_3ds`.

3. **Edit `project.json`** (`name`, `title`, `id`, `start_scene`). The runtime does not consume this
   file yet; the desktop editor will.

4. **Edit `scenes/main.scene.json`** and the asset GUIDs in `assets/manifest.json`.

5. **Write the app entry point** by copying `src/demo_3ds.cpp`. It:

   - initialises `RomFsFileSystem`, `AssetDb` (from `manifest.json`) and `AssetManager`
   - loads the scene with `load_scene(world, text)`
   - resolves asset handles with `resolve_scene_assets(world, assets, warnings)`
   - registers project components with `register_project_components()`
   - constructs `Runtime` and loops: poll input, `runtime.update`, `world.update_transforms`,
     `build_render_list`, `render_scripts`, `render->execute`

If you forget to register a project component before `load_scene`, the loader preserves the component
as unknown data and reports a warning instead of failing.

## 4b. Editor

The host build also produces the editor:

```powershell
.\tools\build.ps1 -Target desktop
.\build\desktop\editor\lumika_editor.exe
```

It loads `projects/demo_project` on start and docks panels like Unity:

| Panel | What it does |
| --- | --- |
| Scene | Live OpenGL viewport of the scene with a ground grid, camera frustum gizmos, selection and translate gizmos |
| Game | Tab beside Scene showing the Top (400x240) and Bottom (320x240) camera views |
| Hierarchy | Entity tree, select, create (`New`), delete |
| Inspector | Name, transform, components and schema-driven properties, add/remove components |
| Assets | Indexed by manifest tags, type + tag filters, fuzzy search over name/path/tags, prefab list |
| Console | Status and engine log |

Scene view controls:

| Input | Action |
| --- | --- |
| Right-drag | Look around |
| `WASD` / arrow keys | Fly forward/back/strafe (`Q`/`E` down/up) |
| `Shift` | Faster movement |
| Mouse wheel | Dolly along the view direction |
| Left-click | Select the nearest entity; click a gizmo handle and drag to move it on that axis |
| `F` | Frame the selected entity |
| `F5` | Play/stop the scene in the editor |
| `F6` / `F7` | Focus the Game / Scene tab |

`View → Camera Gizmos` toggles the near/far frustum wireframes for every `Camera` component (the
selected camera is highlighted). `View → Reset Layout` rebuilds the default dockspace and
`View → Reset Camera` returns to the start pose. The viewport renders through the same `RenderList`
as the 3DS, using the desktop OpenGL 3.3 backend in `engine/platform/desktop`.

### Play and camera preview

`F5` (or **Game → Play**) serializes the current scene, loads it into a separate world, resolves its
assets and runs the registered scripts on it, exactly like the runtime. Dynamic meshes (Tetris board,
next piece) update live, and the Scene view switches to the play copy while it runs. `F5` again
stops and restores the authored scene.

The **Game** tab renders each screen's `Camera` component with its field of view, near/far planes and
layer mask, so it is representative of what the console will show. The difference: the desktop
backend does not decode `.t3x` textures or draw sprites yet, so sprite-based HUD text and textured
backdrops are missing from the preview.

Editing model:

- Properties are generated from `PropDesc`, so new component fields appear without editor changes.
- Asset-ref properties show a picker filtered by `meta.asset_type`; entity refs show an entity picker.
- **Create prefab**: select an entity, type a name, press *From selection*. A `.prefab.json` is written
  to `assets/prefabs/`.
- **Instantiate**: double-click a prefab (or right-click → Instantiate). GUIDs are regenerated and
  internal references remapped, so prefabs can be instantiated repeatedly.
- **Playtest**: `Build → Playtest` runs `tools/build.ps1 -Target 3ds`, then launches the `.3dsx` in
  Azahar.

Limitations: the editor links the demo project's scripts directly (a generic per-project registry is
next); play-in-editor runs scripts and dynamic meshes but has no 3DS input, audio, sprite/HUD or
texture decoding; gizmos are translate-only; there is no undo/redo; `.prefab.json` files are found by
scanning `assets/prefabs/`, not through the manifest; dynamic meshes created while playing are not
freed when you stop.



```
projects/my_game/
  project.json                 manifest for tooling/editor
  CMakeLists.txt               builds the 3DS and host targets
  assets/
    manifest.json              guid -> type -> cooked path
    models/*.obj               source meshes
    textures/*.png             source textures
    materials/*.mat.json       material definitions
    audio/*.wav                PCM16 clips
    cooked/                    generated, committed for now
  scenes/main.scene.json       entity tree, components, properties
  scripts/                     C++ components + registration
  src/demo_3ds.cpp             app entry point
```

## 6. Add a script component

```cpp
// scripts/spin.hpp
#pragma once
#include <e3d/e3d.hpp>

class Spin final : public e3d::ScriptComponent {
public:
    E3D_COMPONENT(Spin, e3d::ScriptComponent)

    e3d::Vec3 axis{0.0f, 1.0f, 0.0f};
    float degrees_per_second = 60.0f;

    static std::span<const e3d::PropDesc> properties();
    void on_update(float dt) override;
};
```

```cpp
// scripts/spin.cpp
#include "spin.hpp"

std::span<const e3d::PropDesc> Spin::properties() {
    static const e3d::PropDesc props[] = {
        E3D_PROP(Spin, axis),
        E3D_PROP(Spin, degrees_per_second).step(1.0f),
    };
    return {props, std::size(props)};
}

void Spin::on_update(float dt) {
    e3d::Entity* owner = e3d::world().entity(entity());
    if (owner == nullptr) return;
    owner->transform.rotation += axis * (degrees_per_second * dt);
}

E3D_IMPLEMENT_COMPONENT(Spin)
```

Register it in `scripts/register.cpp`:

```cpp
e3d::register_component(Spin::static_descriptor());
```

Reference it from a scene:

```json
{ "type": "Spin", "props": { "axis": [0, 1, 0], "degrees_per_second": 55 } }
```

`E3D_PROP(Type, member)` supports `bool`, `int`, `float`, `Vec2/3/4`, `Color`, enums, `std::string`,
`AssetRef` and `EntityRef`. Add a property = one line; loading, saving and the future inspector all
pick it up.

## 7. Add an asset

1. Drop `rocket.obj` into `assets/models/`, `rocket.png` into `assets/textures/`.
2. Run `tools\cook.ps1` → `assets/cooked/models/rocket.cmesh`, `.../rocket.t3x`.
3. Add entries to `assets/manifest.json`:

   ```json
   { "guid": "<uuid>", "type": "mesh", "path": "models/rocket.cmesh" },
   { "guid": "<uuid>", "type": "texture", "path": "textures/rocket.t3x" }
   ```

4. Create a material `assets/materials/rocket.mat.json`:

   ```json
   {
     "format_version": 1,
     "shader": "lit_textured",
     "tint": [1, 1, 1, 1],
     "ambient": 0.25,
     "diffuse": 0.8,
     "light_direction": [-0.4, 0.8, 0.6],
     "texture": { "$asset": "<texture guidance>" }
   }
   ```

   Add the material to the manifest with `"type": "material"`.

5. Reference both from a `MeshRenderer` component in the scene with `{ "$asset": "<guid>" }`.

## 8. Scenes and materials

- Entities have a `guid`, `name`, `transform` (Euler degrees, XYZ), `components` and `children`.
- Components are `{ "type": "<registered name>", "props": { ... } }`.
- References use `{"$asset": guid}` and `{"$entity": guid}`.
- Unknown component types and unknown properties are preserved on save.
- Component property names are the C++ member names.

## 9. Troubleshooting

| Symptom | Cause / fix |
| --- | --- |
| Scene loads but a mesh is invisible | Asset GUID missing from `manifest.json`, or material texture GUID wrong. Check the warnings in the SD report. |
| Old scene/assets in the `.3dsx` | RomFS is staged at configure time; delete the `.3dsx` or `build/3ds` and rebuild. |
| `ndspInit` fails (`0xD880A7FA`) | Missing `sdmc:/3ds/dspfirm.cdc`; see §2. |
| 3DS build fails under native CMake | devkitPro's toolchain requires MSYS2 CMake; use `tools/build.ps1 -Target 3ds`. |
| Text is mirrored/upside down | Font atlas orientation; see `engine/src/render/text.cpp` UV mapping. |
| Board missing on the top screen | Check the camera `screen` property and `cull_mask` vs `MeshRenderer.layer`; see §8. |

## 10. Demo controls

| Input | Action |
| --- | --- |
| D-pad / Circle Pad | Move |
| Down | Soft drop |
| Up | Hard drop |
| A / B | Rotate CW / CCW |
| Start | Pause |
| Select | Quit |

## 11. What is not there yet

- Desktop editor (M6) and desktop renderer.
- On-device ImGui / debug overlay.
- Ghost piece, hold, animation, physics.
- Automatic asset cooking in the build (currently `tools/cook.ps1`).
- Project generator / `project.json` runtime consumption.
- Stereo 3D rendering.

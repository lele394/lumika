# **Nintendo 3DS Game Engine — Project Summary**

## **1. Project vision**

Build a small, custom **Unity-inspired game engine for the Nintendo 3DS**, using AI-assisted development.

The goal is not to recreate all of Unity. It is to provide a familiar game development workflow with:

- A desktop editor.
- A component-based scene system.
- 2D and 3D rendering.
- Scriptable GameObjects.
- Custom materials and shaders.
- Animation and physics.
- Scene management.
- Native Nintendo 3DS deployment.

The engine should be data-driven. Most project content should be represented as JSON, with scripts and external assets attached to those definitions.

## **2. Core architecture**

The engine will have three main parts:

**Desktop Editor**

Scene editor · Inspector · Hierarchy · Asset browser · Material editor

**Shared Engine Core**

Scene graph · GameObjects · Components · Scripts · Resources

**Desktop Runtime**

Development and preview

**3DS Runtime**

Native ARM11 execution

The desktop editor and both runtimes should use shared project formats and engine definitions.

## **3. GameObject and component system**

The fundamental object is a GameObject.

A GameObject is essentially an anchor in a scene hierarchy. It contains:

- A unique ID.
- A name.
- A Transform.
- Components.
- Child GameObjects.

Components provide functionality.

Examples:

| Component    | Responsibility   |
| ------------ | ---------------- |
| MeshRenderer | Render 3D models |
| Camera       | Define a view    |
| Light        | Lighting         |
| Rigidbody    | Physics          |
| Collider     | Collision        |
| AudioSource  | Audio playback   |
| Script       | Custom behaviour |
| Animator     | Animation        |

Components can be attached to any GameObject, including the root scene node.

There should be no special distinction between a scene root and an ordinary GameObject.

## **4. Data-driven JSON format**

Scenes should be serializable as JSON.

Example:

```
{
  "format_version": 1,
  "scene_id": "main_scene",
  "root": {
    "id": "root",
    "name": "MainScene",
    "components": [
      {
        "type": "EnvironmentSettings",
        "properties": {
          "background": [0.2, 0.3, 0.5]
        }
      }
    ],
    "children": [
      {
        "id": "player",
        "name": "Player",
        "transform": {
          "position": [0, 0, 0],
          "rotation": [0, 0, 0],
          "scale": [1, 1, 1]
        },
        "components": [
          {
            "type": "MeshRenderer",
            "mesh": "models/player.mesh",
            "material": "materials/player.mat"
          },
          {
            "type": "Script",
            "script": "scripts/player.lua",
            "properties": {
              "speed": 5.0
            }
          }
        ],
        "children": []
      }
    ]
  }
}
```

Important design principles:

- JSON is the editable project format.
- Runtime data is loaded into native C++ objects.
- References use stable UUIDs, not object names.
- Component properties have explicit schemas.
- Assets are stored separately.
- The runtime resolves dependencies when loading scenes.

## **5. Script and lifecycle system**

Scripts are components attached to GameObjects.

They expose lifecycle hooks that the engine calls automatically.

Proposed hooks:

| Hook                  | Execution                   |
| --------------------- | --------------------------- |
| `on_create()`         | Component instance creation |
| `on_attach()`         | Component attachment        |
| `on_start()`          | Before first update         |
| `on_update(dt)`       | Every frame                 |
| `on_fixed_update(dt)` | Fixed physics timestep      |
| `on_enable()`         | Component enabled           |
| `on_disable()`        | Component disabled          |
| `on_destroy()`        | Component destruction       |

Example:

```
function on_create()
    self.speed = 5.0
end

function on_start()
    self.transform = self:get_transform()
end

function on_update(dt)
    self.transform:translate(
        0,
        0,
        self.speed * dt
    )
end
```

The initial recommendation was to investigate Lua as the scripting language.

C# was also discussed because of the Unity-like development experience. ReSharp3DS was identified as a project worth investigating, but its maturity and suitability remain unverified.

## **6. Component and GameObject references**

Scripts must be able to access other objects and components.

Proposed API:

| Function                               | Purpose                       |
| -------------------------------------- | ----------------------------- |
| `self:get_gameobject()`                | Get owning GameObject         |
| `self:get_parent()`                    | Get parent                    |
| `self:get_child(name)`                 | Find direct child             |
| `self:find(path)`                      | Find object in hierarchy      |
| `self:get_component(type)`             | Find component on same object |
| `self:get_component_in_parent(type)`   | Search ancestors              |
| `self:get_component_in_children(type)` | Search descendants            |
| `self:get_scene()`                     | Access current scene          |

The engine should also support serialized object references.

This allows scripts to expose references to other GameObjects or components through the editor.

## **7. Desktop editor**

The editor should follow Unity's general interface and workflow.

[Unity - Manual: The Project window](https://images.openai.com/static-rsc-4/2BKIz7ra4HOEHPFmHw3pTWAi7oDGgI0h0s5XCagQmyM5-YMgeheG3P_FXd5CfJfXGvBa8L4B93lqCZvs4TeDS0Ei8mp2ASokyVLDx6NSU2Rsx4Q-EYBMetOFpaFP7KOd0rCOgrIAznYcdD3yX9CloQI0oDXpftujvO0nZYFeIHk?purpose=inline)

[Why Maya material color is different than Unity material color? - Shaders & Materials - Unity Discussions](https://images.openai.com/static-rsc-4/Ho9fqpsaiCCsC1LGbXen3Ujg3USpDgAgnAdaOeLdMdmMJQdrjMZUvtU4tJ4tZJy4RvNXSoPCER7b1jbpg11E-JyUJRDwxvBPW9qeQFS_5gfcuLJSP5UtMq9ZXZrrnHtR20abhIhWT-GrXMBk782XBAueUzJliMRzTlJLy4n7K_M?purpose=inline)

7

Proposed panels:

| Panel            | Function                  |
| ---------------- | ------------------------- |
| Scene View       | Visual scene editing      |
| Game View        | Preview game camera       |
| Hierarchy        | GameObject tree           |
| Inspector        | Component properties      |
| Project          | Asset browser             |
| Console          | Logs and errors           |
| Material Editor  | Material configuration    |
| Animation Editor | Keyframes and controllers |
| Build Manager    | 3DS packaging             |
| Profiler         | Runtime performance       |

### **Editor technology**

Initial proposal:

- C#.
- Avalonia UI.
- OpenGL desktop viewport.
- Shared C++ engine core.

Qt is an alternative, but Avalonia is attractive for its C# integration and cross-platform support.

## **8. Graphics and hardware**

The target hardware is the Nintendo 3DS family.

Proposed development stack:

| Technology            | Purpose                |
| --------------------- | ---------------------- |
| devkitPro             | Development toolchain  |
| devkitARM             | ARM cross-compilation  |
| libctru               | Hardware and OS access |
| citro3d               | 3D rendering           |
| citro2d               | 2D rendering           |
| Assimp                | Model importing        |
| glTF                  | Source model format    |
| Custom binary formats | Runtime assets         |

The PICA200 GPU requires a hardware-aware rendering system.

Custom shaders are possible, but the engine must expose features that the hardware can support. It should not assume modern desktop GPU capabilities.

Both original and New Nintendo 3DS support should be considered.

## **9. Proposed feature scope**

Initial target

| Feature                    | Priority  |
| -------------------------- | --------- |
| Scene hierarchy            | Essential |
| GameObjects and components | Essential |
| JSON serialization         | Essential |
| Script lifecycle           | Essential |
| Component references       | Essential |
| Transform system           | Essential |
| 3D mesh rendering          | Essential |
| Materials and textures     | Essential |
| Scene transitions          | Essential |
| Desktop editor             | Essential |
| Custom shaders             | High      |
| Animation                  | High      |
| Prefabs                    | High      |
| Physics                    | High      |
| Audio                      | High      |
| 2D rendering               | High      |
| Profiling                  | Later     |

## **10. Recommended implementation order**

The most important decision is to build the data architecture before the renderer.

1. **Engine core**

   GameObject, Transform, Component, Component Registry and Scene Graph.
2. **Data system**

   JSON serialization, component schemas, UUID references and asset loading.
3. **Script runtime**

   Script attachment, lifecycle hooks and cross-component access.
4. **Desktop runtime**

   Basic rendering, input and game loop.
5. **3DS runtime**

   libctru integration and citro3d rendering.
6. **Desktop editor**

   Hierarchy, Inspector, Scene View, Project browser and Console.
7. **Game features**

   Materials, shaders, animation, physics and audio.
8. **Build pipeline**

   Asset conversion, optimisation and 3DS packaging.

## **11. Current open decisions**

These points still need technical investigation or a final decision.

| Question                     | Current status     |
| ---------------------------- | ------------------ |
| C++17 or newer?              | C++17 proposed     |
| Lua or C# scripting?         | Unresolved         |
| Avalonia or Qt?              | Avalonia preferred |
| Desktop rendering backend?   | OpenGL proposed    |
| Scene format?                | JSON proposed      |
| Prefab implementation?       | To define          |
| Component dependency system? | To define          |
| Shader format and compiler?  | To investigate     |
| ReSharp3DS suitability?      | To investigate     |

## **12. The central design philosophy**

I would summarise the project in five principles:

1. **Everything is data.** Scenes, objects, components and properties have explicit definitions.
2. **Everything is composable.** GameObjects are containers for components, not specialised classes.
3. **Everything is inspectable.** The editor can generate inspectors from component schemas.
4. **The editor is first-class.** This is a game development environment, not just a rendering library.
5. **The 3DS is the primary target.** Desktop tools exist to make development easier, not to dictate hardware capabilities.

**The end goal:** open the editor, create a scene, add a GameObject, attach a mesh and script, configure its properties, press Play, and eventually deploy the same project to a physical Nintendo 3DS.

That is the project we have defined so far.
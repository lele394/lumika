add_library(e3d_core STATIC
  src/assets/mesh_loader.cpp
  src/core/guid.cpp
  src/core/json.cpp
  src/core/log.cpp
  src/reflect/registry.cpp
  src/render/render_list.cpp
  src/render/renderer.cpp
  src/scene/components.cpp
  src/scene/prop_io.cpp
  src/scene/scene_io.cpp
  src/scene/world.cpp
)

target_include_directories(e3d_core PUBLIC include)
target_compile_features(e3d_core PUBLIC cxx_std_20)

if(NOT NINTENDO_3DS)
  target_compile_options(e3d_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(NINTENDO_3DS)
  add_subdirectory(platform/3ds)
endif()

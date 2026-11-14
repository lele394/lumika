cmake_minimum_required(VERSION 3.21)
project(3ds_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NINTENDO_3DS)
  set(E3D_3DS ON)
  # Device builds are exception- and RTTI-free; keep the engine core identical.
  add_compile_options(-fno-exceptions -fno-rtti)
endif()

add_subdirectory(engine)
add_subdirectory(projects/demo_project)

if(NOT E3D_3DS)
  include(CTest)
  add_subdirectory(tools)
  add_subdirectory(tests)
endif()

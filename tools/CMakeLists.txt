add_executable(scene_dump scene_dump/main.cpp)
target_link_libraries(scene_dump PRIVATE e3d_core)
target_compile_options(scene_dump PRIVATE -Wall -Wextra)

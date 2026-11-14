add_executable(meshc meshc/main.cpp)
target_link_libraries(meshc PRIVATE e3d_core)
target_compile_options(meshc PRIVATE -Wall -Wextra)

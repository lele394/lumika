add_library(e3d_render_3ds STATIC
  src/render_device_3ds.cpp
)

target_include_directories(e3d_render_3ds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(e3d_render_3ds PUBLIC e3d_core citro3d)

# --- shaders ---------------------------------------------------------------
find_program(E3D_PICASSO picasso HINTS "${DEVKITPRO}/tools/bin" REQUIRED)

set(E3D_SHADER_SRC ${PROJECT_SOURCE_DIR}/engine/render/shaders/unlit_color.v.pica)
set(E3D_SHADER_OUT ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(E3D_SHADER_BIN ${E3D_SHADER_OUT}/unlit_color.shbin)
set(E3D_SHADER_UNIFORMS ${E3D_SHADER_OUT}/unlit_color_uniforms.h)
set(E3D_SHADER_DATA ${E3D_SHADER_OUT}/unlit_color_shbin_data.h)

add_custom_command(
  OUTPUT ${E3D_SHADER_BIN} ${E3D_SHADER_UNIFORMS} ${E3D_SHADER_DATA}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${E3D_SHADER_OUT}
  COMMAND ${E3D_PICASSO} -o ${E3D_SHADER_BIN} -h ${E3D_SHADER_UNIFORMS} ${E3D_SHADER_SRC}
  COMMAND ${CMAKE_COMMAND}
          -DINPUT=${E3D_SHADER_BIN}
          -DOUTPUT=${E3D_SHADER_DATA}
          -DSYMBOL=unlit_color_shbin
          -P ${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake
  DEPENDS ${E3D_SHADER_SRC} ${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake
  COMMENT "Compiling 3DS shader unlit_color"
  VERBATIM
)

add_custom_target(e3d_3ds_shaders DEPENDS ${E3D_SHADER_DATA} ${E3D_SHADER_UNIFORMS})
add_dependencies(e3d_render_3ds e3d_3ds_shaders)
target_include_directories(e3d_render_3ds PRIVATE ${E3D_SHADER_OUT})

add_library(e3d_render_3ds STATIC
  src/render_device_3ds.cpp
)

target_include_directories(e3d_render_3ds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(e3d_render_3ds PUBLIC e3d_core citro3d)

add_library(e3d_audio_3ds STATIC
  src/audio_device_3ds.cpp
)

target_include_directories(e3d_audio_3ds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(e3d_audio_3ds PUBLIC e3d_core ctru)

add_library(e3d_io_3ds STATIC
  src/file_system_3ds.cpp
  src/input_3ds.cpp
)

target_include_directories(e3d_io_3ds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(e3d_io_3ds PUBLIC e3d_core ctru)

# --- shaders ---------------------------------------------------------------
find_program(E3D_PICASSO picasso HINTS "${DEVKITPRO}/tools/bin" REQUIRED)

set(E3D_SHADER_OUT ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(E3D_SHADER_OUTPUTS "")

foreach(SHADER unlit_color lit_textured unlit_textured lit_color)
  set(E3D_SHADER_SRC ${PROJECT_SOURCE_DIR}/engine/render/shaders/${SHADER}.v.pica)
  set(E3D_SHADER_BIN ${E3D_SHADER_OUT}/${SHADER}.shbin)
  set(E3D_SHADER_UNIFORMS ${E3D_SHADER_OUT}/${SHADER}_uniforms.h)
  set(E3D_SHADER_DATA ${E3D_SHADER_OUT}/${SHADER}_shbin_data.h)

  add_custom_command(
    OUTPUT ${E3D_SHADER_BIN} ${E3D_SHADER_UNIFORMS} ${E3D_SHADER_DATA}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${E3D_SHADER_OUT}
    COMMAND ${E3D_PICASSO} -o ${E3D_SHADER_BIN} -h ${E3D_SHADER_UNIFORMS} ${E3D_SHADER_SRC}
    COMMAND ${CMAKE_COMMAND}
            -DINPUT=${E3D_SHADER_BIN}
            -DOUTPUT=${E3D_SHADER_DATA}
            -DSYMBOL=${SHADER}_shbin
            -P ${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake
    DEPENDS ${E3D_SHADER_SRC} ${PROJECT_SOURCE_DIR}/cmake/embed_binary.cmake
    COMMENT "Compiling 3DS shader ${SHADER}"
    VERBATIM
  )

  list(APPEND E3D_SHADER_OUTPUTS ${E3D_SHADER_BIN} ${E3D_SHADER_UNIFORMS} ${E3D_SHADER_DATA})
endforeach()

add_custom_target(e3d_3ds_shaders DEPENDS ${E3D_SHADER_OUTPUTS})
add_dependencies(e3d_render_3ds e3d_3ds_shaders)
target_include_directories(e3d_render_3ds PRIVATE ${E3D_SHADER_OUT})

find_package(glfw3 CONFIG REQUIRED)
find_path(GLEW_INCLUDE_DIR GL/glew.h)
find_library(GLEW_LIBRARY NAMES glew32)
if(GLEW_LIBRARY)
  get_filename_component(GLEW_LIB_DIR "${GLEW_LIBRARY}" DIRECTORY)
  if(EXISTS "${GLEW_LIB_DIR}/libglew32.a")
    set(GLEW_LIBRARY "${GLEW_LIB_DIR}/libglew32.a")
  endif()
endif()
if(NOT GLEW_INCLUDE_DIR OR NOT GLEW_LIBRARY)
  message(FATAL_ERROR "GLEW not found (install mingw-w64-x86_64-glew)")
endif()

set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/third_party/imgui)

add_executable(lumika_editor
  main.cpp
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
  ${PROJECT_SOURCE_DIR}/projects/demo_project/scripts/register.cpp
  ${PROJECT_SOURCE_DIR}/projects/demo_project/scripts/spin.cpp
  ${PROJECT_SOURCE_DIR}/projects/demo_project/scripts/play_sound.cpp
  ${PROJECT_SOURCE_DIR}/projects/demo_project/scripts/tetris_game.cpp
  ${PROJECT_SOURCE_DIR}/projects/demo_project/scripts/board_view.cpp
  ${PROJECT_SOURCE_DIR}/projects/demo_project/scripts/next_piece.cpp
  ${PROJECT_SOURCE_DIR}/projects/demo_project/scripts/tetris_hud.cpp
  ${PROJECT_SOURCE_DIR}/engine/platform/desktop/src/render_device_gl.cpp
)

target_include_directories(lumika_editor PRIVATE
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  ${PROJECT_SOURCE_DIR}/projects/demo_project/scripts
  ${PROJECT_SOURCE_DIR}/engine/platform/desktop/include
  ${GLEW_INCLUDE_DIR}
)

target_link_libraries(lumika_editor PRIVATE e3d_core glfw ${GLEW_LIBRARY})
target_compile_definitions(lumika_editor PRIVATE GLEW_STATIC)
if(WIN32)
  target_link_libraries(lumika_editor PRIVATE opengl32)
endif()

target_compile_definitions(lumika_editor PRIVATE
  LUMIKA_ROOT="${PROJECT_SOURCE_DIR}"
)

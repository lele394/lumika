find_package(glfw3 CONFIG REQUIRED)

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
)

target_include_directories(lumika_editor PRIVATE
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  ${PROJECT_SOURCE_DIR}/projects/demo_project/scripts
)

target_link_libraries(lumika_editor PRIVATE e3d_core glfw)
if(WIN32)
  target_link_libraries(lumika_editor PRIVATE opengl32)
endif()

target_compile_definitions(lumika_editor PRIVATE
  LUMIKA_ROOT="${PROJECT_SOURCE_DIR}"
)

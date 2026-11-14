if(NINTENDO_3DS)
  add_executable(tetris3d src/main_3ds.cpp)
  target_link_libraries(tetris3d PRIVATE e3d_core citro2d citro3d)

  # devkitPro helper: builds an SMDH and packs tetris3d.3dsx next to the ELF.
  ctr_create_3dsx(tetris3d)

  # M1 device check: scene loading/reference resolution/round-trip on ARM.
  add_executable(scene_check src/scene_check_3ds.cpp)
  target_link_libraries(scene_check PRIVATE e3d_core)
  ctr_create_3dsx(scene_check)

  # M2 renderer check: cooked mesh + t3x texture + lit shader on both screens.
  set(E3D_ROMFS_DIR ${CMAKE_CURRENT_BINARY_DIR}/romfs)
  file(MAKE_DIRECTORY ${E3D_ROMFS_DIR})
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/cooked/ DESTINATION ${E3D_ROMFS_DIR})

  add_executable(render_check src/render_check_3ds.cpp)
  target_link_libraries(render_check PRIVATE e3d_core e3d_render_3ds)
  ctr_create_3dsx(render_check ROMFS ${E3D_ROMFS_DIR})

  # M3 demo: scene-driven meshes/materials/scripts + input + NDSP audio.
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/scenes/main.scene.json DESTINATION ${E3D_ROMFS_DIR})

  add_executable(demo3d
    src/demo_3ds.cpp
    scripts/spin.cpp
    scripts/play_sound.cpp
    scripts/register.cpp
  )
  target_include_directories(demo3d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
  target_link_libraries(demo3d PRIVATE e3d_core e3d_render_3ds e3d_audio_3ds e3d_io_3ds)
  ctr_create_3dsx(demo3d ROMFS ${E3D_ROMFS_DIR})
else()
  find_package(glfw3 CONFIG REQUIRED)

  add_executable(tetris3d src/main_desktop.cpp)
  target_link_libraries(tetris3d PRIVATE e3d_core glfw)
  if(WIN32)
    target_link_libraries(tetris3d PRIVATE opengl32)
  endif()
endif()

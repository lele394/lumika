if(NINTENDO_3DS)
  add_executable(tetris3d src/main_3ds.cpp)
  target_link_libraries(tetris3d PRIVATE e3d_core citro2d citro3d)

  # devkitPro helper: builds an SMDH and packs tetris3d.3dsx next to the ELF.
  # Add ROMFS=<dir> later for packaged assets.
  ctr_create_3dsx(tetris3d)

  # M1 device check: scene loading/reference resolution/round-trip on ARM.
  add_executable(scene_check src/scene_check_3ds.cpp)
  target_link_libraries(scene_check PRIVATE e3d_core)
  ctr_create_3dsx(scene_check)
else()
  find_package(glfw3 CONFIG REQUIRED)

  add_executable(tetris3d src/main_desktop.cpp)
  target_link_libraries(tetris3d PRIVATE e3d_core glfw)
  if(WIN32)
    target_link_libraries(tetris3d PRIVATE opengl32)
  endif()
endif()

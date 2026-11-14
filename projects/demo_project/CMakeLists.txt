if(NINTENDO_3DS)
  add_executable(tetris3d src/main_3ds.cpp)
  target_link_libraries(tetris3d PRIVATE e3d_core citro2d citro3d)

  # devkitPro helper: builds an SMDH and packs tetris3d.3dsx next to the ELF.
  # Add ROMFS=<dir> later for packaged assets.
  ctr_create_3dsx(tetris3d)
else()
  find_package(glfw3 CONFIG REQUIRED)

  add_executable(tetris3d src/main_desktop.cpp)
  target_link_libraries(tetris3d PRIVATE e3d_core glfw)
  if(WIN32)
    target_link_libraries(tetris3d PRIVATE opengl32)
  endif()
endif()

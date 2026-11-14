if(E3D_3DS)
  add_executable(tetris3d src/main_3ds.cpp)
  set_target_properties(tetris3d PROPERTIES SUFFIX ".elf")

  target_link_directories(tetris3d PRIVATE "${CTRULIB}/lib")
  target_link_libraries(tetris3d PRIVATE e3d_core citro2d citro3d ctru m)

  find_program(THREE_DSXTOOL 3dsxtool HINTS "${DEVKITPRO}/tools/bin" REQUIRED)

  add_custom_command(TARGET tetris3d POST_BUILD
    COMMAND ${THREE_DSXTOOL} $<TARGET_FILE:tetris3d> ${CMAKE_CURRENT_BINARY_DIR}/tetris3d.3dsx
    COMMENT "Packing tetris3d.3dsx"
    VERBATIM)
else()
  find_package(glfw3 CONFIG REQUIRED)

  add_executable(tetris3d src/main_desktop.cpp)
  target_link_libraries(tetris3d PRIVATE e3d_core glfw)
  if(WIN32)
    target_link_libraries(tetris3d PRIVATE opengl32)
  endif()
endif()

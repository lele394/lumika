add_executable(e3d_tests
  main.cpp
  test_core.cpp
  test_guid.cpp
  test_json.cpp
  test_math.cpp
  test_registry.cpp
  test_scene.cpp
  test_types.cpp
  test_world.cpp
)

target_link_libraries(e3d_tests PRIVATE e3d_core)
target_include_directories(e3d_tests PRIVATE ${PROJECT_SOURCE_DIR}/third_party)
target_compile_options(e3d_tests PRIVATE -Wall -Wextra)

add_test(NAME e3d_tests COMMAND e3d_tests)

add_library(e3d_core STATIC
  src/core/log.cpp
)

target_include_directories(e3d_core PUBLIC include)
target_compile_features(e3d_core PUBLIC cxx_std_20)

if(NOT NINTENDO_3DS)
  target_compile_options(e3d_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

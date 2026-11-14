# Cross-compilation toolchain for Nintendo 3DS homebrew (devkitARM + libctru).
# Requires DEVKITPRO and DEVKITARM in the environment (see tools/env.ps1).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT DEFINED ENV{DEVKITPRO} OR NOT DEFINED ENV{DEVKITARM})
  message(FATAL_ERROR "DEVKITPRO/DEVKITARM are not set. Run: . tools/env.ps1")
endif()

set(DEVKITPRO "$ENV{DEVKITPRO}" CACHE PATH "devkitPro root")
set(DEVKITARM "$ENV{DEVKITARM}" CACHE PATH "devkitARM root")
set(CTRULIB "${DEVKITPRO}/libctru" CACHE PATH "libctru root")

set(CMAKE_C_COMPILER   "${DEVKITARM}/bin/arm-none-eabi-gcc.exe")
set(CMAKE_CXX_COMPILER "${DEVKITARM}/bin/arm-none-eabi-g++.exe")
set(CMAKE_AR           "${DEVKITARM}/bin/arm-none-eabi-ar.exe")
set(CMAKE_RANLIB       "${DEVKITARM}/bin/arm-none-eabi-ranlib.exe")
set(CMAKE_STRIP        "${DEVKITARM}/bin/arm-none-eabi-strip.exe")
set(CMAKE_SIZE         "${DEVKITARM}/bin/arm-none-eabi-size.exe")

set(E3D_ARCH_FLAGS "-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft")

set(CMAKE_C_FLAGS_INIT   "${E3D_ARCH_FLAGS} -D__3DS__ -D_3DS")
set(CMAKE_CXX_FLAGS_INIT "${E3D_ARCH_FLAGS} -D__3DS__ -D_3DS -fno-exceptions -fno-rtti")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-specs=3dsx.specs ${E3D_ARCH_FLAGS} -Wl,--gc-sections -L${CTRULIB}/lib")

set(CMAKE_FIND_ROOT_PATH ${DEVKITPRO} ${DEVKITARM})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

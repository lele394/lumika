# Generates a C++ header containing a binary blob.
# Usage: cmake -DINPUT=<file> -DOUTPUT=<file> -DSYMBOL=<name> -P embed_binary.cmake

if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED SYMBOL)
    message(FATAL_ERROR "embed_binary.cmake requires INPUT, OUTPUT and SYMBOL")
endif()

file(READ "${INPUT}" hex HEX)
string(LENGTH "${hex}" hex_length)
math(EXPR byte_length "${hex_length} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " bytes "${hex}")

file(WRITE "${OUTPUT}"
"// Generated from ${INPUT} by embed_binary.cmake. Do not edit.\n"
"#pragma once\n"
"#include <cstddef>\n"
"#include <cstdint>\n\n"
"static const std::uint8_t ${SYMBOL}[] = {\n    ${bytes}\n};\n\n"
"static constexpr std::size_t ${SYMBOL}_size = ${byte_length};\n")

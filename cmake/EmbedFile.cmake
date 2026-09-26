# Embeds a text/binary file into a C++ translation unit as a byte array.
# Usage (internal, called by haocam_embed_shader):
#   cmake -DINPUT=<file> -DOUTPUT=<generated.h> -DARRAY_NAME=<name> -P EmbedFile.cmake

if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED ARRAY_NAME)
    message(FATAL_ERROR "EmbedFile.cmake requires INPUT, OUTPUT and ARRAY_NAME")
endif()

file(READ "${INPUT}" content HEX)
string(LENGTH "${content}" hex_length)
math(EXPR byte_count "${hex_length} / 2")

set(byte_text "")
set(count 0)
set(i 0)
while(i LESS hex_length)
    string(SUBSTRING "${content}" ${i} 2 byte)
    string(APPEND byte_text "0x${byte}")
    math(EXPR i "${i} + 2")
    math(EXPR count "${count} + 1")
    math(EXPR count_mod "${count} % 16")
    if(i LESS hex_length)
        string(APPEND byte_text ",")
        if(count_mod EQUAL 0)
            string(APPEND byte_text "\n")
        else()
            string(APPEND byte_text " ")
        endif()
    endif()
endwhile()

file(WRITE "${OUTPUT}"
"// Generated from ${INPUT} by cmake/EmbedFile.cmake - do not edit.
// ${byte_count} bytes.
#pragma once
#include <cstddef>
namespace haocam::shaders {
inline constexpr unsigned char ${ARRAY_NAME}[] = {
${byte_text}
};
inline constexpr size_t ${ARRAY_NAME}_size = sizeof(${ARRAY_NAME});
} // namespace haocam::shaders
")

# Get the current short git hash (8 characters)
execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Optional: handle cases where git might not be available
if (NOT GIT_HASH)
    set(GIT_HASH "00000000")
endif()

# Uppercase it to look like a hex constant (optional)
string(TOUPPER ${GIT_HASH} GIT_HASH_UPPER)

# Path to the generated header file
set(GIT_HASH_HEADER "${CMAKE_BINARY_DIR}/git_hash.h")

# Generate the header content
file(WRITE ${GIT_HASH_HEADER}
"#ifndef GIT_HASH_H\n"
"#define GIT_HASH_H\n\n"
"#define GIT_HASH 0x${GIT_HASH_UPPER}\n\n"
"#endif // GIT_HASH_H\n"
)

# Add the binary dir to include paths so you can do #include "git_hash.h"
include_directories(${CMAKE_BINARY_DIR})

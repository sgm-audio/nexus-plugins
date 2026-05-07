cmake_minimum_required(VERSION 3.21)
project(MambaCompressor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG        1.2.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(clap)

add_library(mamba_compressor MODULE
    src/mamba_compressor.cpp
)

target_include_directories(mamba_compressor PRIVATE
    ${clap_SOURCE_DIR}/include
)

target_compile_options(mamba_compressor PRIVATE
    -O3
    -march=native
    -ffast-math
    -Wall
    -Wextra
    -Wno-unused-parameter
)

set_target_properties(mamba_compressor PROPERTIES
    OUTPUT_NAME  "MambaCompressor"
    SUFFIX       ".clap"
    PREFIX       ""
    POSITION_INDEPENDENT_CODE ON
)

# Install targets
if(UNIX AND NOT APPLE)
    install(TARGETS mamba_compressor
        DESTINATION "$ENV{HOME}/.clap"
    )
elseif(APPLE)
    install(TARGETS mamba_compressor
        DESTINATION "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP"
    )
elseif(WIN32)
    install(TARGETS mamba_compressor
        DESTINATION "$ENV{COMMONPROGRAMFILES}/CLAP"
    )
endif()

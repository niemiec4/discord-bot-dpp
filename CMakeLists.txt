cmake_minimum_required(VERSION 3.16)
project(discord_bot CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(dpp REQUIRED)

# The installed dpp package hardcodes Debian library paths (e.g.
# /usr/lib/x86_64-linux-gnu/libssl.so). If those do not exist on this system,
# point the imported target at the actual libraries via find_library.
get_target_property(DPP_LINK_LIBS dpp::dpp INTERFACE_LINK_LIBRARIES)
set(DPP_LIBS_MISSING OFF)
foreach(_lib IN LISTS DPP_LINK_LIBS)
    if(NOT EXISTS "${_lib}")
        set(DPP_LIBS_MISSING ON)
    endif()
endforeach()
if(DPP_LIBS_MISSING)
    find_library(DPP_SSL_LIB NAMES libssl.so.3 libssl.so libssl)
    find_library(DPP_CRYPTO_LIB NAMES libcrypto.so.3 libcrypto.so libcrypto)
    find_library(DPP_Z_LIB NAMES libz.so.1 libz.so libz)
    find_library(DPP_M_LIB NAMES libm.so.6 libm.so libm)
    find_library(DPP_OPUS_LIB NAMES libopus.so.0 libopus.so libopus)
    if(DPP_SSL_LIB AND DPP_CRYPTO_LIB AND DPP_Z_LIB AND DPP_M_LIB)
        message(STATUS "Repairing dpp::dpp link libraries for this system")
        set_target_properties(dpp::dpp PROPERTIES INTERFACE_LINK_LIBRARIES
            "${DPP_SSL_LIB};${DPP_CRYPTO_LIB};${DPP_Z_LIB};${DPP_OPUS_LIB};${DPP_M_LIB}")
    endif()
endif()

add_executable(discord_bot
    main.cpp
    src/config.cpp
    src/bot_utils.cpp
    src/warnings.cpp
    src/commands.cpp
    src/cmd_moderation.cpp
    src/cmd_utility.cpp
    src/cmd_fun.cpp
    src/settings.cpp
    src/levels.cpp
    src/cmd_settings.cpp
    src/cmd_levels.cpp
)

target_link_libraries(discord_bot PRIVATE dpp::dpp)

if(NOT MSVC)
    target_compile_options(discord_bot PRIVATE -Wall -Wextra)
endif()

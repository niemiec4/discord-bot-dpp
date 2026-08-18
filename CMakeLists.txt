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

# SQLite: system library has no dev symlink here, find the actual .so.
find_library(SQLITE3_LIB NAMES sqlite3 libsqlite3.so.0 libsqlite3.so)
if(NOT SQLITE3_LIB)
    message(FATAL_ERROR "libsqlite3 not found — install it (e.g. apt install libsqlite3-0)")
endif()
message(STATUS "SQLite library: ${SQLITE3_LIB}")

add_executable(discord_bot
    main.cpp
    src/config.cpp
    src/bot_utils.cpp
    src/db.cpp
    src/warnings.cpp
    src/settings.cpp
    src/levels.cpp
    src/commands.cpp
    src/cmd_moderation.cpp
    src/cmd_utility.cpp
    src/cmd_fun.cpp
    src/cmd_settings.cpp
    src/cmd_levels.cpp
    src/economy.cpp
    src/custom_commands.cpp
    src/role_menus.cpp
    src/giveaways.cpp
    src/tickets.cpp
    src/polls.cpp
    src/snipes.cpp
    src/auditlog.cpp
    src/interactions.cpp
    src/cmd_economy.cpp
    src/cmd_custom.cpp
    src/cmd_rolemenu.cpp
    src/cmd_tickets.cpp
    src/cmd_giveaways.cpp
    src/cmd_polls.cpp
    src/cmd_mod_extra.cpp
)

target_link_libraries(discord_bot PRIVATE dpp::dpp ${SQLITE3_LIB})

if(NOT MSVC)
    target_compile_options(discord_bot PRIVATE -Wall -Wextra)
endif()

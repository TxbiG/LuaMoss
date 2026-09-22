cmake_minimum_required(VERSION 3.20)

project(LuaMoss VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)

# Moss
set(MOSS_FRAMEWORK_DIR ""
    CACHE PATH "Path to the Moss framework")

if(NOT MOSS_FRAMEWORK_DIR)
    set(MOSS_FRAMEWORK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Moss")
endif()

get_filename_component(
    MOSS_FRAMEWORK_DIR
    "${MOSS_FRAMEWORK_DIR}"
    ABSOLUTE
)

if(NOT EXISTS "${MOSS_FRAMEWORK_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Moss framework not found: ${MOSS_FRAMEWORK_DIR}")
endif()

# Lua 5.4
find_package(Lua 5.4 QUIET)

if(NOT TARGET Lua::Lua)
    FetchContent_Declare(
        lua_source
        URL https://www.lua.org/ftp/lua-5.4.8.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_MakeAvailable(lua_source)

    file(
        GLOB LUA_SOURCES
        CONFIGURE_DEPENDS
        "${lua_source_SOURCE_DIR}/src/*.c"
    )

    list(REMOVE_ITEM
        LUA_SOURCES
        "${lua_source_SOURCE_DIR}/src/lua.c"
        "${lua_source_SOURCE_DIR}/src/luac.c"
    )

    add_library(lua_static STATIC ${LUA_SOURCES})

    add_library(Lua::Lua ALIAS lua_static)

    target_include_directories(
        lua_static
        PUBLIC
            "${lua_source_SOURCE_DIR}/src"
    )

    set_target_properties(
        lua_static
        PROPERTIES
            POSITION_INDEPENDENT_CODE ON
    )

    if(WIN32)
        target_compile_definitions(
            lua_static
            PRIVATE
                LUA_USE_WINDOWS
        )
    elseif(APPLE)
        target_compile_definitions(
            lua_static
            PRIVATE
                LUA_USE_MACOSX
        )
    elseif(UNIX)
        target_compile_definitions(
            lua_static
            PRIVATE
                LUA_USE_LINUX
        )

        target_link_libraries(
            lua_static
            PRIVATE
                m
                dl
        )
    endif()
endif()

# Moss
add_subdirectory(
    "${MOSS_FRAMEWORK_DIR}"
    "${CMAKE_BINARY_DIR}/moss"
    EXCLUDE_FROM_ALL
)

if(NOT TARGET Moss::Moss)
    message(FATAL_ERROR
        "Moss::Moss target was not found.")
endif()

# LuaMoss
add_library(
    moss
    MODULE
        src/main.cpp
)

target_compile_features(
    moss
    PRIVATE
        cxx_std_17
)

target_link_libraries(
    moss
    PRIVATE
        Moss::Moss
        Lua::Lua
)

set_target_properties(
    moss
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "moss"
)
cmake_minimum_required(VERSION 3.9)

macro(find_all_headers_and_sources)
    file(
        GLOB _public_headers
        "include/*.hh"
        "include/*.h"
        "include/*.hpp"
    )
    file(
        GLOB _private_headers
        "*.hh"
        "*.h"
        "*.hpp"
    )
    aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}" _all_srcs)
    message(
        STATUS
        "_all_srcs=${_all_srcs}"
    )
endmacro()

function(is_debug_build out)
    STRING(TOUPPER "${CMAKE_BUILD_TYPE}" _CMAKE_BUILD_TYPE_UPPER)
    if(_CMAKE_BUILD_TYPE_UPPER STREQUAL "DEBUG")
        set(_TMP yes)
    endif()
    set( ${out}  ${_TMP} PARENT_SCOPE)
endfunction()

macro(set_install_rpath)
    # the RPATH to be used when installing, but only if it's not a system directory
    LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
    IF("${isSystemDir}" STREQUAL "-1")
        SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/$<CONFIG>")
    ENDIF("${isSystemDir}" STREQUAL "-1")
endmacro()

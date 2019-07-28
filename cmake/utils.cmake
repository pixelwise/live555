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
endmacro()

function(is_debug_build out)
    STRING(TOUPPER "${CMAKE_BUILD_TYPE}" _CMAKE_BUILD_TYPE_UPPER)
    if(_CMAKE_BUILD_TYPE_UPPER STREQUAL "DEBUG")
        set(_TMP yes)
    endif()
    set( ${out}  ${_TMP} PARENT_SCOPE)
endfunction()

macro(set_install_rpath)
    set(
        _RPATH
        "${CMAKE_INSTALL_PREFIX}/lib"
    )
    SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    # the RPATH to be used when installing, but only if it's not a system directory
    LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${_RPATH}" isSystemDir)
    IF("${isSystemDir}" STREQUAL "-1")
        LIST(APPEND CMAKE_INSTALL_RPATH "${_RPATH}")
    ENDIF("${isSystemDir}" STREQUAL "-1")
    message(
        STATUS
        "_RPATH=${_RPATH}\nCMAKE_INSTALL_RPATH=${CMAKE_INSTALL_RPATH}"
    )
endmacro()

function(install_target target public_headers)
    install(
        TARGETS ${target} EXPORT live555Target
        INCLUDES DESTINATION include/${target}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
    install(FILES ${public_headers}
        DESTINATION include/${target}
    )
endfunction()

function(EXTEND_env VALUES OUTPUT)
    set(
        _TMP_STR
        ${${OUTPUT}}
        ${VALUES}
    )
    join_list(_TMP_STR ":" _TMP_STR)
    set (${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()

function(join_list INPUT GLUE OUTPUT)
    string (REPLACE ";" "${GLUE}" _TMP_STR "${${INPUT}}")
    set (${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()

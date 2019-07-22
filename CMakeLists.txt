cmake_minimum_required(VERSION 3.9)

# Parse version number from the header file
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/liveMedia/include/liveMedia_version.hh contents)
string(REGEX MATCH "define LIVEMEDIA_LIBRARY_VERSION_STRING	\"([0-9\.]+)\"" _ ${contents})
set(project_version ${CMAKE_MATCH_1})
if(NOT DEFINED PROJECT_NAME)
    set(master_project TRUE)
endif()

project(Live555
    LANGUAGES C CXX 
    VERSION ${project_version}
)
message(STATUS "Live555 version: ${project_version}")

list(APPEND CMAKE_MODULE_PATH ${live555_SOURCE_DIR}/cmake)
include(TargetVersion)
include(utils)
set_install_rpath()

option(LIVE555_BUILD_EXAMPLES "Build examples and test programs" OFF)

add_library(live555_cxx_flags INTERFACE)
if(WIN32)
    target_compile_definitions(live555_cxx_flags INTERFACE
        LOCALE_NOT_USED
        SOCKLEN_T=int
    )
else()
    target_compile_definitions(live555_cxx_flags INTERFACE
        BSD=1
        SOCKLEN_T=socklen_t
    )
endif()
install(TARGETS live555_cxx_flags EXPORT live555Target)

add_subdirectory(UsageEnvironment)
add_subdirectory(groupsock)
add_subdirectory(BasicUsageEnvironment)
add_subdirectory(liveMedia)

if(LIVE555_BUILD_EXAMPLES)
    add_subdirectory(testProgs)
    add_subdirectory(proxyServer)
    add_subdirectory(mediaServer)
endif()

if(master_project)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

# Installation stuff
set(config_version_file ${CMAKE_CURRENT_BINARY_DIR}/live555ConfigVersion.cmake)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(${config_version_file}
                                 VERSION ${live555_VERSION}
                                 COMPATIBILITY ExactVersion)
add_library(
    all_libs
    INTERFACE
)
target_link_libraries(
    all_libs
    INTERFACE
    UsageEnvironment
    BasicUsageEnvironment
    groupsock
    liveMedia
)
add_library(
    Live555::all_libs
    ALIAS
    all_libs
)
install(
    TARGETS all_libs EXPORT live555Target
)

install(FILES ${config_version_file}
    DESTINATION lib/cmake/Live555
)

install(EXPORT live555Target
    NAMESPACE Live555::
    DESTINATION lib/cmake/Live555
    FILE Live555Config.cmake
)

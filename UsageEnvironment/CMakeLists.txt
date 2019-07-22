find_all_headers_and_sources()
add_library(UsageEnvironment
    SHARED
    ${_public_headers}
    ${_private_headers}
    ${_all_srcs}
)
add_library(live555::UsageEnvironment ALIAS UsageEnvironment)
target_include_directories(UsageEnvironment
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # This is a cyclic dependency :(
        $<BUILD_INTERFACE:${live555_SOURCE_DIR}/groupsock/include>
)
target_link_libraries(UsageEnvironment PUBLIC live555_cxx_flags)
live555_target_version(UsageEnvironment AUTO)
set_target_properties(UsageEnvironment PROPERTIES FOLDER "live555/lib")

install_target(UsageEnvironment "${_public_headers}")

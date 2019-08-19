find_all_headers_and_sources()
add_library(groupsock STATIC
    ${_public_headers}
    ${_private_headers}
    ${_all_srcs}
)
add_library(Live555::groupsock ALIAS groupsock)
target_include_directories(groupsock
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(groupsock
    PUBLIC
    live555_cxx_flags
    BasicUsageEnvironment
)
if(WIN32)
    target_link_libraries(groupsock
        ws2_32 mswsock
    )
endif()
live555_target_version(groupsock AUTO)
set_target_properties(groupsock PROPERTIES FOLDER "Live555/lib")

install_target(groupsock "${_public_headers}")

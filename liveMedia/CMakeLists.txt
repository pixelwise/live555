find_all_headers_and_sources()
add_library(liveMedia
    SHARED
    ${_public_headers}
    ${_private_headers}
    ${_all_srcs}
)
add_library(Live555::liveMedia ALIAS liveMedia)
target_include_directories(liveMedia
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(liveMedia PUBLIC
    live555_cxx_flags
    groupsock
    UsageEnvironment
)

live555_target_version(liveMedia AUTO)
set_target_properties(liveMedia PROPERTIES FOLDER "Live555/lib")

install_target(liveMedia "${_public_headers}")

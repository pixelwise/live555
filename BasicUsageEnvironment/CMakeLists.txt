find_all_headers_and_sources()
add_library(BasicUsageEnvironment STATIC
    ${_public_headers}
    ${_private_headers}
    ${_all_srcs}
)
add_library(Live555::BasicUsageEnvironment ALIAS BasicUsageEnvironment)
target_include_directories(BasicUsageEnvironment
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(BasicUsageEnvironment PUBLIC
    live555_cxx_flags
    UsageEnvironment
)
live555_target_version(BasicUsageEnvironment AUTO)
set_target_properties(BasicUsageEnvironment PROPERTIES FOLDER "Live555/lib")

install_target(BasicUsageEnvironment "${_public_headers}")

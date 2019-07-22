find_all_headers_and_sources()
add_library(groupsock
    SHARED
    ${_public_headers}
    ${_private_headers}
    ${_all_srcs}
)
add_library(live555::groupsock ALIAS groupsock)
target_include_directories(groupsock
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(groupsock
    PUBLIC
    live555_cxx_flags
    UsageEnvironment
)
if(WIN32)
    target_link_libraries(groupsock
        ws2_32 mswsock
    )
endif()
live555_target_version(groupsock AUTO)
set_target_properties(groupsock PROPERTIES FOLDER "live555/lib")

install(TARGETS groupsock EXPORT live555Target
    INCLUDES DESTINATION include/groupsock
    RUNTIME DESTINATION bin/$<CONFIG>
    LIBRARY DESTINATION lib/$<CONFIG>
    ARCHIVE DESTINATION lib/$<CONFIG>
)
install(FILES ${headers}
    DESTINATION include/groupsock
)

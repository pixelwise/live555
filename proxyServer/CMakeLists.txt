add_executable(live555ProxyServer live555ProxyServer.cpp)
target_link_libraries(live555ProxyServer PRIVATE
    live555_cxx_flags
    liveMedia
    BasicUsageEnvironment
)
set_target_properties(live555ProxyServer PROPERTIES FOLDER "Live555/Servers")

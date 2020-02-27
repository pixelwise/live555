include(utils)

function(find_clang_rt_lib_dir startpath out)
    message(STATUS "looking for clang rt lib in ${startpath} pattern libclang_rt.asan*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    file(GLOB_RECURSE _LIBS "${startpath}/libclang_rt.asan*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    message(STATUS "found ${_LIBS}")
    list(GET _LIBS 0 _LIB)
    get_filename_component(_LIB_DIR "${_LIB}" DIRECTORY)
    set (${out} "${_LIB_DIR}" PARENT_SCOPE)
endfunction()

macro(handle_sanitizers)
    if (USE_MSAN)
        # msan cannot be linked if using shared-libasan or shared-libsan
        set(shared_sanitizer no)
        # when use memory sanitizer -Og leads to false negative
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
    elseif(USE_ASAN OR USE_LSAN OR USE_UBSAN)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Og")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Og")
    endif()
    if (USE_ASAN OR USE_LSAN OR USE_MSAN OR USE_UBSAN)
        find_package(
            LLVM REQUIRED CONFIG
        )
        if(NOT LLVM_FOUND)
            message(
                STATUS
                "Cannot find the LLVM package, use -DLLVM_DIR to specify a direcotry path point to LLVMConfig.cmake"
            )
        endif()
        message(
            STATUS
            "LLVM_TOOLS_BINARY_DIR=${LLVM_TOOLS_BINARY_DIR}"
        )
        if(NOT LLVM_SANITIZER_LIB_DIR)
            find_clang_rt_lib_dir(${LLVM_LIBRARY_DIR} LLVM_Clang_RT_LIB_DIR)
        endif()
        set(
            CMAKE_EXE_LINKER_FLAGS
            "${CMAKE_EXE_LINKER_FLAGS} -fPIC"
        )
        if(shared_sanitizer)
            if(LLVM_Clang_RT_LIB_DIR)
                message("LLVM_Clang_RT_LIB_DIR=${LLVM_Clang_RT_LIB_DIR}")
                set(CMAKE_BUILD_RPATH "${LLVM_Clang_RT_LIB_DIR}" ${CMAKE_BUILD_RPATH})
                set(CMAKE_INSTALL_RPATH "${LLVM_Clang_RT_LIB_DIR}" ${CMAKE_INSTALL_RPATH})
            endif()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -fPIC")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -shared-libasan -shared-libsan -fPIC")
        else()
            message(
                STATUS
                "Set -Dshared_sanitizer=yes, when use sanitizer other than USE_MSAN, for more detailed stacktrace if there is unkown modules in sanitizer output!"
            )
        endif()
        if(NOT SAN_SYMBOLIZER)
            set(
                SAN_SYMBOLIZER
                "llvm-symbolizer"
            )
        endif()
        message(
            STATUS
            "LLVM_TOOLS_BINARY_DIR=${LLVM_TOOLS_BINARY_DIR}"
        )
        find_program(
            SAN_SYMBOLIZER_PATH
            ${SAN_SYMBOLIZER}
            ${LLVM_TOOLS_BINARY_DIR}
        )
        if(NOT SAN_SYMBOLIZER_PATH)
            message(FATAL_ERROR "cannot find sanitize symbolizer ${SAN_SYMBOLIZER} in ${LLVM_TOOLS_BINARY_DIR}")
        endif()
    endif()
    if (USE_ASAN)
        message(STATUS "Use ASAN, please use ASAN_OPTIONS env when execute binaries.")
        if(USE_LSAN)
            message(
                WARNING
                "leak sanitizer is already integreated into ASAN. USE_LASN is useless when USE_ASAN=yes!"
            )
        endif()

        if(USE_MSAN)
            message(
                WARNING
                "memory sanitizer cannot be enabled if address sanitizer is enabled. USE_MSAN is suppressed now!"
            )
        endif()

        set(
            sanitizers
            address
        )
        set(
            env_asan_options
            $ENV{ASAN_OPTIONS}
        )
        EXTEND_env(
            "protect_shadow_gap=0:replace_intrin=0:fast_unwind_on_malloc=0:detect_leaks=1"
            env_asan_options
        )
        set(
            ASAN_SYMBOLIZER_PATH
            "${SAN_SYMBOLIZER_PATH}"
        )
        message(
            STATUS
            "env_asan_options=${env_asan_options}"
        )

    elseif (USE_LSAN)
        message(STATUS "Use LSAN, please use LSAN_OPTIONS env when execute binaries. LSAN is also integrated inside the ASAN.")
        if(USE_MSAN)
            message(
                WARNING
                "memory sanitizer cannot be enabled if leak sanitizer is enabled. USE_MSAN is suppressed now!"
            )
        endif()
        set(
            sanitizers
            leak
        )
        set(
            env_lsan_options
            $ENV{LSAN_OPTIONS}
        )
        EXTEND_env(
            "protect_shadow_gap=0:replace_intrin=0:fast_unwind_on_malloc=0:detect_leaks=1"
            env_lsan_options
        )
        set(
            LSAN_SYMBOLIZER_PATH
            "${SAN_SYMBOLIZER_PATH}"
        )
        message(
            STATUS
            "env_lsan_options=${env_lsan_options}"
        )
    elseif (USE_MSAN)
        set(
            sanitizers
            memory
        )
        #memory does not work with address/leak
        #the -fsanitize-recover=memory need to be in both compiler and linker
        set(
            sanitizers_compiler_flags
            "-fsanitize-recover=all"
            "-fno-optimize-sibling-calls"
            "-fsanitize-memory-track-origins"
            "-fsanitize-memory-use-after-dtor"
            "-fPIE"
            "-pie"
            "-fno-elide-constructors"
        )

        set(
            env_msan_options
            $ENV{MSAN_OPTIONS}
        )
        EXTEND_env(
            "protect_shadow_gap=0:replace_intrin=0:fast_unwind_on_malloc=0:halt_on_warning=0"
            env_msan_options
        )
        message(
            STATUS
            "env_msan_options=${env_msan_options}"
        )
        set(
            MSAN_SYMBOLIZER_PATH
            "${SAN_SYMBOLIZER_PATH}"
        )

        if(MSAN_USE_INSTRUMENTED_CXX)
            message(
                STATUS
                "Use instrumented libc++ for memory sanitizer! MSAN_libcxx_ROOT=${MSAN_libcxx_ROOT}"
            )
            set(
                sanitizers_compiler_flags
                "-stdlib=libc++"
            )
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
            set(CMAKE_BUILD_RPATH "${MSAN_libcxx_ROOT}/lib" ${CMAKE_BUILD_RPATH})
            link_directories("${MSAN_libcxx_ROOT}/lib")
            include_directories("${MSAN_libcxx_ROOT}/include" "${MSAN_libcxx_ROOT}/include/c++/v1")
        else()
            message(
                STATUS
                "Set -DMSAN_USE_INSTRUMENTED_CXX=yes to use instrumented libc++ for memory sanitizer! All libs (for example: boost, ffmpeg ...) need to be compiled with -stdlib=libc++ to use the clang's version. Set MSAN_libcxx_ROOT to the path of parent of lib/libc++"
            )
        endif()
    endif()

    if(USE_UBSAN)
        message(STATUS "Use UBSAN, please use UBSAN_OPTIONS env when execute binaries.")
        set(
            sanitizers
            ${sanitizers}
            undefined
        )
        set(
            env_ubsan_options
            $ENV{UBSAN_OPTIONS}
        )
        EXTEND_env(
            "fast_unwind_on_malloc=0:print_stacktrace=1"
            env_ubsan_options
        )
        set(
            UBSAN_SYMBOLIZER_PATH
            "${SAN_SYMBOLIZER_PATH}"
        )
        message(
            STATUS
            "env_ubsan_options=${env_ubsan_options}"
        )
    endif()
    if (sanitizers)
        join_list( sanitizers "," sanitizers)
        set(
            sanitizers_compiler_flags
            "-fsanitize=${sanitizers}"
            ${sanitizers_compiler_flags}

        )
        join_list( sanitizers_compiler_flags " " sanitizers_compiler_flags)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${sanitizers_compiler_flags}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${sanitizers_compiler_flags}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${sanitizers_compiler_flags}")
        set(
            test_envs
            ${test_envs}
            "MSAN_SYMBOLIZER_PATH=${MSAN_SYMBOLIZER_PATH}"
            "LSAN_SYMBOLIZER_PATH=${LSAN_SYMBOLIZER_PATH}"
            "ASAN_SYMBOLIZER_PATH=${ASAN_SYMBOLIZER_PATH}"
            "UBSAN_SYMBOLIZER_PATH=${UBSAN_SYMBOLIZER_PATH}"
            "ASAN_OPTIONS=${env_asan_options}"
            "LSAN_OPTIONS=${env_lsan_options}"
            "MSAN_OPTIONS=${env_msan_options}"
            "UBSAN_OPTIONS=${env_ubsan_options}"
        )
        set(
            test_envs_exports
            ${test_envs_exports}
            "MSAN_SYMBOLIZER_PATH=${MSAN_SYMBOLIZER_PATH}"
            "LSAN_SYMBOLIZER_PATH=${LSAN_SYMBOLIZER_PATH}"
            "ASAN_SYMBOLIZER_PATH=${ASAN_SYMBOLIZER_PATH}"
            "UBSAN_SYMBOLIZER_PATH=${UBSAN_SYMBOLIZER_PATH}"
            "ASAN_OPTIONS=${env_asan_options}:\${ASAN_OPTIONS}"
            "LSAN_OPTIONS=${env_lsan_options}:\${LSAN_OPTIONS}"
            "MSAN_OPTIONS=${env_msan_options}:\${MSAN_OPTIONS}"
            "UBSAN_OPTIONS=${env_ubsan_options}:\${UBSAN_OPTIONS}"
        )
    endif()

endmacro()

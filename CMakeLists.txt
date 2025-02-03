
cmake_minimum_required(VERSION 3.16)
include(CMakePrintHelpers)
include(GNUInstallDirs)

project(libasrt VERSION 1.0.0 LANGUAGES CXX)
message(STATUS "********************** Building project libasrt **********************")

# ---------------------------------------------------------------------------------------
# Common configurations
# ---------------------------------------------------------------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD "20")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------------------
# Toolchain configuration
# ---------------------------------------------------------------------------------------
set(CROSS_COMPILE_FOR_ARM OFF)
if(CROSS_COMPILE_FOR_ARM)
    message(STATUS "Cross compiling for arm")
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain_arm.cmake)
else()
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain_x64.cmake)
endif()

cmake_print_variables(
    CMAKE_SYSTEM_NAME 
    CMAKE_SYSTEM_PROCESSOR 
    CMAKE_C_COMPILER 
    CMAKE_C_COMPILER_VERSION 
    CMAKE_CXX_COMPILER 
    CMAKE_CXX_COMPILER_VERSION
)

# ---------------------------------------------------------------------------------------
# Build Configuration: Release/Debug/RelWithDebInfo/MinSizeRel
# ---------------------------------------------------------------------------------------
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})

# ---------------------------------------------------------------------------------------
# project build options
# ---------------------------------------------------------------------------------------
option(ASRT_BUILD_STATIC "Build static library" OFF)
option(ASRT_BUILD_SHARED "Build shared library" OFF)
option(ASRT_BUILD_EXECUTION "Build execution library" ON)
option(ASRT_BUILD_EXAMPLE "Build examples" ON)
option(ASRT_NO_EXCEPTIONS "Build without exceptions" OFF)
option(ASRT_USE_STD_EXPECTED "Use std::expected instead of bundled expected library." OFF)
set(SPDLOG_LOG_LEVEL SPDLOG_LEVEL_TRACE) # SPDLOG_LEVEL_DEBUG SPDLOG_LEVEL_TRACE
set(SINGLE_THREADED FALSE)

cmake_print_variables(
    ASRT_BUILD_STATIC
    ASRT_BUILD_SHARED
    ASRT_BUILD_EXECUTION
    ASRT_BUILD_EXAMPLE
    ASRT_NO_EXCEPTIONS
    ASRT_USE_STD_EXPECTED
    SPDLOG_LOG_LEVEL
    SINGLE_THREADED
    )

# ---------------------------------------------------------------------------------------
# Validation and configuration
# ---------------------------------------------------------------------------------------
if(ASRT_BUILD_STATIC AND ASRT_BUILD_SHARED)
    message(FATAL_ERROR "Cannot build both static and shared library")
endif()

set(ASRT_HEADER_ONLY ON)
if(ASRT_BUILD_STATIC OR ASRT_BUILD_SHARED OR BUILD_SHARED_LIBS)
    set(ASRT_HEADER_ONLY OFF)
endif()

if(ASRT_USE_STD_EXPECTED)
    if(CMAKE_CXX_STANDARD LESS 23 OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12)
        message(FATAL_ERROR "std::expected requires C++23 and g++ >= 12")
    endif()
else()
    message(STATUS "Using bundled tl::expected")
endif()

# ---------------------------------------------------------------------------------------
# Common target configuration
# ---------------------------------------------------------------------------------------
function(configure_asrt_target target scope_keyword compile_option_scope_keyword)

    target_compile_definitions(${target}
        ${scope_keyword}
            $<$<BOOL:${ASRT_NO_EXCEPTIONS}>:TL_NO_EXCEPTIONS>
            $<$<BOOL:${ASRT_NO_EXCEPTIONS}>:SPDLOG_NO_EXCEPTIONS>
            SPDLOG_ACTIVE_LEVEL=${SPDLOG_LOG_LEVEL}
            $<$<BOOL:${SINGLE_THREADED}>:APPLICATION_IS_SINGLE_THREADED>
    )

    target_compile_options(${target}
        ${compile_option_scope_keyword}
            -Wall -Werror -Wshadow
            -Wpessimizing-move -Wredundant-move
            -fdiagnostics-color=always
            $<$<BOOL:${ASRT_NO_EXCEPTIONS}>:-fno-exceptions>
    )

    target_include_directories(${target}
        ${scope_keyword}
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

    target_link_libraries(${target} ${scope_keyword} pthread)

    foreach(option IN ITEMS
        ASRT_NO_EXCEPTIONS
        ASRT_USE_STD_EXPECTED
        ASRT_BUILD_EXECUTION
    )
        if(${option})
            target_compile_definitions(${target} ${scope_keyword} ${option})
        endif()
    endforeach()
endfunction()

# ---------------------------------------------------------------------------------------
# Target creation
# ---------------------------------------------------------------------------------------
if(ASRT_HEADER_ONLY)
    message(STATUS "Building asrt as a header-only library")
    add_library(asrt INTERFACE)
    target_compile_definitions(asrt INTERFACE ASRT_HEADER_ONLY)
    configure_asrt_target(asrt INTERFACE INTERFACE)
else()
    # Shared/static configuration
    if(ASRT_BUILD_SHARED)
        message(STATUS "Building asrt as a shared library")
        add_library(asrt SHARED src/asrt.cpp)
        target_compile_definitions(asrt PUBLIC ASRT_SHARED_LIB)
        target_compile_definitions(asrt PRIVATE ASRT_EXPORTS)
        target_compile_options(asrt PRIVATE -fvisibility=hidden)
        set_target_properties(asrt PROPERTIES
            OUTPUT_NAME "asrt"
            POSITION_INDEPENDENT_CODE ON
            VISIBILITY_INLINES_HIDDEN ON # TODO this is redundant
        )
    else()
        message(STATUS "Building asrt as a static library")
        add_library(asrt STATIC src/asrt.cpp)
    endif()

    configure_asrt_target(asrt PUBLIC PRIVATE)
    set_target_properties(asrt PROPERTIES
        LINK_OPTIONS "$<$<CONFIG:RELEASE>:-s>"
    )
endif()

# ---------------------------------------------------------------------------------------
# Build/Install configurations
# ---------------------------------------------------------------------------------------

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(ASRT_HEADER_ONLY)
    install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/asrt 
        DESTINATION include)
else()
    install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/asrt 
        DESTINATION include
        FILES_MATCHING
        PATTERN "impl" EXCLUDE
    )
endif()

#set(INSTALL_BIN_DESTINATION lib/$<IF:$<BOOL:${CROSS_COMPILE_FOR_ARM}>,arm,x64>)
install(TARGETS asrt EXPORT asrt-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
    )

install(EXPORT asrt-targets
    FILE asrt-config.cmake
    NAMESPACE asrt::
    DESTINATION lib/cmake/asrt
    )

if(ASRT_BUILD_EXAMPLE)
    message(STATUS "Building examples...")
    add_subdirectory(examples)
endif()
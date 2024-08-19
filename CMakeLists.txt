
cmake_minimum_required(VERSION 3.16)
include(CMakePrintHelpers)

project(libasrt)
message(STATUS "********************** Building project libasrt **********************")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD "20")

# ---------------------------------------------------------------------------------------
# Cross-compile options
# ---------------------------------------------------------------------------------------
set(CROSS_COMPILE_FOR_ARM OFF)
if(CROSS_COMPILE_FOR_ARM)
    message(STATUS "Cross compiling for arm")
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain_arm.cmake)
else()
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain_x64.cmake)
endif()

cmake_print_variables(CMAKE_SYSTEM_NAME)
cmake_print_variables(CMAKE_SYSTEM_PROCESSOR)
cmake_print_variables(CMAKE_C_COMPILER)
cmake_print_variables(CMAKE_C_COMPILER_VERSION)
cmake_print_variables(CMAKE_CXX_COMPILER)
cmake_print_variables(CMAKE_CXX_COMPILER_VERSION)

# ---------------------------------------------------------------------------------------
# Build Configuration: Release/Debug/RelWithDebInfo/MinSizeRel
# ---------------------------------------------------------------------------------------
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})


# ---------------------------------------------------------------------------------------
# build options
# ---------------------------------------------------------------------------------------
# option to build static library
option(LIBASRT_BUILD_STATIC "Build static library" ON)

# option to build shared library
option(LIBASRT_BUILD_SHARED "Build shared library" OFF)

# option to build example
option(LIBASRT_BUILD_EXAMPLE "Build examples" ON)

# option to build this library with exceptions disabled
option(LIBASRT_NO_EXCEPTIONS "Build without exceptions" OFF)

# option to build library with std::expected
option(LIBASRT_USE_STD_EXPECTED "Use std::expected instead of bundled expected library." OFF)

# option to build library with ara::core language features
option(LIBASRT_USE_ARA_VECTOR_LANG_FEATURES "Use ara::core language features instead of std ones" OFF)

if(LIBASRT_USE_STD_EXPECTED)
    if((NOT (${CMAKE_CXX_STANDARD} GREATER_EQUAL "23")) OR (NOT (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL "12")) )
        message(FATAL_ERROR "std::expected is only available with a g++-12 compiler with std=c++23")
    endif()
else()
    message(STATUS "Using bundled tl::expected")
endif()

# ---------------------------------------------------------------------------------------
# Project (compile-time) log level & concurrency settings
# ---------------------------------------------------------------------------------------
set(SPDLOG_LOG_LEVEL SPDLOG_LEVEL_TRACE)
set(SINGLE_THREADED FALSE)
# #set(SPDLOG_LOG_LEVEL SPDLOG_LEVEL_DEBUG)
# #set(SPDLOG_LOG_LEVEL SPDLOG_LEVEL_TRACE)
cmake_print_variables(SPDLOG_LOG_LEVEL)
cmake_print_variables(SINGLE_THREADED)

# ---------------------------------------------------------------------------------------
# Target build & install settings
# ---------------------------------------------------------------------------------------
set(LIBASRT_SRCS )

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(INSTALL_DIRECTORY ${CMAKE_SOURCE_DIR}/install)
set(INSTALL_DESTINATION ${INSTALL_DIRECTORY}/lib/$<IF:$<BOOL:${CROSS_COMPILE_FOR_ARM}>,arm,x64>)

# # Build static library
# if(LIBASRT_BUILD_STATIC)
#     message(STATUS "Building static library...")
#     set(TARGET_NAME diag)

#     add_library(${TARGET_NAME} STATIC ${LIBASRT_SRCS})
#     add_library(diag::${TARGET_NAME} ALIAS ${TARGET_NAME})

#     target_compile_options(${TARGET_NAME}
#         PRIVATE 
#             -Wall -Werror -Wshadow -Wpessimizing-move -Wredundant-move -fdiagnostics-color=always
#             $<$<BOOL:${LIBASRT_NO_EXCEPTIONS}>:-fno-exceptions>)

#     target_compile_definitions(${TARGET_NAME} PUBLIC
#         $<$<BOOL:${LIBASRT_NO_EXCEPTIONS}>:TL_NO_EXCEPTIONS>
#         $<$<BOOL:${LIBASRT_NO_EXCEPTIONS}>:SPDLOG_NO_EXCEPTIONS>
#         SPDLOG_ACTIVE_LEVEL=${SPDLOG_LOG_LEVEL}
#         $<$<BOOL:${SINGLE_THREADED}>:APPLICATION_IS_SINGLE_THREADED>)

#     target_include_directories(${TARGET_NAME} 
#         PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
#         PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/lib>"
#         PUBLIC "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
#     )

#     target_link_libraries(${TARGET_NAME} 
#         PUBLIC pthread
#     )

#     set_target_properties(${TARGET_NAME} PROPERTIES
#         LINK_OPTIONS $<$<CONFIG:RELEASE>:-s>
#     )

#     install(TARGETS ${TARGET_NAME} 
#         DESTINATION ${INSTALL_DESTINATION})
# endif()

# # Build shared library
# if(LIBASRT_BUILD_SHARED OR BUILD_SHARED_LIBS)
#     message(STATUS "Building shared library...")
#     set(TARGET_NAME diag_shared)

#     add_library(${TARGET_NAME} SHARED ${LIBASRT_SRCS})
#     add_library(diag::${TARGET_NAME} ALIAS ${TARGET_NAME})

#     target_compile_options(${TARGET_NAME}
#         PRIVATE 
#             -Wall -Werror -Wshadow -Wpessimizing-move -Wredundant-move -fdiagnostics-color=always
#             -fvisibility=hidden
#             $<$<BOOL:${LIBASRT_NO_EXCEPTIONS}>:-fno-exceptions>)

#     target_compile_definitions(${TARGET_NAME} PUBLIC
#         LIBASRT_SHARED_LIB
#         $<$<BOOL:${LIBASRT_NO_EXCEPTIONS}>:TL_NO_EXCEPTIONS>
#         $<$<BOOL:${LIBASRT_NO_EXCEPTIONS}>:SPDLOG_NO_EXCEPTIONS>
#         SPDLOG_ACTIVE_LEVEL=${SPDLOG_LOG_LEVEL}
#         $<$<BOOL:${SINGLE_THREADED}>:APPLICATION_IS_SINGLE_THREADED>)

#     target_include_directories(${TARGET_NAME}
#         PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
#         PRIVATE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/lib>"
#         PUBLIC "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
#     )

#     target_link_libraries(${TARGET_NAME} 
#         PUBLIC pthread
#     )

#     set_target_properties(${TARGET_NAME} PROPERTIES
#         LINK_OPTIONS $<$<CONFIG:RELEASE>:-s>
#         OUTPUT_NAME "diag"
#     )

#     install(TARGETS ${TARGET_NAME} 
#         DESTINATION ${INSTALL_DESTINATION})    
# endif()

# # Compile definition based on cmake options
# foreach(LIBASRT_OPTION
#     LIBASRT_NO_EXCEPTIONS
#     LIBASRT_USE_STD_EXPECTED
#     LIBASRT_USE_ARA_VECTOR_LANG_FEATURES)
#     if(${LIBASRT_OPTION})
#         if(LIBASRT_BUILD_STATIC)
#             target_compile_definitions(diag PUBLIC ${SPDLOG_OPTION})
#         endif()

#         if(LIBASRT_BUILD_SHARED OR BUILD_SHARED_LIBS)
#             target_compile_definitions(diag_shared PUBLIC ${SPDLOG_OPTION})
#         endif()
#     endif()
# endforeach()

if(LIBASRT_BUILD_EXAMPLE)
    message(STATUS "Building examples...")
    add_subdirectory(example)
endif()

# ---------------------------------------------------------------------------------------
# Install header files
# ---------------------------------------------------------------------------------------
# install(FILES 
#         ${CMAKE_CURRENT_LIST_DIR}/include/asrt/****.h
#         #$<$<BOOL:${LIBIAG_INSTALL_BUNDLED_EXPECTED}>:${CMAKE_CURRENT_LIST_DIR}/lib/expected.hpp>
#         DESTINATION ${INSTALL_DIRECTORY}/include/asrt)

install(FILES ${CMAKE_CURRENT_LIST_DIR}/data/someip_config.json
        DESTINATION ${INSTALL_DIRECTORY}/bin)
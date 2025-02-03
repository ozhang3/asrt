# ---------------------------------------------------------------------------------------
# Set source files to compile
# ---------------------------------------------------------------------------------------

#file(GLOB_RECURSE EXAMPLE_FILE_LIST CONFIGURE_DEPENDS "*.cpp")

SET(EXAMPLE_FILE_LIST
    #libdiag_client_demo
    #libdiag_server_demo.c
    #libdiag_server_demo.cpp
    #libdiag_client_demo.c
    multicast_sender.cpp
    #sd_example.cpp
    #udp_server.cpp
    #sender_receiver.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/lib)

foreach(target_file_name ${EXAMPLE_FILE_LIST})
    get_filename_component(target_name ${target_file_name} NAME_WE)

    add_executable(${target_name} ${target_file_name})

    target_link_libraries(${target_name} PRIVATE asrt)

    set_target_properties(${target_name} PROPERTIES
    #   OUTPUT_NAME $<IF:$<BOOL:${CROSS_COMPILE_FOR_ARM}>,${target_name}_arm,${target_name}>
        OUTPUT_NAME "${target_name}"
        INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib
    )

    install(TARGETS ${target_name} DESTINATION examples)
endforeach()

if(CMAKE_GENERATOR MATCHES "Visual Studio|Xcode")
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${example_names})
endif()
# ---------------------------------------------------------------------------------------
# Set source files to compile
# ---------------------------------------------------------------------------------------
SET(EXAMPLE_FILE_LIST
    #libdiag_client_demo
    #libdiag_server_demo.c
    #libdiag_server_demo.cpp
    #libdiag_client_demo.c
    #multicast_sender.cpp
    #sd_example.cpp
    #udp_server.cpp
)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

foreach(target_file_name ${EXAMPLE_FILE_LIST})
    get_filename_component(target_name ${target_file_name} NAME_WE)

    add_executable(${target_name} ${target_file_name})

    target_include_directories(${target_name} PRIVATE 
        ${CMAKE_SOURCE_DIR}/lib)

    if(LIBDIAG_BUILD_STATIC)
        target_link_libraries(${target_name} PRIVATE diag::diag)
    else()
        target_link_libraries(${target_name} PRIVATE diag::diag_shared)
        set_target_properties(${target_name} PROPERTIES
            INSTALL_RPATH ${INSTALL_DESTINATION}
        )
    endif()

    set_target_properties(${target_name} PROPERTIES
        OUTPUT_NAME $<IF:$<BOOL:${CROSS_COMPILE_FOR_ARM}>,${target_name}_arm,${target_name}>
    )

    install(TARGETS ${target_name} DESTINATION ${INSTALL_DIRECTORY}/bin)

endforeach()

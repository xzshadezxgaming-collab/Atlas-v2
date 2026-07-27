add_executable(Atlas
    main.cpp
)

target_link_libraries(Atlas
    PRIVATE
        AtlasEngine
)

# Keep a copy of the assets next to the executable so the game can find
# them regardless of the working directory it is launched from.
add_custom_command(TARGET Atlas POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Assets
        $<TARGET_FILE_DIR:Atlas>/Assets
)

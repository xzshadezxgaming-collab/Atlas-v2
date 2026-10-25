add_executable(AtlasSimTests
    SimTests.cpp
)

target_link_libraries(AtlasSimTests
    PRIVATE
        AtlasEngine
)

if(WIN32)
    add_custom_command(TARGET AtlasSimTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/ThirdParty/sdl3/lib/x64/SDL3.dll
            $<TARGET_FILE_DIR:AtlasSimTests>
    )
endif()

add_test(NAME SimTests COMMAND AtlasSimTests)

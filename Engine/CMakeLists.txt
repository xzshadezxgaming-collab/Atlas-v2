add_library(AtlasEngine STATIC
    Core/Application.cpp
    Core/IniFile.cpp
    Core/Window.cpp

    Actors/Actor.cpp
    Actors/AIController.cpp
    Actors/Limb.cpp

    Audio/Audio.cpp

    Combat/Explosion.cpp
    Combat/Grenade.cpp
    Combat/Weapon.cpp

    Particles/ParticleSystem.cpp

    Input/Input.cpp

    Graphics/Background.cpp
    Graphics/PixelFont.cpp
    Graphics/Texture2D.cpp
    Graphics/Sprite.cpp
    Graphics/Camera.cpp

    World/Terrain.cpp

    Physics/RigidBody.cpp

    ECS/Transform.cpp
    ECS/Entity.cpp
)

target_compile_features(AtlasEngine PUBLIC cxx_std_23)

target_include_directories(AtlasEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
    # Windows uses the SDL3 binaries vendored in ThirdParty. Drop
    # SDL3.lib/SDL3.dll into ThirdParty/sdl3/lib/x64 (they are gitignored);
    # see ThirdParty/sdl3/INSTALL.md.
    target_include_directories(AtlasEngine
        PUBLIC
            ${CMAKE_SOURCE_DIR}/ThirdParty/sdl3/include
    )

    target_link_libraries(AtlasEngine
        PUBLIC
            "${CMAKE_SOURCE_DIR}/ThirdParty/sdl3/lib/x64/SDL3.lib"
    )

    set(SDL3_DLL "${CMAKE_SOURCE_DIR}/ThirdParty/sdl3/lib/x64/SDL3.dll")

    add_custom_command(TARGET AtlasEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SDL3_DLL}
            $<TARGET_FILE_DIR:AtlasEngine>
    )
else()
    # Other platforms use an installed SDL3 (package manager or built from
    # source: https://github.com/libsdl-org/SDL).
    find_package(SDL3 REQUIRED CONFIG)

    target_link_libraries(AtlasEngine
        PUBLIC
            SDL3::SDL3
    )
endif()

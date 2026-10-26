add_library(AtlasEngine STATIC
    Core/Application.cpp
    Core/Settings.cpp
    Core/BuyCart.cpp
    UI/Ui.cpp
    Core/IniFile.cpp
    Core/Window.cpp

    Actors/Actor.cpp
    Actors/AIController.cpp
    Actors/BodyPart.cpp
    Actors/Limb.cpp

    Audio/Audio.cpp

    Combat/Delivery.cpp
    Combat/Explosion.cpp
    Combat/Grenade.cpp
    Combat/Weapon.cpp

    Particles/ParticleSystem.cpp

    Input/Input.cpp

    Graphics/Background.cpp
    Graphics/PixelFont.cpp
    Graphics/Texture2D.cpp
    Graphics/Sprite.cpp
    Graphics/SpriteBank.cpp
    Graphics/Camera.cpp
    Graphics/MapThumbnail.cpp

    World/Terrain.cpp

    Physics/RigidBody.cpp

    Sim/World.cpp

    Net/Protocol.cpp
    Net/Session.cpp

    ECS/Transform.cpp
    ECS/Entity.cpp
)

target_compile_features(AtlasEngine PUBLIC cxx_std_23)

target_include_directories(AtlasEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# stb_truetype (vendored, public domain): rasterizes the UI fonts.
target_include_directories(AtlasEngine
    SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/ThirdParty/stb
)

# ENet (vendored in ThirdParty/enet): reliable UDP for multiplayer.
target_include_directories(AtlasEngine
    PRIVATE
        ${CMAKE_SOURCE_DIR}/ThirdParty/enet/include
)

target_link_libraries(AtlasEngine
    PRIVATE
        enet
)

if(WIN32)
    target_link_libraries(AtlasEngine PRIVATE ws2_32 winmm)
endif()

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

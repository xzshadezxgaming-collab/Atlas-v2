#pragma once

#include "../Audio/Audio.h"

#include <cstdint>

namespace Atlas
{
    enum class SimEventType : std::uint8_t
    {
        Sound,        // Sound at Volume
        GunFired,     // Player fired a gun (crosshair bloom)
        HardLanding,  // Player landed hard; Amount = impact 0..1
        Explosion,    // a grenade went off at X, Y
        EnemyKilled,  // an enemy died at X, Y
        Damage,       // an enemy took Value damage at X, Y
        WaveStarted,  // Value = wave number
        BrainUnderAttack, // Value = team whose brain is taking damage
        BrainDestroyed,   // Value = team; at X, Y
        MatchOver,        // Value = winning team
        BodySwitched,     // Player took over another body
        EnemyDropIncoming, // Value = how many, at X
    };

    // Something that happened during a tick that presentation cares
    // about (sound, camera shake, floating text). The simulation only
    // reports these; what to do with them - and whether they matter to
    // the local player - is the client's call. This keeps audio, camera
    // and HUD out of the World so it can run headless or on a server.
    struct SimEvent
    {
        SimEventType Type = SimEventType::Sound;

        int Player = -1; // player slot it concerns, -1 = none

        Sfx Sound = Sfx::Shot;
        float Volume = 1.0f;

        float X = 0.0f;
        float Y = 0.0f;
        float Amount = 0.0f;
        int Value = 0;
    };
}

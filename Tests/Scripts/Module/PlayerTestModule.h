#pragma once

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Script/GameBehaviour.h>

// A C++ script of the player tests' game module (PlayerTestModule: a shared library the tests put into the data
// folder's Modules/). It writes its messages to the log the tests read.
namespace helix::player::tests
{
    inline LogChannel ModuleLog{"PlayerTestModule"};

    HX_CLASS(Namespace = "PlayerTests")
    class Spinner : public engine::GameBehaviour
    {
        void Awake() { HX_LOG_INFO(ModuleLog, "Spinner.Awake on {}", gameObject()->Name()); }
        void Start() { HX_LOG_INFO(ModuleLog, "Spinner.Start"); }
        void Update()
        {
            if (++updates_ == 2)
                HX_LOG_INFO(ModuleLog, "Spinner.Update twice");
        }
        void OnDestroy() { HX_LOG_INFO(ModuleLog, "Spinner.OnDestroy"); }
        // Sent by the C# script.
        void Hello() { HX_LOG_INFO(ModuleLog, "Spinner.Hello"); }

        u32 updates_ = 0;
    };
}

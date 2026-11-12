using Helix.Scripting;
using HelixEngine;

namespace PlayerTests
{
    // A C# script of the player tests' data folder (Managed/HelixPlayer.TestScripts.dll), beside a C++ one.
    public class Greeter : GameBehaviour
    {
        void Awake() => Host.Log(LogLevel.Info, "Greeter.Awake on " + gameObject.name);

        void Start()
        {
            Host.Log(LogLevel.Info, "Greeter.Start");
            // To the C++ script of the same GameObject.
            SendMessage("Hello");
        }

        void OnDestroy() => Host.Log(LogLevel.Info, "Greeter.OnDestroy");
    }
}

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "../../src/compiler/Compiler.hpp"
#include "../../src/core/Engine.hpp"

namespace
{
    bool RunWait(decl_audio::Engine &engine, const std::uint32_t milliseconds)
    {
        using namespace std::chrono_literals;

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        while (std::chrono::steady_clock::now() < deadline)
        {
            engine.Update();
            std::this_thread::sleep_for(16ms);
        }

        return true;
    }

    std::filesystem::path GetDefaultBankPath()
    {
        return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "tests" / "data" / "PlaybackBehaviorBank.json";
    }

    std::filesystem::path GetPhase7BankPath()
    {
        return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "tests" / "data" / "ParameterForwardingBehaviorBank.json";
    }
    std::filesystem::path GetSpatializationBankPath()
    {
        return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "tests" / "data" / "SpatializationBehaviorBank.json";
    }

    void PrintSection(const char *label, const char *description)
    {
        std::cout << '\n'
                  << label << '\n';
        std::cout << description << '\n';
    }

    bool RunOneShotTest(decl_audio::Engine &engine, const decl_audio::compiler::CompiledBank &bank)
    {
        PrintSection("OneShot", "You should hear one short hit, then silence.");
        engine.SubmitCreateInstanceForTesting(1001, bank.GetProgramId("playback.oneshot"));
        return RunWait(engine, 1000);
    }

    bool RunLoopTest(decl_audio::Engine &engine, const decl_audio::compiler::CompiledBank &bank)
    {
        constexpr decl_audio::playback::InstanceId kLoopInstanceId = 1002;

        PrintSection("Loop", "You should hear a steady loop for two seconds, then it should stop cleanly.");
        engine.SubmitCreateInstanceForTesting(kLoopInstanceId, bank.GetProgramId("playback.loop"));
        if (!RunWait(engine, 2000))
        {
            return false;
        }

        engine.SubmitRequestStopForTesting(kLoopInstanceId);
        return RunWait(engine, 1000);
    }

    bool RunRandomTest(decl_audio::Engine &engine, const decl_audio::compiler::CompiledBank &bank)
    {
        PrintSection("Random", "You should hear three short hits. They may alternate between the two source assets.");
        engine.SubmitCreateInstanceForTesting(1003, bank.GetProgramId("playback.random"));
        if (!RunWait(engine, 700))
        {
            return false;
        }

        engine.SubmitCreateInstanceForTesting(1004, bank.GetProgramId("playback.random"));
        if (!RunWait(engine, 700))
        {
            return false;
        }

        engine.SubmitCreateInstanceForTesting(1005, bank.GetProgramId("playback.random"));
        return RunWait(engine, 700);
    }

    bool RunParamForwardingTest(decl_audio::Engine &engine)
    {
        const std::filesystem::path bank_path = GetPhase7BankPath();
        PrintSection("Param Forwarding", "You should hear a quiet loop first, then a louder loop after the runtime volume parameter changes.");

        if (!engine.LoadBehaviors(bank_path.string().c_str()))
        {
            std::cerr << decl_audio::compiler::DumpDiagnostics(engine.GetLoadDiagnostics());
            return false;
        }

        engine.SetValue("player", "volume", 0.2f);
        engine.SetTag("player", "resolver.active");
        engine.Update();
        if (!RunWait(engine, 1000))
        {
            return false;
        }

        engine.SetValue("player", "volume", 1.0f);
        engine.Update();
        if (!RunWait(engine, 1000))
        {
            return false;
        }

        engine.RemoveTag("player", "resolver.active");
        engine.Update();
        return RunWait(engine, 700);
    }
    bool RunPanningTest(decl_audio::Engine &engine)
    {
        const std::filesystem::path bank_path = GetSpatializationBankPath();
        PrintSection("Panning Test", "you should hear a sound on your right");

        if (!engine.LoadBehaviors(bank_path.string().c_str()))
        {
            std::cerr << decl_audio::compiler::DumpDiagnostics(engine.GetLoadDiagnostics());
            return false;
        }
        engine.SetListenerPosition(-1, 0, 0);
        engine.SetPosition("player", 3, 0, 0);
        engine.SetTag("player", "spatial.mono");
        engine.Update();
        if (!RunWait(engine, 500))
        {
            return false;
        }
        engine.SetListenerPosition(-1, 0, 0);
        engine.SetPosition("player", 1, 0, 0);
        engine.Update();
        if (!RunWait(engine, 500))
        {
            return false;
        }
        PrintSection("Panning Test", "you should hear a sound on your left");

        engine.SetListenerPosition(1, 0, 0);
        engine.SetPosition("player", -1, 0, 0);
        engine.Update();
        if (!RunWait(engine, 500))
        {
            return false;
        }
        engine.SetListenerPosition(1, 0, 0);
        engine.SetPosition("player", -3, 0, 0);
        engine.Update();
        if (!RunWait(engine, 500))
        {
            return false;
        }
        engine.RemoveTag("player", "spatial.mono");
        engine.Update();
        return RunWait(engine, 700);
    }

    bool RunTransientTagTest(decl_audio::Engine &engine)
    {
        const std::filesystem::path bank_path = GetSpatializationBankPath();
        PrintSection("TransientTag Test", "you should hear a sound");

        if (!engine.LoadBehaviors(bank_path.string().c_str()))
        {
            std::cerr << decl_audio::compiler::DumpDiagnostics(engine.GetLoadDiagnostics());
            return false;
        }
        engine.SetListenerPosition(0, 0, 0);
        engine.SetPosition("player", 0, 0, 0);
        engine.SetTransientTag("player", "spatial.mono");
        engine.Update();
        return RunWait(engine, 700);
    }
} // namespace

int main(int argc, char **argv)
{
    if (argc > 2)
    {
        std::cerr << "Usage: Decl_Audio.SandboxCLI [behavior-bank.json]\n";
        return 1;
    }

    const std::filesystem::path bank_path = (argc == 2) ? std::filesystem::path(argv[1]) : GetDefaultBankPath();

    EngineConfig config{};
    Init(&config);
    config.audio.backend = DECL_AUDIO_BACKEND_PLATFORM_DEFAULT;

    decl_audio::Engine engine(config);
    std::cout << "Loading bank: " << bank_path << '\n';
    if (!engine.LoadBehaviors(bank_path.string().c_str()))
    {
        std::cerr << decl_audio::compiler::DumpDiagnostics(engine.GetLoadDiagnostics());
        return 1;
    }

    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(bank_path);
    if (compile_result.HasErrors())
    {
        std::cerr << decl_audio::compiler::DumpDiagnostics(compile_result.diagnostics);
        return 1;
    }

    // if (!RunOneShotTest(engine, compile_result.bank))
    //     return 1;

    // if (!RunLoopTest(engine, compile_result.bank))
    //     return 1;

    // if (!RunRandomTest(engine, compile_result.bank))
    //     return 1;

    // if (!RunParamForwardingTest(engine))
    //     return 1;

    // if (!RunPanningTest(engine))
    //     return 1;

    if (!RunTransientTagTest(engine))
        return 1;

    return 0;
}

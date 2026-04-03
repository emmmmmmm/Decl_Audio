#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>

#include "../../src/compiler/Compiler.hpp"
#include "../../src/core/Engine.hpp"

namespace fs = std::filesystem;
namespace
{

    struct CliOptions
    {
        fs::path bankPath = fs::current_path();
        fs::path assetPath = fs::current_path();
        bool autoTests = false;
    };

    void PrintUsage(const char *exeName)
    {
        std::cout
            << "Usage: " << exeName << " [options]\n"
            << "  --banks <path>        Asset root directory\n"
            << "  --automatedTesting     Starts auto-testing routine\n"
            << "  --help                 Show this help\n";
    }

    void PrintPromptHelp()
    {
        std::cout
            << "Commands:\n"
            << "  help\n"
            << "  tag <entity> <tag>\n"
            << "  clear <entity> <tag>\n"
            << "  transient <entity> <tag>\n"
            << "  float <entity> <key> <value>\n"
            << "  string <entity> <key> <value...>\n"
            << "  pos <entity> <x> <y> <z>\n"
            << "  quat <entity> <a> <b> <c> <d>\n"
            << "  transform <entity> <x> <y> <z> <a> <b> <c> <d>\n"
            << "  dump\n"
            << "  exit\n";
    }

    bool ParseArgs(int argc, char **argv, CliOptions &options, bool &shouldExit)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];

            if (arg == "--help" || arg == "-h")
            {
                PrintUsage(argv[0]);
                shouldExit = true;
                return false;
            }
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for " << arg << '\n';
                return false;
            }

            const std::string value = argv[++i];
            if (arg == "--assets")
            {
                options.assetPath = value;
            }
            else if (arg == "--banks")
            {
                options.bankPath = value;
            }
            else if (arg == "--automatedTesting")
            {
                options.autoTests = true;
            }
            else
            {
                std::cerr << "Unknown option: " << arg << '\n';
                return false;
            }
        }

        return true;
    }

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

    fs::path GetDefaultBankPath()
    {
        return fs::path(__FILE__).parent_path().parent_path().parent_path() / "tests" / "data" / "PlaybackBehaviorBank.json";
    }

    fs::path GetPhase7BankPath()
    {
        return fs::path(__FILE__).parent_path().parent_path().parent_path() / "tests" / "data" / "ParameterForwardingBehaviorBank.json";
    }
    fs::path GetSpatializationBankPath()
    {
        return fs::path(__FILE__).parent_path().parent_path().parent_path() / "tests" / "data" / "SpatializationBehaviorBank.json";
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
        const fs::path bank_path = GetPhase7BankPath();
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
        const fs::path bank_path = GetSpatializationBankPath();
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
        const fs::path bank_path = GetSpatializationBankPath();
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

    bool ProcessCommand(decl_audio::Engine &engine, const std::string &line)
    {
        std::istringstream input(line);
        std::string command;
        input >> command;

        if (command.empty())
        {
            return true;
        }
        if (command == "help")
        {
            PrintPromptHelp();
            return true;
        }
        if (command == "exit" || command == "quit")
        {
            return false;
        }
        if (command == "dump")
        {
            engine.GetDebugSnapshot();
            return true;
        }
        if (command == "tag")
        {
            std::string entity;
            std::string tag;
            if (input >> entity >> tag)
            {
                engine.SetTag(entity.c_str(), tag.c_str());
                return true;
            }
        }
        else if (command == "clear")
        {
            std::string entity;
            std::string tag;
            if (input >> entity >> tag)
            {
                engine.RemoveTag(entity.c_str(), tag.c_str());
                return true;
            }
        }
        else if (command == "transient")
        {
            std::string entity;
            std::string tag;
            if (input >> entity >> tag)
            {
                engine.SetTransientTag(entity.c_str(), tag.c_str());
                return true;
            }
        }
        else if (command == "float")
        {
            std::string entity;
            std::string key;
            float value = 0.0f;
            if (input >> entity >> key >> value)
            {
                engine.SetValue(entity.c_str(), key.c_str(), value);
                return true;
            }
        }

        else if (command == "pos")
        {
            std::string entity;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            if (input >> entity >> x >> y >> z)
            {
                engine.SetPosition(entity.c_str(), x, y, z);
                return true;
            }
        }
        // else if (command == "quat")
        // {
        //     std::string entity;
        //     float a = 0.0f;
        //     float b = 0.0f;
        //     float c = 0.0f;
        //     float d = 1.0f;
        //     if (input >> entity >> a >> b >> c >> d)
        //     {
        //         AudioManager_SetQuatValue(entity.c_str(), "rotation", a, b, c, d);
        //         return true;
        //     }
        // }
        // else if (command == "transform")
        // {
        //     std::string entity;
        //     float x = 0.0f;
        //     float y = 0.0f;
        //     float z = 0.0f;
        //     float a = 0.0f;
        //     float b = 0.0f;
        //     float c = 0.0f;
        //     float d = 1.0f;
        //     if (input >> entity >> x >> y >> z >> a >> b >> c >> d)
        //     {
        //         AudioManager_SetTransform(entity.c_str(), x, y, z, a, b, c, d);
        //         return true;
        //     }
        // }

        std::cout << "Invalid command. Type `help` for supported commands.\n";
        return true;
    }
    bool RunInteractiveTest(decl_audio::Engine &engine)
    {
        const fs::path bank_path = GetSpatializationBankPath();

        // that's kind of stupid and we should not do that! xD really we need to reconsider how we load banks in the test-thing at all
        if (!engine.LoadBehaviors(bank_path.string().c_str()))
        {
            std::cerr << decl_audio::compiler::DumpDiagnostics(engine.GetLoadDiagnostics());
            return false;
        }

        PrintPromptHelp();

        std::string line;
        while (std::cout << "> " && std::getline(std::cin, line))
        {
            if (!ProcessCommand(engine, line))
            {
                return 0;
            }
            engine.Update(); // huh?
        }
        return 0;
    }

} // namespace

int main(int argc, char **argv)
{

    CliOptions options;
    options.bankPath = GetDefaultBankPath();
    bool shouldExit = false;
    if (!ParseArgs(argc, argv, options, shouldExit))
    {
        return shouldExit ? 0 : 1;
    }

    EngineConfig config = GetDefaultConfig();

    decl_audio::Engine engine(config);
    std::cout << "Loading bank: " << options.bankPath << '\n';
    if (!engine.LoadBehaviors(options.bankPath.string().c_str()))
    {
        std::cerr << decl_audio::compiler::DumpDiagnostics(engine.GetLoadDiagnostics());
        return 1;
    }

    const decl_audio::compiler::CompileResult compile_result = decl_audio::compiler::LoadCompiledBankFromJsonFile(options.bankPath);
    if (compile_result.HasErrors())
    {
        std::cerr << decl_audio::compiler::DumpDiagnostics(compile_result.diagnostics);
        return 1;
    }

    if (options.autoTests)
    {
        if (!RunOneShotTest(engine, compile_result.bank))
            return 1;

        if (!RunLoopTest(engine, compile_result.bank))
            return 1;

        if (!RunRandomTest(engine, compile_result.bank))
            return 1;

        if (!RunParamForwardingTest(engine))
            return 1;

        if (!RunPanningTest(engine))
            return 1;

        if (!RunTransientTagTest(engine))
            return 1;
    }

    if (!RunInteractiveTest(engine))
        return 1;

    return 0;
}

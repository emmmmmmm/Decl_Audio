#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "../../src/compiler/Compiler.hpp"
#include "../../src/core/Engine.hpp"
#include "../../src/core/DebugUtils.hpp"

namespace fs = std::filesystem;

namespace
{
    struct CliOptions
    {
        fs::path bankPath = fs::current_path();
        bool autoTests = false;
    };

    void PrintUsage(const char *exe_name)
    {
        std::cout
            << "Usage: " << exe_name << " [options]\n"
            << "  --bank <path>          Behavior bank JSON path\n"
            << "  --banks <path>         Alias for --bank\n"
            << "  --automatedTesting     Run the built-in sandbox routine before the prompt\n"
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
            << "  pos <entity> <x> <y> <z>\n"
            << "  listener <x> <y> <z>\n"
            << "  destroy <entity>\n"
            << "  dump\n"
            << "  exit\n";
    }

    bool ParseArgs(int argc, char **argv, CliOptions &options, bool &should_exit)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];

            if (arg == "--help" || arg == "-h")
            {
                PrintUsage(argv[0]);
                should_exit = true;
                return false;
            }

            if (arg == "--automatedTesting")
            {
                options.autoTests = true;
                continue;
            }

            if (arg == "--bank" || arg == "--banks")
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "Missing value for " << arg << '\n';
                    return false;
                }

                options.bankPath = argv[++i];
                continue;
            }

            std::cerr << "Unknown option: " << arg << '\n';
            return false;
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
        return fs::path(__FILE__).parent_path().parent_path().parent_path() / "tests" / "data" / "SandboxBehaviorBank.json";
    }

    void PrintSection(const char *label, const char *description)
    {
        std::cout << '\n'
                  << label << '\n'
                  << description << '\n';
    }

    bool ClearEntity(decl_audio::Engine &engine, const char *entity_id, const std::uint32_t wait_ms = 100)
    {
        engine.DestroyEntity(entity_id);
        engine.Update();
        return RunWait(engine, wait_ms);
    }

    bool RunOneShotTest(decl_audio::Engine &engine)
    {
        constexpr const char *kEntityId = "sandbox.oneshot";

        PrintSection("OneShot", "You should hear one short hit, then silence.");
        engine.SetTransientTag(kEntityId, "sandbox.playback.oneshot");
        engine.Update();
        if (!RunWait(engine, 1000))
        {
            return false;
        }

        return ClearEntity(engine, kEntityId);
    }

    bool RunLoopTest(decl_audio::Engine &engine)
    {
        constexpr const char *kEntityId = "sandbox.loop";

        PrintSection("Loop", "You should hear a steady loop for two seconds, then it should stop cleanly.");
        engine.SetTag(kEntityId, "sandbox.playback.loop");
        engine.Update();
        if (!RunWait(engine, 2000))
        {
            return false;
        }

        engine.RemoveTag(kEntityId, "sandbox.playback.loop");
        engine.Update();
        if (!RunWait(engine, 1000))
        {
            return false;
        }

        return ClearEntity(engine, kEntityId);
    }

    bool RunRandomTest(decl_audio::Engine &engine)
    {
        constexpr const char *kEntityId = "sandbox.random";

        PrintSection("Random", "You should hear three short hits. They may alternate between the two source assets.");
        engine.SetTransientTag(kEntityId, "sandbox.playback.random");
        engine.Update();
        if (!RunWait(engine, 700))
        {
            return false;
        }

        engine.SetTransientTag(kEntityId, "sandbox.playback.random");
        engine.Update();
        if (!RunWait(engine, 700))
        {
            return false;
        }

        engine.SetTransientTag(kEntityId, "sandbox.playback.random");
        engine.Update();
        if (!RunWait(engine, 700))
        {
            return false;
        }

        return ClearEntity(engine, kEntityId);
    }

    bool RunParamForwardingTest(decl_audio::Engine &engine)
    {
        constexpr const char *kEntityId = "sandbox.param";

        PrintSection("Param Forwarding", "You should hear a quiet loop first, then a louder loop after the runtime volume parameter changes.");
        engine.SetValue(kEntityId, "volume", 0.2f);
        engine.SetTag(kEntityId, "resolver.active");
        engine.Update();
        if (!RunWait(engine, 1000))
        {
            return false;
        }

        engine.SetValue(kEntityId, "volume", 1.0f);
        engine.Update();
        if (!RunWait(engine, 1000))
        {
            return false;
        }

        engine.RemoveTag(kEntityId, "resolver.active");
        engine.Update();
        if (!RunWait(engine, 700))
        {
            return false;
        }

        return ClearEntity(engine, kEntityId);
    }

    bool RunPanningTest(decl_audio::Engine &engine)
    {
        constexpr const char *kEntityId = "sandbox.pan";

        PrintSection("Panning Test", "You should hear a sound on your right.");
        engine.SetListenerPosition(-1.0f, 0.0f, 0.0f);
        engine.SetPosition(kEntityId, 3.0f, 0.0f, 0.0f);
        engine.SetTag(kEntityId, "spatial.mono");
        engine.Update();
        if (!RunWait(engine, 500))
        {
            return false;
        }

        engine.SetPosition(kEntityId, 1.0f, 0.0f, 0.0f);
        engine.Update();
        if (!RunWait(engine, 500))
        {
            return false;
        }

        PrintSection("Panning Test", "You should hear a sound on your left.");
        engine.SetListenerPosition(1.0f, 0.0f, 0.0f);
        engine.SetPosition(kEntityId, -1.0f, 0.0f, 0.0f);
        engine.Update();
        if (!RunWait(engine, 500))
        {
            return false;
        }

        engine.SetPosition(kEntityId, -3.0f, 0.0f, 0.0f);
        engine.Update();
        if (!RunWait(engine, 500))
        {
            return false;
        }

        engine.RemoveTag(kEntityId, "spatial.mono");
        engine.SetListenerPosition(0.0f, 0.0f, 0.0f);
        engine.Update();
        if (!RunWait(engine, 700))
        {
            return false;
        }

        return ClearEntity(engine, kEntityId);
    }

    bool RunTransientTagTest(decl_audio::Engine &engine)
    {
        constexpr const char *kEntityId = "sandbox.transient";

        PrintSection("TransientTag Test", "You should hear a short sound triggered by a transient tag.");
        engine.SetListenerPosition(0.0f, 0.0f, 0.0f);
        engine.SetPosition(kEntityId, 0.0f, 0.0f, 0.0f);
        engine.SetTransientTag(kEntityId, "spatial.mono");
        engine.Update();
        if (!RunWait(engine, 700))
        {
            return false;
        }

        return ClearEntity(engine, kEntityId);
    }

    bool RunMassEventsTest(decl_audio::Engine &engine, int numEvents = 100)
    {
        PrintSection("MassEvents Test", "You should hear a cluster of short sounds triggered by a transient tag.");

        engine.SetListenerPosition(0.0f, 0.0f, 0.0f);

        for (int i = 0; i < numEvents; i++)
        {
            auto name = "test_" + std::to_string(i);
            engine.SetTransientTag(name.c_str(), "spatial.mono");
            engine.SetPosition(name.c_str(), (float)i, 0.0f, 0.0f);
        }

        engine.Update();
        if (!RunWait(engine, 700))
        {
            return false;
        }
        for (int i = 0; i < numEvents; i++)
        {
            auto name = "test_" + std::to_string(i);
            engine.DestroyEntity(name.c_str());
        }
        engine.Update();

        return RunWait(engine, 100);
    }
    bool SeriesProgramTest(decl_audio::Engine &engine)
    {
        constexpr const char *kEntityId = "sandbox.series";

        PrintSection("Series program Test", "You should hear multiple oneshots triggered by the same program");

        engine.SetListenerPosition(0.0f, 0.0f, 0.0f);
        engine.SetPosition(kEntityId, 0.0f, 0.0f, 0.0f);
        engine.SetTransientTag(kEntityId, "test.series");
        engine.Update();
        if (!RunWait(engine, 2700))
        {
            return false;
        }

        return ClearEntity(engine, kEntityId);
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
            decl_audio::DebugUtils::PrintSnapshot(&engine);
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
        else if (command == "listener")
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            if (input >> x >> y >> z)
            {
                engine.SetListenerPosition(x, y, z);
                return true;
            }
        }
        else if (command == "destroy")
        {
            std::string entity;
            if (input >> entity)
            {
                engine.DestroyEntity(entity.c_str());
                return true;
            }
        }

        std::cout << "Invalid command. Type `help` for supported commands.\n";
        return true;
    }

    bool RunInteractiveTest(decl_audio::Engine &engine)
    {
        PrintPromptHelp();

        std::string line;
        while (std::cout << "> " && std::getline(std::cin, line))
        {
            if (!ProcessCommand(engine, line))
            {
                return true;
            }

            engine.Update();
        }

        return true;
    }
} // namespace

int main(int argc, char **argv)
{
    CliOptions options;
    options.bankPath = GetDefaultBankPath();

    bool should_exit = false;
    if (!ParseArgs(argc, argv, options, should_exit))
    {
        return should_exit ? 0 : 1;
    }

    EngineConfig config = GetDefaultConfig();
    decl_audio::Engine engine(config);

    std::cout << "Loading bank: " << options.bankPath << '\n';
    if (!engine.LoadBehaviors(options.bankPath.string().c_str()))
    {
        std::cerr << decl_audio::DumpDiagnostics(engine.GetLoadDiagnostics());
        return 1;
    }

    if (options.autoTests || true)
    {
        if (!RunOneShotTest(engine))
            return 1;
        if (!RunLoopTest(engine))
            return 1;
        if (!RunRandomTest(engine))
            return 1;
        if (!RunParamForwardingTest(engine))
            return 1;
        if (!RunPanningTest(engine))
            return 1;
        if (!RunTransientTagTest(engine))
            return 1;
        if (!RunMassEventsTest(engine, 100))
            return 1;
        if (!SeriesProgramTest(engine))
            return 1;
    }

    if (!RunInteractiveTest(engine))
        return 1;

    return 0;
}

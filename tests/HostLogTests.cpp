#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "../include/Decl_Audio/Decl_Audio.h"

namespace
{
    bool Expect(bool condition, const char *message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            return false;
        }

        return true;
    }

    std::filesystem::path GetFixturePath(const char *file_name)
    {
        return std::filesystem::path(__FILE__).parent_path() / "data" / file_name;
    }

    EngineConfig GetTestConfig()
    {
        EngineConfig config = GetDefaultConfig();
        config.backend = DECL_AUDIO_BACKEND_SILENT;
        return config;
    }

    bool TestLoadFailuresReachHostLogQueue()
    {
        const std::filesystem::path fixture_path = GetFixturePath("InvalidBehaviorBank.json");

        EngineConfig config = GetTestConfig();
        DeclAudioEngine *engine = nullptr;
        if (!Expect(CreateEngine(&config, &engine), "CreateEngine should succeed for host log tests"))
            return false;

        if (!Expect(!LoadBehaviors(engine, fixture_path.string().c_str()), "invalid fixture should fail to load"))
        {
            DestroyEngine(engine);
            return false;
        }

        std::vector<DeclAudioLogMessage> messages;
        for (;;)
        {
            DeclAudioLogMessage message{};
            if (!TryDequeueLog(engine, &message))
                break;
            messages.push_back(message);
        }

        DeclAudioLogMessage drained{};
        const bool queue_is_empty = !TryDequeueLog(engine, &drained);
        DestroyEngine(engine);

        if (!Expect(messages.size() >= 3, "invalid fixture should emit multiple host log messages"))
            return false;
        if (!Expect(std::string(messages[0].message).find("oneshot nodes require exactly one asset") != std::string::npos, "first log should preserve the first diagnostic"))
            return false;
        if (!Expect(std::string(messages[1].message).find("duplicate behavior id 'movement.invalid'") != std::string::npos, "second log should preserve FIFO ordering"))
            return false;
        if (!Expect(std::string(messages[2].message).find("random nodes require at least one asset") != std::string::npos, "third log should preserve FIFO ordering"))
            return false;
        return Expect(queue_is_empty, "drained host log queue should report empty");
    }
} // namespace

bool RunHostLogTests()
{
    if (!TestLoadFailuresReachHostLogQueue())
        return false;

    std::cout << "HostLog tests passed\n";
    return true;
}

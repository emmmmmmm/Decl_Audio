#include <iostream>
#include <string_view>

bool RunCompilerTests();
bool RunAssetBankTests();
bool RunPlaybackTests();
bool RunRingBufferTests();
bool RunWorldStateTests();
bool RunHostLogTests();
bool RunBankSerializerTests();
int RunAudioCapacityOverflowDeathTestChild(const char *started_flag_path);

namespace
{
    const char *g_test_executable_path = nullptr;
}

const char *GetTestExecutablePath()
{
    return g_test_executable_path;
}

int main(int argc, char **argv)
{
    g_test_executable_path = argv[0];

    if (argc == 3 && std::string_view(argv[1]) == "--death-test-audio-capacity-overflow")
    {
        return RunAudioCapacityOverflowDeathTestChild(argv[2]);
    }

    if (!RunRingBufferTests())
        return 1;

    if (!RunCompilerTests())
        return 1;

    if (!RunAssetBankTests())
        return 1;

    if (!RunPlaybackTests())
        return 1;

    if (!RunWorldStateTests())
        return 1;

    if (!RunHostLogTests())
        return 1;

    if (!RunBankSerializerTests())
        return 1;

    std::cout << "All tests passed\n";
    return 0;
}

#include <iostream>

bool RunCompilerTests();
bool RunAssetBankTests();
bool RunPlaybackTests();
bool RunRingBufferTests();
bool RunWorldStateTests();

int main()
{
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

    std::cout << "All tests passed\n";
    return 0;
}

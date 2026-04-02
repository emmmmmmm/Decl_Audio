#include <iostream>

bool RunCompilerTests();
bool RunRingBufferTests();

int main()
{
    if (!RunRingBufferTests())
        return 1;

    if (!RunCompilerTests())
        return 1;

    std::cout << "All tests passed\n";
    return 0;
}

#pragma once

#include <cstdint>

namespace decl_audio
{
    // Identifies a loaded bank. `slot` indexes the audio thread's fixed bank
    // table; `generation` is bumped on slot reuse so a stale id from a logic bug
    // mismatches instead of silently addressing a different bank (used once
    // unloading lands - see docs/BankLoadingRefactor.md section 3.4).
    //
    // Ids cross thread boundaries (commands carry a BankId); pointers are resolved
    // locally off the slot. This mirrors how an InstanceId resolves to a
    // ProgramInstance and a program_id resolves to a CompiledProgram.
    struct BankId final
    {
        std::uint32_t slot = 0;
        std::uint32_t generation = 0;

        friend bool operator==(const BankId &, const BankId &) noexcept = default;
    };
} // namespace decl_audio

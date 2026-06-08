#pragma once

#include <cstddef>
#include <cstdint>

namespace decl_audio
{
    // Maximum number of banks that can be loaded (or draining) at once. The audio
    // thread's bank table and the control-thread registry are both fixed arrays of
    // this size, indexed by BankId.slot. A retiring/draining bank still occupies its
    // slot until fully drained, so AddBank may legitimately fail "no free slot".
    inline constexpr std::size_t kMaxBanks = 16;

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

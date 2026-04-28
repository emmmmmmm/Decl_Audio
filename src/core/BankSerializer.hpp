#pragma once

#include "../assets/AssetBank.hpp"
#include "../compiler/CompiledBank.hpp"
#include "Diagnostics.hpp"

#include <cstdint>
#include <vector>

namespace decl_audio::serialization
{
    inline constexpr std::uint32_t kBankMagic   = 0xDEC1A0D1u;
    inline constexpr std::uint32_t kBankVersion = 1u;

    struct LoadBankResult final
    {
        compiler::CompiledBank compiled_bank;
        assets::AssetBank      asset_bank;
        std::vector<Diagnostic> diagnostics;

        [[nodiscard]] bool HasErrors() const noexcept
        {
            return decl_audio::HasErrors(diagnostics);
        }
    };

    // Deserialize a .dacbank binary file into a CompiledBank + AssetBank.
    // Returns a result with at least one Error diagnostic on any failure.
    [[nodiscard]] LoadBankResult LoadBankFromFile(const char *bank_path);

    // Serialize a compiled bank and its audio data to a .dacbank binary file.
    // Returns false and appends an Error diagnostic on failure.
    [[nodiscard]] bool WriteBankToFile(
        const char *output_path,
        const compiler::CompiledBank &compiled_bank,
        const assets::AssetBank &asset_bank,
        std::vector<Diagnostic> &out_diagnostics);

} // namespace decl_audio::serialization

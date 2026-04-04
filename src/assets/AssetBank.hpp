#pragma once

#include "../compiler/CompiledBank.hpp"
#include "../compiler/Compiler.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace decl_audio::assets
{
    inline constexpr std::uint32_t kRequiredSampleRate = 48000; // todo

    struct DecodedBuffer final
    {
        std::vector<float> samples;
        std::uint64_t frame_count = 0;
        std::uint32_t channel_count = 0;
        std::uint32_t sample_rate = 0;

        [[nodiscard]] std::size_t SampleCount() const noexcept
        {
            return samples.size();
        }
    };

    struct AssetBank final
    {
        std::vector<DecodedBuffer> buffers;
        std::vector<std::filesystem::path> source_paths;

        [[nodiscard]] const DecodedBuffer &GetBuffer(compiler::AssetId id) const
        {
            return buffers.at(static_cast<std::size_t>(id));
        }

        [[nodiscard]] const std::filesystem::path &GetSourcePath(compiler::AssetId id) const
        {
            return source_paths.at(static_cast<std::size_t>(id));
        }
    };

    struct LoadResult final
    {
        AssetBank bank;
        std::vector<compiler::Diagnostic> diagnostics;

        [[nodiscard]] bool HasErrors() const noexcept
        {
            for (const compiler::Diagnostic &diagnostic : diagnostics)
            {
                if (diagnostic.severity == compiler::DiagnosticSeverity::Error)
                    return true;
            }

            return false;
        }
    };

    [[nodiscard]] LoadResult LoadAssetBank(const compiler::CompiledBank &compiled_bank, const std::filesystem::path &source_path);
    [[nodiscard]] std::string DumpAssetBank(const compiler::CompiledBank &compiled_bank, const AssetBank &asset_bank);
} // namespace decl_audio::assets

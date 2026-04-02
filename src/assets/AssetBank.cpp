#include "pch.h"

#define MA_NO_DEVICE_IO
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MINIAUDIO_IMPLEMENTATION
#include "../ThirdParty/miniaudio/miniaudio.h"

#include "AssetBank.hpp"

#include <sstream>

namespace decl_audio::assets
{
    namespace
    {
        [[nodiscard]] compiler::Diagnostic MakeError(const std::filesystem::path &source_path,
                                                     std::string_view object_path,
                                                     std::string message)
        {
            compiler::Diagnostic diagnostic;
            diagnostic.severity = compiler::DiagnosticSeverity::Error;
            diagnostic.location.file_path = source_path.string();
            diagnostic.location.object_path = std::string(object_path);
            diagnostic.message = std::move(message);
            return diagnostic;
        }

        [[nodiscard]] std::filesystem::path ResolveAssetPath(const std::filesystem::path &source_path, std::string_view asset_path)
        {
            return (source_path.parent_path() / std::filesystem::path(asset_path)).lexically_normal();
        }

        [[nodiscard]] std::string FormatDecoderError(ma_result result)
        {
            std::ostringstream stream;
            stream << "decoder failed with result " << static_cast<int>(result);
            return stream.str();
        }

        [[nodiscard]] ma_result InitDecoderForFile(const std::filesystem::path &asset_path, ma_decoder &decoder)
        {
            ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);

#ifdef _WIN32
            const std::wstring wide_path = asset_path.wstring();
            return ma_decoder_init_file_w(wide_path.c_str(), &config, &decoder);
#else
            const std::string narrow_path = asset_path.string();
            return ma_decoder_init_file(narrow_path.c_str(), &config, &decoder);
#endif
        }
    } // namespace

    LoadResult LoadAssetBank(const compiler::CompiledBank &compiled_bank, const std::filesystem::path &source_path)
    {
        LoadResult result;
        result.bank.buffers.reserve(compiled_bank.asset_paths.size());
        result.bank.source_paths.reserve(compiled_bank.asset_paths.size());

        for (std::size_t asset_index = 0; asset_index < compiled_bank.asset_paths.size(); ++asset_index)
        {
            const std::string &asset_name = compiled_bank.asset_paths[asset_index];
            const std::filesystem::path resolved_path = ResolveAssetPath(source_path, asset_name);
            const std::string object_path = "asset[" + std::to_string(asset_index) + "] " + asset_name;

            if (!std::filesystem::exists(resolved_path))
            {
                result.diagnostics.push_back(MakeError(source_path, object_path, "referenced asset does not exist: " + resolved_path.string()));
                continue;
            }

            ma_decoder decoder{};
            const ma_result init_result = InitDecoderForFile(resolved_path, decoder);
            if (init_result != MA_SUCCESS)
            {
                result.diagnostics.push_back(MakeError(source_path, object_path, "failed to initialize decoder for " + resolved_path.string() + "; " + FormatDecoderError(init_result)));
                continue;
            }

            ma_format format = ma_format_unknown;
            ma_uint32 channels = 0;
            ma_uint32 sample_rate = 0;
            const ma_result format_result = ma_decoder_get_data_format(&decoder, &format, &channels, &sample_rate, nullptr, 0);
            if (format_result != MA_SUCCESS)
            {
                ma_decoder_uninit(&decoder);
                result.diagnostics.push_back(MakeError(source_path, object_path, "failed to query decoder format for " + resolved_path.string() + "; " + FormatDecoderError(format_result)));
                continue;
            }

            if (format != ma_format_f32)
            {
                ma_decoder_uninit(&decoder);
                result.diagnostics.push_back(MakeError(source_path, object_path, "decoder did not produce float samples"));
                continue;
            }

            if (channels == 0 || channels > 2)
            {
                ma_decoder_uninit(&decoder);
                result.diagnostics.push_back(MakeError(source_path, object_path, "unsupported channel count " + std::to_string(channels) + "; MVP supports mono or stereo assets only"));
                continue;
            }

            if (sample_rate != kRequiredSampleRate)
            {
                ma_decoder_uninit(&decoder);
                result.diagnostics.push_back(MakeError(source_path, object_path, "unsupported sample rate " + std::to_string(sample_rate) + " Hz; expected " + std::to_string(kRequiredSampleRate) + " Hz"));
                continue;
            }

            ma_uint64 frame_count = 0;
            const ma_result length_result = ma_decoder_get_length_in_pcm_frames(&decoder, &frame_count);
            if (length_result != MA_SUCCESS)
            {
                ma_decoder_uninit(&decoder);
                result.diagnostics.push_back(MakeError(source_path, object_path, "failed to query decoded frame count for " + resolved_path.string() + "; " + FormatDecoderError(length_result)));
                continue;
            }

            DecodedBuffer decoded_buffer;
            decoded_buffer.frame_count = frame_count;
            decoded_buffer.channel_count = channels;
            decoded_buffer.sample_rate = sample_rate;
            decoded_buffer.samples.resize(static_cast<std::size_t>(frame_count * channels));

            ma_uint64 total_frames_read = 0;
            while (total_frames_read < frame_count)
            {
                ma_uint64 frames_read = 0;
                const ma_result read_result = ma_decoder_read_pcm_frames(
                    &decoder,
                    decoded_buffer.samples.data() + static_cast<std::size_t>(total_frames_read * channels),
                    frame_count - total_frames_read,
                    &frames_read);

                if (read_result != MA_SUCCESS)
                {
                    ma_decoder_uninit(&decoder);
                    result.diagnostics.push_back(MakeError(source_path, object_path, "failed while decoding " + resolved_path.string() + "; " + FormatDecoderError(read_result)));
                    decoded_buffer.samples.clear();
                    total_frames_read = 0;
                    break;
                }

                if (frames_read == 0)
                    break;

                total_frames_read += frames_read;
            }

            ma_decoder_uninit(&decoder);

            if (decoded_buffer.samples.empty() && frame_count != 0)
                continue;

            if (total_frames_read != frame_count)
            {
                result.diagnostics.push_back(MakeError(source_path, object_path, "decoder stopped early for " + resolved_path.string() + "; expected " + std::to_string(frame_count) + " frames, got " + std::to_string(total_frames_read)));
                continue;
            }

            result.bank.buffers.push_back(std::move(decoded_buffer));
            result.bank.source_paths.push_back(resolved_path);
        }

        return result;
    }

    std::string DumpAssetBank(const compiler::CompiledBank &compiled_bank, const AssetBank &asset_bank)
    {
        std::ostringstream stream;
        stream << "AssetBank\n";
        stream << "  buffers: " << asset_bank.buffers.size() << '\n';

        for (std::size_t asset_index = 0; asset_index < asset_bank.buffers.size(); ++asset_index)
        {
            const DecodedBuffer &buffer = asset_bank.buffers[asset_index];
            stream << "Asset[" << asset_index << "] "
                   << compiled_bank.GetAssetPath(static_cast<compiler::AssetId>(asset_index))
                   << '\n';
            stream << "  source: " << asset_bank.source_paths[asset_index].string() << '\n';
            stream << "  frames: " << buffer.frame_count << '\n';
            stream << "  channels: " << buffer.channel_count << '\n';
            stream << "  sampleRate: " << buffer.sample_rate << '\n';
            stream << "  samples: " << buffer.samples.size() << '\n';
        }

        return stream.str();
    }
} // namespace decl_audio::assets

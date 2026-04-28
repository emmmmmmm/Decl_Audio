// No pch.h — this file is shared between the DLL and the Validator (no PCH).

#include "BankSerializer.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

using decl_audio::compiler::CompiledBehavior;
using decl_audio::compiler::CompiledProgram;
using decl_audio::compiler::CompiledSpatializationSettings;
using decl_audio::compiler::CompiledNode;
using decl_audio::compiler::CompiledCondition;

static_assert(sizeof(CompiledBehavior) == 28,
    "CompiledBehavior layout changed — update BankSerializer version");
static_assert(sizeof(CompiledSpatializationSettings) == 16,
    "CompiledSpatializationSettings layout changed — update BankSerializer version");
static_assert(sizeof(CompiledProgram) == 60,
    "CompiledProgram layout changed — update BankSerializer version");
static_assert(sizeof(CompiledNode) == 36,
    "CompiledNode layout changed — update BankSerializer version");
static_assert(sizeof(CompiledCondition) == 12,
    "CompiledCondition layout changed — update BankSerializer version");

namespace
{
    class BinaryWriter
    {
    public:
        template<typename T>
        void Write(T value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            const auto *bytes = reinterpret_cast<const std::uint8_t *>(&value);
            buffer_.insert(buffer_.end(), bytes, bytes + sizeof(T));
        }

        void WriteBytes(const void *data, std::size_t count)
        {
            const auto *bytes = static_cast<const std::uint8_t *>(data);
            buffer_.insert(buffer_.end(), bytes, bytes + count);
        }

        template<typename T>
        void WritePodVector(const std::vector<T> &vec)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            Write(static_cast<std::uint32_t>(vec.size()));
            if (!vec.empty())
                WriteBytes(vec.data(), vec.size() * sizeof(T));
        }

        template<typename TId>
        void WriteStringMap(const std::unordered_map<std::string, TId> &map)
        {
            static_assert(sizeof(TId) == sizeof(std::uint32_t));
            Write(static_cast<std::uint32_t>(map.size()));
            for (const auto &[name, id] : map)
            {
                Write(static_cast<std::uint32_t>(name.size()));
                WriteBytes(name.data(), name.size());
                Write(static_cast<std::uint32_t>(id));
            }
        }

        [[nodiscard]] bool FlushToFile(const char *path, std::vector<decl_audio::Diagnostic> &diags) const
        {
            std::FILE *f = nullptr;
#if defined(_MSC_VER)
            if (fopen_s(&f, path, "wb") != 0)
                f = nullptr;
#else
            f = std::fopen(path, "wb");
#endif
            if (!f)
            {
                diags.push_back(decl_audio::MakeError(path, "failed to open output file for writing"));
                return false;
            }
            const bool ok = buffer_.empty() ||
                std::fwrite(buffer_.data(), 1, buffer_.size(), f) == buffer_.size();
            std::fclose(f);
            if (!ok)
            {
                diags.push_back(decl_audio::MakeError(path, "failed to write bank file"));
                return false;
            }
            return true;
        }

    private:
        std::vector<std::uint8_t> buffer_;
    };

    class BinaryReader
    {
    public:
        explicit BinaryReader(std::vector<std::uint8_t> data)
            : data_(std::move(data)) {}

        template<typename T>
        [[nodiscard]] bool Read(T &out, std::string &err)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            if (cursor_ + sizeof(T) > data_.size())
            {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "unexpected end of file (offset=%zu, need=%zu, size=%zu)",
                    cursor_, sizeof(T), data_.size());
                err = buf;
                return false;
            }
            std::memcpy(&out, data_.data() + cursor_, sizeof(T));
            cursor_ += sizeof(T);
            return true;
        }

        template<typename T>
        [[nodiscard]] bool ReadPodVector(std::vector<T> &out, std::string &err)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            std::uint32_t count = 0;
            if (!Read(count, err))
                return false;
            const std::size_t byte_count = static_cast<std::size_t>(count) * sizeof(T);
            if (cursor_ + byte_count > data_.size())
            {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "vector data overruns file (count=%u, bytes=%zu, offset=%zu)",
                    count, byte_count, cursor_);
                err = buf;
                return false;
            }
            out.resize(count);
            if (count > 0)
                std::memcpy(out.data(), data_.data() + cursor_, byte_count);
            cursor_ += byte_count;
            return true;
        }

        template<typename TId>
        [[nodiscard]] bool ReadStringMap(std::unordered_map<std::string, TId> &map, std::string &err)
        {
            std::uint32_t count = 0;
            if (!Read(count, err))
                return false;
            map.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                std::uint32_t name_len = 0;
                if (!Read(name_len, err))
                    return false;
                if (cursor_ + name_len > data_.size())
                {
                    char buf[128];
                    std::snprintf(buf, sizeof(buf),
                        "string name overruns file (len=%u, offset=%zu)",
                        name_len, cursor_);
                    err = buf;
                    return false;
                }
                std::string name(reinterpret_cast<const char *>(data_.data() + cursor_), name_len);
                cursor_ += name_len;
                std::uint32_t id = 0;
                if (!Read(id, err))
                    return false;
                map.emplace(std::move(name), static_cast<TId>(id));
            }
            return true;
        }

        [[nodiscard]] bool ReadBytesInto(void *dst, std::size_t count, std::string &err)
        {
            if (cursor_ + count > data_.size())
            {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "data overruns file (bytes=%zu, offset=%zu, size=%zu)",
                    count, cursor_, data_.size());
                err = buf;
                return false;
            }
            std::memcpy(dst, data_.data() + cursor_, count);
            cursor_ += count;
            return true;
        }

    private:
        std::vector<std::uint8_t> data_;
        std::size_t cursor_ = 0;
    };

    std::string HexStr(std::uint32_t v)
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "0x%08X", v);
        return buf;
    }

} // anonymous namespace

namespace decl_audio::serialization
{
    bool WriteBankToFile(
        const char *output_path,
        const compiler::CompiledBank &bank,
        const assets::AssetBank &asset_bank,
        std::vector<Diagnostic> &out_diagnostics)
    {
        BinaryWriter w;

        w.Write(kBankMagic);
        w.Write(kBankVersion);

        w.Write(bank.max_program_node_count);
        w.Write(bank.max_program_parameter_slot_count);
        w.Write(bank.max_program_concurrent_voices);

        w.WritePodVector(bank.behaviors);
        w.WritePodVector(bank.programs);
        w.WritePodVector(bank.nodes);
        w.WritePodVector(bank.behavior_tags);
        w.WritePodVector(bank.conditions);
        w.WritePodVector(bank.node_children);
        w.WritePodVector(bank.node_assets);
        w.WritePodVector(bank.program_parameters);
        w.WritePodVector(bank.tag_depths);
        w.WritePodVector(bank.tag_group_head);

        w.WriteStringMap(bank.behavior_name_to_id);
        w.WriteStringMap(bank.program_name_to_id);
        w.WriteStringMap(bank.tag_name_to_id);
        w.WriteStringMap(bank.parameter_name_to_id);
        w.WriteStringMap(bank.asset_name_to_id);

        w.Write(static_cast<std::uint32_t>(asset_bank.buffers.size()));
        for (const assets::DecodedBuffer &buf : asset_bank.buffers)
        {
            w.Write(buf.frame_count);
            w.Write(buf.channel_count);
            w.Write(buf.sample_rate);
            w.Write(static_cast<std::uint32_t>(buf.samples.size()));
            if (!buf.samples.empty())
                w.WriteBytes(buf.samples.data(), buf.samples.size() * sizeof(float));
        }

        return w.FlushToFile(output_path, out_diagnostics);
    }

    LoadBankResult LoadBankFromFile(const char *bank_path)
    {
        LoadBankResult result;

        std::ifstream f(bank_path, std::ios::binary);
        if (!f.is_open())
        {
            result.diagnostics.push_back(MakeError(bank_path, "failed to open bank file"));
            return result;
        }
        std::vector<std::uint8_t> data(
            (std::istreambuf_iterator<char>(f)),
            std::istreambuf_iterator<char>());
        f.close();

        BinaryReader r(std::move(data));
        std::string err;

        std::uint32_t magic = 0;
        if (!r.Read(magic, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (magic != kBankMagic)
        {
            result.diagnostics.push_back(MakeError(bank_path,
                "invalid bank file magic (expected " + HexStr(kBankMagic) + ", got " + HexStr(magic) + ")"));
            return result;
        }

        std::uint32_t version = 0;
        if (!r.Read(version, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (version != kBankVersion)
        {
            result.diagnostics.push_back(MakeError(bank_path,
                "unsupported bank version " + std::to_string(version) +
                " (expected " + std::to_string(kBankVersion) + ")"));
            return result;
        }

        auto &bank  = result.compiled_bank;
        auto &abank = result.asset_bank;

        if (!r.Read(bank.max_program_node_count, err) ||
            !r.Read(bank.max_program_parameter_slot_count, err) ||
            !r.Read(bank.max_program_concurrent_voices, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }

        if (!r.ReadPodVector(bank.behaviors, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadPodVector(bank.programs, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadPodVector(bank.nodes, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadPodVector(bank.behavior_tags, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadPodVector(bank.conditions, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadPodVector(bank.node_children, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadPodVector(bank.node_assets, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadPodVector(bank.program_parameters, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadPodVector(bank.tag_depths, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadPodVector(bank.tag_group_head, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }

        if (!r.ReadStringMap(bank.behavior_name_to_id, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadStringMap(bank.program_name_to_id, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadStringMap(bank.tag_name_to_id, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadStringMap(bank.parameter_name_to_id, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        if (!r.ReadStringMap(bank.asset_name_to_id, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }

        std::uint32_t buffer_count = 0;
        if (!r.Read(buffer_count, err))
        {
            result.diagnostics.push_back(MakeError(bank_path, err));
            return result;
        }
        abank.buffers.resize(buffer_count);
        for (assets::DecodedBuffer &buf : abank.buffers)
        {
            std::uint32_t sample_count = 0;
            if (!r.Read(buf.frame_count, err) ||
                !r.Read(buf.channel_count, err) ||
                !r.Read(buf.sample_rate, err) ||
                !r.Read(sample_count, err))
            {
                result.diagnostics.push_back(MakeError(bank_path, err));
                return result;
            }
            buf.samples.resize(sample_count);
            if (sample_count > 0)
            {
                if (!r.ReadBytesInto(buf.samples.data(), sample_count * sizeof(float), err))
                {
                    result.diagnostics.push_back(MakeError(bank_path, err));
                    return result;
                }
            }
        }

        return result;
    }

} // namespace decl_audio::serialization

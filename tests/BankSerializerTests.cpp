#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <thread>

#include "../include/Decl_Audio/Decl_Audio.h"
#include "../src/assets/AssetBank.hpp"
#include "../src/compiler/Compiler.hpp"
#include "../src/core/BankSerializer.hpp"
#include "../src/core/Engine.hpp"

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

    bool TestRoundTrip()
    {
        const std::filesystem::path json_path    = GetFixturePath("ValidBehaviorBank.json");
        const std::filesystem::path output_path  = GetFixturePath("temp_roundtrip.dacbank");

        const decl_audio::compiler::CompileResult compile_result =
            decl_audio::compiler::LoadCompiledBankFromJsonFile(json_path);
        if (!Expect(!compile_result.HasErrors(), "round-trip: JSON should compile without errors"))
            return false;

        const decl_audio::assets::LoadResult asset_result =
            decl_audio::assets::LoadAssetBank(compile_result.bank, json_path);
        if (!Expect(!asset_result.HasErrors(), "round-trip: assets should load without errors"))
            return false;

        const decl_audio::compiler::CompiledBank &orig_bank  = compile_result.bank;
        const decl_audio::assets::AssetBank      &orig_audio = asset_result.bank;

        std::vector<decl_audio::Diagnostic> write_diags;
        if (!Expect(
                decl_audio::serialization::WriteBankToFile(
                    output_path.string().c_str(), orig_bank, orig_audio, write_diags),
                "round-trip: WriteBankToFile should succeed"))
        {
            std::cerr << decl_audio::DumpDiagnostics(write_diags);
            return false;
        }

        const decl_audio::serialization::LoadBankResult loaded =
            decl_audio::serialization::LoadBankFromFile(output_path.string().c_str());

        if (!Expect(!loaded.HasErrors(), "round-trip: LoadBankFromFile should succeed"))
        {
            std::cerr << decl_audio::DumpDiagnostics(loaded.diagnostics);
            return false;
        }

        const decl_audio::compiler::CompiledBank &lb = loaded.compiled_bank;
        const decl_audio::assets::AssetBank      &la = loaded.asset_bank;

        if (!Expect(lb.behaviors.size() == orig_bank.behaviors.size(), "round-trip: behaviors count"))
            return false;
        if (!Expect(lb.programs.size() == orig_bank.programs.size(), "round-trip: programs count"))
            return false;
        if (!Expect(lb.nodes.size() == orig_bank.nodes.size(), "round-trip: nodes count"))
            return false;
        if (!Expect(lb.conditions.size() == orig_bank.conditions.size(), "round-trip: conditions count"))
            return false;
        if (!Expect(lb.behavior_tags.size() == orig_bank.behavior_tags.size(), "round-trip: behavior_tags count"))
            return false;
        if (!Expect(lb.tag_depths.size() == orig_bank.tag_depths.size(), "round-trip: tag_depths count"))
            return false;
        if (!Expect(lb.max_program_node_count == orig_bank.max_program_node_count, "round-trip: max_program_node_count"))
            return false;
        if (!Expect(lb.max_program_parameter_slot_count == orig_bank.max_program_parameter_slot_count, "round-trip: max_program_parameter_slot_count"))
            return false;
        if (!Expect(lb.max_program_concurrent_voices == orig_bank.max_program_concurrent_voices, "round-trip: max_program_concurrent_voices"))
            return false;
        if (!Expect(lb.behavior_name_to_id == orig_bank.behavior_name_to_id, "round-trip: behavior_name_to_id map"))
            return false;
        if (!Expect(lb.program_name_to_id == orig_bank.program_name_to_id, "round-trip: program_name_to_id map"))
            return false;
        if (!Expect(lb.tag_name_to_id == orig_bank.tag_name_to_id, "round-trip: tag_name_to_id map"))
            return false;
        if (!Expect(lb.parameter_name_to_id == orig_bank.parameter_name_to_id, "round-trip: parameter_name_to_id map"))
            return false;
        if (!Expect(lb.asset_name_to_id == orig_bank.asset_name_to_id, "round-trip: asset_name_to_id map"))
            return false;
        if (!Expect(lb.asset_paths.empty(), "round-trip: asset_paths should be empty after LoadBank"))
            return false;

        if (!Expect(la.buffers.size() == orig_audio.buffers.size(), "round-trip: buffer count"))
            return false;
        for (std::size_t i = 0; i < la.buffers.size(); ++i)
        {
            const auto &a = orig_audio.buffers[i];
            const auto &b = la.buffers[i];
            if (!Expect(b.frame_count == a.frame_count, "round-trip: frame_count"))
                return false;
            if (!Expect(b.channel_count == a.channel_count, "round-trip: channel_count"))
                return false;
            if (!Expect(b.sample_rate == a.sample_rate, "round-trip: sample_rate"))
                return false;
            if (!Expect(b.samples.size() == a.samples.size(), "round-trip: sample count"))
                return false;
            if (!a.samples.empty())
            {
                if (!Expect(std::abs(b.samples.front() - a.samples.front()) < 1e-6f, "round-trip: first sample matches"))
                    return false;
                if (!Expect(std::abs(b.samples.back() - a.samples.back()) < 1e-6f, "round-trip: last sample matches"))
                    return false;
            }
        }

        // Wire into an engine and confirm it starts up cleanly (file still exists)
        const EngineConfig cfg = GetTestConfig();
        decl_audio::Engine engine(cfg);
        const bool engine_load_ok = engine.LoadBank(output_path.string().c_str());
        std::filesystem::remove(output_path);

        return Expect(engine_load_ok, "round-trip: Engine::LoadBank should return true");
    }

    bool TestMagicMismatch()
    {
        const std::filesystem::path path = GetFixturePath("temp_bad_magic.dacbank");
        {
            std::ofstream f(path, std::ios::binary);
            const std::uint32_t bad_magic = 0xDEADBEEFu;
            f.write(reinterpret_cast<const char *>(&bad_magic), sizeof(bad_magic));
        }

        const decl_audio::serialization::LoadBankResult result =
            decl_audio::serialization::LoadBankFromFile(path.string().c_str());
        std::filesystem::remove(path);

        if (!Expect(result.HasErrors(), "magic mismatch: should report an error"))
            return false;
        if (!Expect(
                result.diagnostics[0].message.find("invalid bank file magic") != std::string::npos,
                "magic mismatch: error message should mention 'invalid bank file magic'"))
            return false;
        return true;
    }

    bool TestVersionMismatch()
    {
        const std::filesystem::path path = GetFixturePath("temp_bad_version.dacbank");
        {
            std::ofstream f(path, std::ios::binary);
            const std::uint32_t magic   = decl_audio::serialization::kBankMagic;
            const std::uint32_t version = 99u;
            f.write(reinterpret_cast<const char *>(&magic),   sizeof(magic));
            f.write(reinterpret_cast<const char *>(&version), sizeof(version));
        }

        const decl_audio::serialization::LoadBankResult result =
            decl_audio::serialization::LoadBankFromFile(path.string().c_str());
        std::filesystem::remove(path);

        if (!Expect(result.HasErrors(), "version mismatch: should report an error"))
            return false;
        if (!Expect(
                result.diagnostics[0].message.find("unsupported bank version 99") != std::string::npos,
                "version mismatch: error message should mention 'unsupported bank version 99'"))
            return false;
        return true;
    }

    bool TestTruncatedFile()
    {
        const std::filesystem::path path = GetFixturePath("temp_truncated.dacbank");
        {
            std::ofstream f(path, std::ios::binary);
            const std::uint32_t magic   = decl_audio::serialization::kBankMagic;
            const std::uint32_t version = decl_audio::serialization::kBankVersion;
            // Write header only — scalars and everything after will be missing
            f.write(reinterpret_cast<const char *>(&magic),   sizeof(magic));
            f.write(reinterpret_cast<const char *>(&version), sizeof(version));
        }

        const decl_audio::serialization::LoadBankResult result =
            decl_audio::serialization::LoadBankFromFile(path.string().c_str());
        std::filesystem::remove(path);

        return Expect(result.HasErrors(), "truncated file: should report an error");
    }

    bool TestEmptyAndNullPath()
    {
        const EngineConfig cfg = GetTestConfig();
        decl_audio::Engine engine(cfg);

        if (!Expect(!engine.LoadBank(nullptr), "null path: LoadBank should return false"))
            return false;
        if (!Expect(!engine.LoadBank(""), "empty path: LoadBank should return false"))
            return false;
        if (!Expect(!engine.LoadBankAsync(nullptr), "null path: LoadBankAsync should return false"))
            return false;
        if (!Expect(!engine.LoadBankAsync(""), "empty path: LoadBankAsync should return false"))
            return false;
        return true;
    }

    bool TestLoadBankAsyncCompletesViaUpdate()
    {
        // Build a temp .dacbank from a known-good JSON fixture, then load it async.
        const std::filesystem::path json_path   = GetFixturePath("ValidBehaviorBank.json");
        const std::filesystem::path output_path = GetFixturePath("temp_async.dacbank");

        const decl_audio::compiler::CompileResult compile_result =
            decl_audio::compiler::LoadCompiledBankFromJsonFile(json_path);
        if (!Expect(!compile_result.HasErrors(), "async: JSON should compile"))
            return false;

        const decl_audio::assets::LoadResult asset_result =
            decl_audio::assets::LoadAssetBank(compile_result.bank, json_path);
        if (!Expect(!asset_result.HasErrors(), "async: assets should load"))
            return false;

        std::vector<decl_audio::Diagnostic> write_diags;
        if (!Expect(
                decl_audio::serialization::WriteBankToFile(
                    output_path.string().c_str(), compile_result.bank, asset_result.bank, write_diags),
                "async: WriteBankToFile should succeed"))
        {
            std::cerr << decl_audio::DumpDiagnostics(write_diags);
            return false;
        }

        bool ok = true;
        {
            const EngineConfig cfg = GetTestConfig();
            decl_audio::Engine engine(cfg);

            ok = Expect(engine.TryGetCompiledBank() == nullptr, "async: no bank before load") && ok;
            ok = Expect(engine.LoadBankAsync(output_path.string().c_str()), "async: first async load accepted") && ok;
            // In-flight until Update consumes the result; even if the worker already
            // finished, the flag stays set, so a concurrent load is rejected.
            ok = Expect(!engine.LoadBankAsync(output_path.string().c_str()), "async: concurrent load rejected") && ok;

            bool loaded = false;
            for (int i = 0; i < 2000 && !loaded; ++i)
            {
                engine.Update();
                loaded = engine.TryGetCompiledBank() != nullptr;
                if (!loaded)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            ok = Expect(loaded, "async: bank wired after pumping Update") && ok;

            // Loading the same path again returns success - now via the idempotent
            // early-out (the bank is already Active), so no second worker is spawned.
            ok = Expect(engine.LoadBankAsync(output_path.string().c_str()), "async: reload of loaded bank succeeds (idempotent)") && ok;
        }

        std::filesystem::remove(output_path);
        return ok;
    }

} // namespace

bool RunBankSerializerTests()
{
    if (!TestRoundTrip())         return false;
    if (!TestMagicMismatch())     return false;
    if (!TestVersionMismatch())   return false;
    if (!TestTruncatedFile())     return false;
    if (!TestEmptyAndNullPath())  return false;
    if (!TestLoadBankAsyncCompletesViaUpdate()) return false;

    std::cout << "BankSerializerTests passed\n";
    return true;
}

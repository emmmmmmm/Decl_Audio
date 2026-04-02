#include "pch.h"

#include "Engine.hpp"
#include "../assets/AssetBank.hpp"
#include "../compiler/Compiler.hpp"
#include "../runtime/HostCommands.hpp"

#include <string>

namespace decl_audio
{
    Engine::Engine(const EngineConfig &config) noexcept
        : api_version_(config.api_version),
          user_data_(config.user_data)
    {
        // spin up audio thread and set up commandbuffers
    }

    Engine::~Engine() = default;

    bool Engine::LoadBehaviors(const char *source_path) noexcept
    {
        load_diagnostics_.clear();

        if (source_path == nullptr || source_path[0] == '\0')
        {
            return false;
        }

        compiler::CompileResult compile_result = compiler::LoadCompiledBankFromJsonFile(source_path);
        load_diagnostics_ = compile_result.diagnostics;

        if (compile_result.HasErrors())
        {
            return false;
        }

        assets::LoadResult asset_result = assets::LoadAssetBank(compile_result.bank, source_path);
        load_diagnostics_.insert(load_diagnostics_.end(), asset_result.diagnostics.begin(), asset_result.diagnostics.end());

        if (asset_result.HasErrors())
        {
            return false;
        }

        compiled_bank_ = std::make_unique<compiler::CompiledBank>(std::move(compile_result.bank));
        asset_bank_ = std::make_unique<assets::AssetBank>(std::move(asset_result.bank));

        return true;
    }

    void Engine::Update() noexcept
    {
        control_runtime_.Tick();
        // update matching logic / run BehaviorResolver
        // send commands to audiothread
    }

    void Engine::SetTag(const char *entity_id, const char *tag) noexcept
    {
        control_runtime_.Submit(runtime::SetTagCommand{
            std::string(entity_id),
            compiled_bank_->GetTagId(tag)});
    }

    void Engine::RemoveTag(const char *entity_id, const char *tag) noexcept
    {
        control_runtime_.Submit(runtime::RemoveTagCommand{
            std::string(entity_id),
            compiled_bank_->GetTagId(tag)});
    }

    void Engine::SetValue(const char *entity_id, const char *parameter, float value) noexcept
    {
        control_runtime_.Submit(runtime::SetFloatValueCommand{
            std::string(entity_id),
            compiled_bank_->GetParameterId(parameter),
            value});
    }

    void Engine::DestroyEntity(const char *entity_id) noexcept
    {
        control_runtime_.Submit(runtime::DestroyEntityCommand{
            std::string(entity_id)});
    }

} // namespace decl_audio

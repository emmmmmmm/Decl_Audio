#include "pch.h"

#include "Engine.hpp"
#include "../compiler/Compiler.hpp"

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
        if (source_path == nullptr || source_path[0] == '\0')
        {
            return false;
        }

        compiler::CompileResult compile_result = compiler::LoadCompiledBankFromJsonFile(source_path);

        if (compile_result.HasErrors())
        {
            return false;
        }

        compiled_bank_ = std::make_unique<compiler::CompiledBank>(std::move(compile_result.bank));

        return true;
    }

    void Engine::Update() noexcept
    {
        // drain input buffer
        // update WorldState
        // update matching logic / run BehaviorResolver
        // send commands to audiothread
    }

} // namespace decl_audio

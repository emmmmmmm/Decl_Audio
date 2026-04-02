#pragma once

#include <cstdint>
#include <memory>

#include "Decl_Audio/Decl_Audio.h"
#include "../compiler/CompiledBank.hpp"
#include "../runtime/ControlRuntime.hpp"
#include "../runtime/WorldState.hpp"

namespace decl_audio
{
    class Engine
    {
    public:
        explicit Engine(const EngineConfig &config) noexcept;
        virtual ~Engine();

        Engine(const Engine &) = delete;
        Engine &operator=(const Engine &) = delete;
        Engine(Engine &&) = delete;
        Engine &operator=(Engine &&) = delete;

        virtual bool LoadBehaviors(const char *source_path) noexcept;
        virtual void Update() noexcept;
        virtual void SetTag(const char *entity_id, const char *tag) noexcept;
        virtual void RemoveTag(const char *entity_id, const char *tag) noexcept;
        virtual void SetValue(const char *entity_id, const char *parameter, float value) noexcept;
        virtual void DestroyEntity(const char *entity_id) noexcept;

        [[nodiscard]] uint32_t GetApiVersion() const noexcept
        {
            return api_version_;
        };

        [[nodiscard]] void *GetUserData() const noexcept
        {
            return user_data_;
        };

        [[nodiscard]] const runtime::WorldState &GetWorldState() const noexcept
        {
            return control_runtime_.GetWorldState();
        };

    private:
        std::unique_ptr<compiler::CompiledBank> compiled_bank_;
        runtime::ControlRuntime control_runtime_;
        uint32_t api_version_;
        void *user_data_;
    };
} // namespace decl_audio

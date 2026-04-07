#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../compiler/CompilerTypes.hpp"
#include "../core/vec3.hpp"

namespace decl_audio::runtime
{
    struct EntityState final
    {
        std::unordered_set<compiler::TagId> tags;
        std::unordered_set<compiler::TagId> transient_tags;
        std::unordered_map<compiler::ParameterId, float> float_values;
        float volume = 1.0f;
        Vec3 position{};
        bool has_volume = false;
        bool has_position = false;

        [[nodiscard]] bool HasTag(compiler::TagId tag_id) const noexcept
        {
            return tags.contains(tag_id) || transient_tags.contains(tag_id);
        }

        [[nodiscard]] bool HasFloatValue(compiler::ParameterId parameter_id) const noexcept
        {
            return float_values.contains(parameter_id);
        }

        [[nodiscard]] float GetFloatValue(compiler::ParameterId parameter_id) const
        {
            return float_values.at(parameter_id);
        }

        [[nodiscard]] bool HasVolume() const noexcept
        {
            return has_volume;
        }

        [[nodiscard]] float GetVolume() const noexcept
        {
            return volume;
        }

        [[nodiscard]] bool HasPosition() const noexcept
        {
            return has_position;
        }

        [[nodiscard]] const Vec3 &GetPosition() const noexcept
        {
            return position;
        }
    };

    struct WorldState final
    {
        std::unordered_map<std::string, EntityState> entities;

        std::unordered_set<compiler::TagId> global_tags;
        std::unordered_map<compiler::ParameterId, float> global_float_values;

        [[nodiscard]] bool HasEntity(const std::string &entity_id) const noexcept
        {
            return entities.contains(entity_id);
        }

        [[nodiscard]] const EntityState &GetEntity(const std::string &entity_id) const
        {
            return entities.at(entity_id);
        }

        [[nodiscard]] EntityState &GetOrCreateEntity(const std::string &entity_id)
        {
            return entities[entity_id];
        }
    };
} // namespace decl_audio::runtime

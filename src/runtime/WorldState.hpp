#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../compiler/CompilerTypes.hpp"

namespace decl_audio::runtime
{
    struct EntityState final
    {
        std::unordered_set<compiler::TagId> tags;
        std::unordered_map<compiler::ParameterId, float> float_values;

        [[nodiscard]] bool HasTag(compiler::TagId tag_id) const noexcept
        {
            return tags.contains(tag_id);
        }

        [[nodiscard]] float GetFloatValue(compiler::ParameterId parameter_id) const
        {
            return float_values.at(parameter_id);
        }
    };

    struct WorldState final
    {
        std::unordered_map<std::string, EntityState> entities;

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

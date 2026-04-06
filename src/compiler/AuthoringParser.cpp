#include "pch.h"

#include "Compiler.hpp"
#include "../ThirdParty/Json/json.hpp"

#include <limits>
#include <string>

namespace decl_audio::compiler
{
    using Json = nlohmann::json;

    namespace
    {
        [[nodiscard]] bool IsNumber(const Json &value)
        {
            return value.is_number_float() || value.is_number_integer() || value.is_number_unsigned();
        }

        void AppendStringArray(const Json &value,
                               const decl_audio::SourceLocation &location,
                               std::string_view field_path,
                               std::vector<std::string> &out_values,
                               std::vector<decl_audio::Diagnostic> &diagnostics)
        {
            if (!value.is_array())
            {
                diagnostics.push_back(MakeError(location.file_path, field_path, "must be an array of strings"));
                return;
            }

            for (std::size_t i = 0; i < value.size(); ++i)
            {
                const Json &entry = value[i];
                if (!entry.is_string())
                {
                    diagnostics.push_back(MakeError(location.file_path, std::string(field_path) + "[" + std::to_string(i) + "]", "must be a string"));
                    continue;
                }

                out_values.push_back(entry.get<std::string>());
            }
        }

        [[nodiscard]] ComparisonOp ParseComparisonOp(std::string_view op, bool &is_valid)
        {
            is_valid = true;

            if (op == "<")
                return ComparisonOp::Less;
            if (op == "<=")
                return ComparisonOp::LessOrEqual;
            if (op == "==")
                return ComparisonOp::Equal;
            if (op == ">=")
                return ComparisonOp::GreaterOrEqual;
            if (op == ">")
                return ComparisonOp::Greater;

            is_valid = false;
            return ComparisonOp::Equal;
        }

        [[nodiscard]] NodeType ParseNodeType(std::string_view type_name, bool &is_valid)
        {
            is_valid = true;

            if (type_name == "sequence")
                return NodeType::Sequence;
            if (type_name == "select")
                return NodeType::Select;
            if (type_name == "blend")
                return NodeType::Blend;
            if (type_name == "oneshot")
                return NodeType::OneShot;
            if (type_name == "loop")
                return NodeType::Loop;
            if (type_name == "random")
                return NodeType::Random;

            is_valid = false;
            return NodeType::OneShot;
        }

        [[nodiscard]] AttenuationMode ParseAttenuationMode(std::string_view attenuation_name, bool &is_valid)
        {
            is_valid = true;

            if (attenuation_name == "linear")
                return AttenuationMode::Linear;

            is_valid = false;
            return AttenuationMode::Linear;
        }

        AuthoringCondition ParseCondition(const Json &condition_json,
                                          std::string_view source_path,
                                          std::string_view field_path,
                                          std::vector<decl_audio::Diagnostic> &diagnostics)
        {
            AuthoringCondition condition;
            condition.location = MakeLocation(source_path, field_path);

            if (!condition_json.is_object())
            {
                diagnostics.push_back(MakeError(source_path, field_path, "must be an object"));
                return condition;
            }

            if (condition_json.contains("parameter"))
            {
                if (!condition_json["parameter"].is_string())
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".parameter", "must be a string"));
                else
                    condition.parameter = condition_json["parameter"].get<std::string>();
            }
            else
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".parameter", "is required"));
            }

            if (condition_json.contains("op"))
            {
                if (!condition_json["op"].is_string())
                {
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".op", "must be a string"));
                }
                else
                {
                    bool is_valid = false;
                    condition.op = ParseComparisonOp(condition_json["op"].get<std::string>(), is_valid);
                    if (!is_valid)
                        diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".op", "has unsupported comparison"));
                }
            }
            else
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".op", "is required"));
            }

            if (condition_json.contains("value"))
            {
                if (!IsNumber(condition_json["value"]))
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".value", "must be numeric"));
                else
                    condition.literal = condition_json["value"].get<float>();
            }
            else
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".value", "is required"));
            }

            return condition;
        }

        AuthoringNode ParseNode(const Json &container_json,
                                std::string_view source_path,
                                std::string_view field_path,
                                std::vector<decl_audio::Diagnostic> &diagnostics)
        {
            AuthoringNode node;
            node.location = MakeLocation(source_path, field_path);

            if (!container_json.is_object())
            {
                diagnostics.push_back(MakeError(source_path, field_path, "must be an object"));
                return node;
            }

            if (!container_json.contains("type"))
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".type", "is required"));
                return node;
            }

            if (!container_json["type"].is_string())
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".type", "must be a string"));
                return node;
            }

            bool is_valid = false;
            node.type = ParseNodeType(container_json["type"].get<std::string>(), is_valid);
            if (!is_valid)
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".type", "has unsupported container type"));
                return node;
            }

            if (container_json.contains("volume"))
            {
                if (!IsNumber(container_json["volume"]))
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".volume", "must be numeric"));
                else
                    node.volume = container_json["volume"].get<float>();
            }

            if (container_json.contains("loopCount"))
            {
                if (!container_json["loopCount"].is_number_integer())
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".loopCount", "must be an integer"));
                else
                    node.loop_count = container_json["loopCount"].get<std::int32_t>();
            }
            else if (node.type == NodeType::Loop)
            {
                node.loop_count = -1;
            }

            if (container_json.contains("parameter"))
            {
                if (!container_json["parameter"].is_string())
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".parameter", "must be a string"));
                else
                    node.parameter = container_json["parameter"].get<std::string>();
            }

            if (container_json.contains("asset"))
            {
                if (!container_json["asset"].is_string())
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".asset", "must be a string"));
                else
                    node.assets.push_back(container_json["asset"].get<std::string>());
            }

            if (container_json.contains("assets"))
            {
                AppendStringArray(container_json["assets"], node.location, std::string(field_path) + ".assets", node.assets, diagnostics);
            }

            // "children" and "containers" are aliases for the same thing.
            if (container_json.contains("children"))
            {
                const Json &children_json = container_json["children"];
                if (!children_json.is_array())
                {
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".children", "must be an array"));
                }
                else
                {
                    for (std::size_t i = 0; i < children_json.size(); ++i)
                        node.children.push_back(ParseNode(children_json[i], source_path, std::string(field_path) + ".children[" + std::to_string(i) + "]", diagnostics));
                }
            }

            if (container_json.contains("containers"))
            {
                const Json &children_json = container_json["containers"];
                if (!children_json.is_array())
                {
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".containers", "must be an array"));
                }
                else
                {
                    for (std::size_t i = 0; i < children_json.size(); ++i)
                        node.children.push_back(ParseNode(children_json[i], source_path, std::string(field_path) + ".containers[" + std::to_string(i) + "]", diagnostics));
                }
            }

            return node;
        }

        AuthoringSpatializationSettings ParseSpatialization(const Json &spatialization_json,
                                                            std::string_view source_path,
                                                            std::string_view field_path,
                                                            std::vector<decl_audio::Diagnostic> &diagnostics)
        {
            AuthoringSpatializationSettings spatialization;
            spatialization.location = MakeLocation(source_path, field_path);
            spatialization.mode = SpatializationMode::Pan;
            spatialization.min_distance = std::numeric_limits<float>::quiet_NaN();
            spatialization.max_distance = std::numeric_limits<float>::quiet_NaN();

            if (!spatialization_json.is_object())
            {
                diagnostics.push_back(MakeError(source_path, field_path, "must be an object"));
                return spatialization;
            }

            for (auto it = spatialization_json.begin(); it != spatialization_json.end(); ++it)
            {
                const std::string key = it.key();
                if (key != "minDistance" && key != "maxDistance" && key != "attenuation")
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + "." + key, "is not a supported spatialization field"));
            }

            if (!spatialization_json.contains("minDistance"))
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".minDistance", "is required"));
            }
            else if (!IsNumber(spatialization_json["minDistance"]))
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".minDistance", "must be numeric"));
            }
            else
            {
                spatialization.min_distance = spatialization_json["minDistance"].get<float>();
            }

            if (!spatialization_json.contains("maxDistance"))
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".maxDistance", "is required"));
            }
            else if (!IsNumber(spatialization_json["maxDistance"]))
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".maxDistance", "must be numeric"));
            }
            else
            {
                spatialization.max_distance = spatialization_json["maxDistance"].get<float>();
            }

            if (!spatialization_json.contains("attenuation"))
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".attenuation", "is required"));
            }
            else if (!spatialization_json["attenuation"].is_string())
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".attenuation", "must be a string"));
            }
            else
            {
                bool is_valid = false;
                spatialization.attenuation = ParseAttenuationMode(spatialization_json["attenuation"].get<std::string>(), is_valid);
                if (!is_valid)
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".attenuation", "has unsupported attenuation mode"));
            }

            return spatialization;
        }

        AuthoringBehavior ParseBehavior(const Json &behavior_json,
                                        std::string_view source_path,
                                        std::string_view field_path,
                                        std::vector<decl_audio::Diagnostic> &diagnostics)
        {
            AuthoringBehavior behavior;
            behavior.location = MakeLocation(source_path, field_path);

            if (!behavior_json.is_object())
            {
                diagnostics.push_back(MakeError(source_path, field_path, "must be an object"));
                return behavior;
            }

            if (behavior_json.contains("id"))
            {
                if (!behavior_json["id"].is_string())
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".id", "must be a string"));
                else
                    behavior.id = behavior_json["id"].get<std::string>();
            }
            else
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".id", "is required"));
            }

            if (behavior_json.contains("matchTags"))
                AppendStringArray(behavior_json["matchTags"], behavior.location, std::string(field_path) + ".matchTags", behavior.match_tags, diagnostics);

            if (behavior_json.contains("parameters"))
                AppendStringArray(behavior_json["parameters"], behavior.location, std::string(field_path) + ".parameters", behavior.parameters, diagnostics);

            if (behavior_json.contains("spatialization"))
                behavior.spatialization = ParseSpatialization(behavior_json["spatialization"], source_path, std::string(field_path) + ".spatialization", diagnostics);

            if (behavior_json.contains("matchConditions"))
            {
                const Json &conditions_json = behavior_json["matchConditions"];
                if (!conditions_json.is_array())
                {
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".matchConditions", "must be an array"));
                }
                else
                {
                    for (std::size_t i = 0; i < conditions_json.size(); ++i)
                        behavior.match_conditions.push_back(ParseCondition(conditions_json[i], source_path, std::string(field_path) + ".matchConditions[" + std::to_string(i) + "]", diagnostics));
                }
            }

            if (!behavior_json.contains("program"))
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".program", "is required"));
                return behavior;
            }

            const Json &program_json = behavior_json["program"];
            if (!program_json.is_array())
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".program", "must be an array"));
                return behavior;
            }

            for (std::size_t i = 0; i < program_json.size(); ++i)
                behavior.program.push_back(ParseNode(program_json[i], source_path, std::string(field_path) + ".program[" + std::to_string(i) + "]", diagnostics));

            return behavior;
        }
    } // namespace

    ParseResult ParseAuthoringJson(std::string_view source_text, std::string_view source_path)
    {
        ParseResult result;

        Json root = Json::parse(source_text.begin(), source_text.end(), nullptr, false, true);
        if (root.is_discarded())
        {
            result.diagnostics.push_back(MakeError(source_path, "failed to parse authoring json"));
            return result;
        }

        const Json *behaviors_json = nullptr;

        if (root.is_array())
        {
            behaviors_json = &root;
        }
        else if (root.is_object() && root.contains("behaviors"))
        {
            if (!root["behaviors"].is_array())
            {
                result.diagnostics.push_back(MakeError(source_path, "behaviors", "must be an array"));
                return result;
            }

            behaviors_json = &root["behaviors"];
        }
        else
        {
            result.diagnostics.push_back(MakeError(source_path, "<root>", "must be an array or an object containing a 'behaviors' array"));
            return result;
        }

        for (std::size_t i = 0; i < behaviors_json->size(); ++i)
            result.document.behaviors.push_back(ParseBehavior((*behaviors_json)[i], source_path, "behaviors[" + std::to_string(i) + "]", result.diagnostics));

        if (result.document.behaviors.empty())
            result.diagnostics.push_back(MakeError(source_path, "authoring document does not contain any behaviors"));

        return result;
    }
} // namespace decl_audio::compiler

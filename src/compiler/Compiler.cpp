#include "pch.h"

#include "Compiler.hpp"
#include "../ThirdParty/Json/json.hpp"

#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace decl_audio::compiler
{
    using Json = nlohmann::json;

    namespace
    {
        [[nodiscard]] SourceLocation MakeLocation(std::string_view source_path)
        {
            SourceLocation location;
            location.file_path = std::string(source_path);
            return location;
        }

        [[nodiscard]] Diagnostic MakeError(std::string_view source_path, std::string message)
        {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Error;
            diagnostic.location = MakeLocation(source_path);
            diagnostic.message = std::move(message);
            return diagnostic;
        }

        [[nodiscard]] Diagnostic MakeError(const SourceLocation &location, std::string message)
        {
            Diagnostic diagnostic;
            diagnostic.severity = DiagnosticSeverity::Error;
            diagnostic.location = location;
            diagnostic.message = std::move(message);
            return diagnostic;
        }

        [[nodiscard]] bool IsNumber(const Json &value)
        {
            return value.is_number_float() || value.is_number_integer() || value.is_number_unsigned();
        }

        void AppendStringArray(const Json &value,
                               const SourceLocation &location,
                               std::string_view field_path,
                               std::vector<std::string> &out_values,
                               std::vector<Diagnostic> &diagnostics)
        {
            if (!value.is_array())
            {
                diagnostics.push_back(MakeError(location, std::string(field_path) + " must be an array of strings"));
                return;
            }

            for (std::size_t i = 0; i < value.size(); ++i)
            {
                const Json &entry = value[i];
                if (!entry.is_string())
                {
                    diagnostics.push_back(MakeError(location, std::string(field_path) + "[" + std::to_string(i) + "] must be a string"));
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

        [[nodiscard]] AuthoringContainerType ParseAuthoringContainerType(std::string_view type_name, bool &is_valid)
        {
            is_valid = true;

            if (type_name == "oneshot")
                return AuthoringContainerType::OneShot;
            if (type_name == "loop")
                return AuthoringContainerType::Loop;
            if (type_name == "random")
                return AuthoringContainerType::Random;
            if (type_name == "sequence")
                return AuthoringContainerType::Sequence;

            is_valid = false;
            return AuthoringContainerType::OneShot;
        }

        AuthoringCondition ParseCondition(const Json &condition_json,
                                          std::string_view source_path,
                                          std::string_view field_path,
                                          std::vector<Diagnostic> &diagnostics)
        {
            AuthoringCondition condition;
            condition.location = MakeLocation(source_path);

            if (!condition_json.is_object())
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + " must be an object"));
                return condition;
            }

            if (condition_json.contains("parameter"))
            {
                if (!condition_json["parameter"].is_string())
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".parameter must be a string"));
                else
                    condition.parameter = condition_json["parameter"].get<std::string>();
            }
            else
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".parameter is required"));
            }

            if (condition_json.contains("op"))
            {
                if (!condition_json["op"].is_string())
                {
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".op must be a string"));
                }
                else
                {
                    bool is_valid = false;
                    condition.op = ParseComparisonOp(condition_json["op"].get<std::string>(), is_valid);
                    if (!is_valid)
                        diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".op has unsupported comparison"));
                }
            }
            else
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".op is required"));
            }

            if (condition_json.contains("value"))
            {
                if (!IsNumber(condition_json["value"]))
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".value must be numeric"));
                else
                    condition.literal = condition_json["value"].get<float>();
            }
            else
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".value is required"));
            }

            return condition;
        }

        AuthoringContainer ParseContainer(const Json &container_json,
                                          std::string_view source_path,
                                          std::string_view field_path,
                                          std::vector<Diagnostic> &diagnostics)
        {
            AuthoringContainer container;
            container.location = MakeLocation(source_path);

            if (!container_json.is_object())
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + " must be an object"));
                return container;
            }

            if (!container_json.contains("type"))
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".type is required"));
                return container;
            }

            if (!container_json["type"].is_string())
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".type must be a string"));
                return container;
            }

            bool is_valid = false;
            container.type = ParseAuthoringContainerType(container_json["type"].get<std::string>(), is_valid);
            if (!is_valid)
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".type has unsupported container type"));
                return container;
            }

            if (container_json.contains("volume"))
            {
                if (!IsNumber(container_json["volume"]))
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".volume must be numeric"));
                else
                    container.volume = container_json["volume"].get<float>();
            }

            if (container_json.contains("loopCount"))
            {
                if (!container_json["loopCount"].is_number_integer())
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".loopCount must be an integer"));
                else
                    container.loop_count = container_json["loopCount"].get<std::int32_t>();
            }
            else if (container.type == AuthoringContainerType::Loop)
            {
                container.loop_count = -1;
            }

            if (container_json.contains("asset"))
            {
                if (!container_json["asset"].is_string())
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".asset must be a string"));
                else
                    container.assets.push_back(container_json["asset"].get<std::string>());
            }

            if (container_json.contains("assets"))
            {
                AppendStringArray(container_json["assets"], container.location, std::string(field_path) + ".assets", container.assets, diagnostics);
            }

            if (container_json.contains("children"))
            {
                const Json &children_json = container_json["children"];
                if (!children_json.is_array())
                {
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".children must be an array"));
                }
                else
                {
                    for (std::size_t i = 0; i < children_json.size(); ++i)
                        container.children.push_back(ParseContainer(children_json[i], source_path, std::string(field_path) + ".children[" + std::to_string(i) + "]", diagnostics));
                }
            }

            if (container_json.contains("containers"))
            {
                const Json &children_json = container_json["containers"];
                if (!children_json.is_array())
                {
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".containers must be an array"));
                }
                else
                {
                    for (std::size_t i = 0; i < children_json.size(); ++i)
                        container.children.push_back(ParseContainer(children_json[i], source_path, std::string(field_path) + ".containers[" + std::to_string(i) + "]", diagnostics));
                }
            }

            return container;
        }

        AuthoringBehavior ParseBehavior(const Json &behavior_json,
                                        std::string_view source_path,
                                        std::string_view field_path,
                                        std::vector<Diagnostic> &diagnostics)
        {
            AuthoringBehavior behavior;
            behavior.location = MakeLocation(source_path);

            if (!behavior_json.is_object())
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + " must be an object"));
                return behavior;
            }

            if (behavior_json.contains("id"))
            {
                if (!behavior_json["id"].is_string())
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".id must be a string"));
                else
                    behavior.id = behavior_json["id"].get<std::string>();
            }
            else
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".id is required"));
            }

            if (behavior_json.contains("matchTags"))
                AppendStringArray(behavior_json["matchTags"], behavior.location, std::string(field_path) + ".matchTags", behavior.match_tags, diagnostics);

            if (behavior_json.contains("parameters"))
                AppendStringArray(behavior_json["parameters"], behavior.location, std::string(field_path) + ".parameters", behavior.parameters, diagnostics);

            if (behavior_json.contains("matchConditions"))
            {
                const Json &conditions_json = behavior_json["matchConditions"];
                if (!conditions_json.is_array())
                {
                    diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".matchConditions must be an array"));
                }
                else
                {
                    for (std::size_t i = 0; i < conditions_json.size(); ++i)
                        behavior.match_conditions.push_back(ParseCondition(conditions_json[i], source_path, std::string(field_path) + ".matchConditions[" + std::to_string(i) + "]", diagnostics));
                }
            }

            if (!behavior_json.contains("program"))
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".program is required"));
                return behavior;
            }

            const Json &program_json = behavior_json["program"];
            if (!program_json.is_array())
            {
                diagnostics.push_back(MakeError(source_path, std::string(field_path) + ".program must be an array"));
                return behavior;
            }

            for (std::size_t i = 0; i < program_json.size(); ++i)
                behavior.program.push_back(ParseContainer(program_json[i], source_path, std::string(field_path) + ".program[" + std::to_string(i) + "]", diagnostics));

            return behavior;
        }

        template <typename IdType>
        [[nodiscard]] IdType InternName(std::unordered_map<std::string, IdType> &name_to_id, const std::string &name)
        {
            const auto [it, inserted] = name_to_id.emplace(name, static_cast<IdType>(name_to_id.size()));
            (void)inserted;
            return it->second;
        }

        [[nodiscard]] AssetId InternAsset(CompiledBank &bank, const std::string &asset_name)
        {
            auto it = bank.asset_name_to_id.find(asset_name);
            if (it != bank.asset_name_to_id.end())
                return it->second;

            const AssetId id = static_cast<AssetId>(bank.asset_paths.size());
            bank.asset_name_to_id.emplace(asset_name, id);
            bank.asset_paths.push_back(asset_name);
            return id;
        }

        void LowerContainerList(CompiledBank &bank,
                                const std::vector<AuthoringContainer> &authoring_containers,
                                std::string_view behavior_id,
                                std::vector<Diagnostic> &diagnostics)
        {
            for (const AuthoringContainer &authoring_container : authoring_containers)
            {
                if (authoring_container.type == AuthoringContainerType::Sequence)
                {
                    if (authoring_container.children.empty())
                    {
                        diagnostics.push_back(MakeError(authoring_container.location, "behavior '" + std::string(behavior_id) + "' has an empty sequence container"));
                        continue;
                    }

                    LowerContainerList(bank, authoring_container.children, behavior_id, diagnostics);
                    continue;
                }

                CompiledContainer compiled_container;
                compiled_container.volume = authoring_container.volume;
                compiled_container.first_asset = static_cast<std::uint32_t>(bank.container_assets.size());
                compiled_container.loop_count = authoring_container.loop_count;

                switch (authoring_container.type)
                {
                case AuthoringContainerType::OneShot:
                    compiled_container.type = ContainerType::OneShot;
                    if (authoring_container.assets.size() != 1)
                        diagnostics.push_back(MakeError(authoring_container.location, "behavior '" + std::string(behavior_id) + "' oneshot containers require exactly one asset"));
                    break;

                case AuthoringContainerType::Loop:
                    compiled_container.type = ContainerType::Loop;
                    if (authoring_container.assets.size() != 1)
                        diagnostics.push_back(MakeError(authoring_container.location, "behavior '" + std::string(behavior_id) + "' loop containers require exactly one asset"));
                    break;

                case AuthoringContainerType::Random:
                    compiled_container.type = ContainerType::Random;
                    if (authoring_container.assets.empty())
                        diagnostics.push_back(MakeError(authoring_container.location, "behavior '" + std::string(behavior_id) + "' random containers require at least one asset"));
                    break;

                case AuthoringContainerType::Sequence:
                    break;
                }

                for (const std::string &asset_name : authoring_container.assets)
                    bank.container_assets.push_back(InternAsset(bank, asset_name));

                compiled_container.asset_count = static_cast<std::uint32_t>(bank.container_assets.size() - compiled_container.first_asset);
                bank.containers.push_back(compiled_container);
            }
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
                result.diagnostics.push_back(MakeError(source_path, "root.behaviors must be an array"));
                return result;
            }

            behaviors_json = &root["behaviors"];
        }
        else
        {
            result.diagnostics.push_back(MakeError(source_path, "root must be an array or an object containing a 'behaviors' array"));
            return result;
        }

        for (std::size_t i = 0; i < behaviors_json->size(); ++i)
            result.document.behaviors.push_back(ParseBehavior((*behaviors_json)[i], source_path, "behaviors[" + std::to_string(i) + "]", result.diagnostics));

        if (result.document.behaviors.empty())
            result.diagnostics.push_back(MakeError(source_path, "authoring document does not contain any behaviors"));

        return result;
    }

    CompileResult CompileAuthoringDocument(const AuthoringDocument &document)
    {
        CompileResult result;

        for (const AuthoringBehavior &behavior : document.behaviors)
        {
            if (behavior.id.empty())
            {
                result.diagnostics.push_back(MakeError(behavior.location, "behavior id must not be empty"));
                continue;
            }

            const BehaviorId behavior_id = static_cast<BehaviorId>(result.bank.behaviors.size());
            const auto [behavior_it, behavior_inserted] = result.bank.behavior_name_to_id.emplace(behavior.id, behavior_id);
            if (!behavior_inserted)
                result.diagnostics.push_back(MakeError(behavior.location, "duplicate behavior id '" + behavior.id + "'"));

            for (const std::string &parameter_name : behavior.parameters)
                (void)InternName(result.bank.parameter_name_to_id, parameter_name);

            const std::uint32_t first_tag = static_cast<std::uint32_t>(result.bank.behavior_tags.size());
            for (const std::string &tag_name : behavior.match_tags)
                result.bank.behavior_tags.push_back(InternName(result.bank.tag_name_to_id, tag_name));

            const std::uint32_t first_condition = static_cast<std::uint32_t>(result.bank.conditions.size());
            for (const AuthoringCondition &condition : behavior.match_conditions)
            {
                if (condition.parameter.empty())
                {
                    result.diagnostics.push_back(MakeError(condition.location, "behavior '" + behavior.id + "' has a condition with an empty parameter name"));
                    continue;
                }

                CompiledCondition compiled_condition;
                compiled_condition.parameter_id = InternName(result.bank.parameter_name_to_id, condition.parameter);
                compiled_condition.op = condition.op;
                compiled_condition.literal = condition.literal;
                result.bank.conditions.push_back(compiled_condition);
            }

            const ProgramId program_id = static_cast<ProgramId>(result.bank.programs.size());
            result.bank.program_name_to_id.emplace(behavior.id, program_id);

            const std::uint32_t first_container = static_cast<std::uint32_t>(result.bank.containers.size());
            LowerContainerList(result.bank, behavior.program, behavior.id, result.diagnostics);
            const std::uint32_t container_count = static_cast<std::uint32_t>(result.bank.containers.size() - first_container);

            if (container_count == 0)
                result.diagnostics.push_back(MakeError(behavior.location, "behavior '" + behavior.id + "' compiled to an empty program"));

            CompiledProgram compiled_program;
            compiled_program.id = program_id;
            compiled_program.first_container = first_container;
            compiled_program.container_count = container_count;
            result.bank.programs.push_back(compiled_program);

            CompiledBehavior compiled_behavior;
            compiled_behavior.id = behavior_it->second;
            compiled_behavior.program_id = program_id;
            compiled_behavior.first_tag = first_tag;
            compiled_behavior.tag_count = static_cast<std::uint32_t>(result.bank.behavior_tags.size() - first_tag);
            compiled_behavior.first_condition = first_condition;
            compiled_behavior.condition_count = static_cast<std::uint32_t>(result.bank.conditions.size() - first_condition);
            result.bank.behaviors.push_back(compiled_behavior);
        }

        return result;
    }

    CompileResult LoadCompiledBankFromJsonFile(const std::filesystem::path &source_path)
    {
        CompileResult result;

        std::ifstream input(source_path, std::ios::binary);
        if (!input.is_open())
        {
            result.diagnostics.push_back(MakeError(source_path.string(), "failed to open authoring file"));
            return result;
        }

        const std::string source_text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        ParseResult parse_result = ParseAuthoringJson(source_text, source_path.string());

        result.diagnostics = parse_result.diagnostics;
        if (parse_result.HasErrors())
            return result;

        CompileResult compile_result = CompileAuthoringDocument(parse_result.document);
        result.bank = std::move(compile_result.bank);
        result.diagnostics.insert(result.diagnostics.end(), compile_result.diagnostics.begin(), compile_result.diagnostics.end());
        return result;
    }
} // namespace decl_audio::compiler

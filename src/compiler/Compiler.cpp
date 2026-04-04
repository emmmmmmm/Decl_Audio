#include "pch.h"

#include "Compiler.hpp"

#include <limits>
#include <sstream>
#include <string>

namespace decl_audio::compiler
{
    namespace
    {
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
                    if (authoring_container.loop_count == 0)
                        diagnostics.push_back(MakeError(authoring_container.location, "behavior '" + std::string(behavior_id) + "' loop containers do not allow loopCount = 0"));
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

        [[nodiscard]] const char *ToString(ComparisonOp op)
        {
            switch (op)
            {
            case ComparisonOp::Less:
                return "<";
            case ComparisonOp::LessOrEqual:
                return "<=";
            case ComparisonOp::Equal:
                return "==";
            case ComparisonOp::GreaterOrEqual:
                return ">=";
            case ComparisonOp::Greater:
                return ">";
            }

            return "<invalid>";
        }

        [[nodiscard]] const char *ToString(ContainerType type)
        {
            switch (type)
            {
            case ContainerType::OneShot:
                return "oneshot";
            case ContainerType::Loop:
                return "loop";
            case ContainerType::Random:
                return "random";
            }

            return "<invalid>";
        }

        [[nodiscard]] const char *ToString(SpatializationMode mode)
        {
            switch (mode)
            {
            case SpatializationMode::None:
                return "none";
            case SpatializationMode::Pan:
                return "pan";
            }

            return "<invalid>";
        }

        [[nodiscard]] const char *ToString(AttenuationMode attenuation)
        {
            switch (attenuation)
            {
            case AttenuationMode::Linear:
                return "linear";
            }

            return "<invalid>";
        }
    } // namespace

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
            {
                if (parameter_name.empty())
                {
                    result.diagnostics.push_back(MakeError(behavior.location, "behavior '" + behavior.id + "' declares an empty parameter name"));
                    continue;
                }

                if (parameter_name == "volume" || parameter_name == "position")
                {
                    result.diagnostics.push_back(MakeError(behavior.location, "behavior '" + behavior.id + "' must not declare reserved runtime parameter '" + parameter_name + "'"));
                    continue;
                }

                (void)InternName(result.bank.parameter_name_to_id, parameter_name);
            }

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

                if (condition.parameter == "volume" || condition.parameter == "position")
                {
                    result.diagnostics.push_back(MakeError(condition.location, "behavior '" + behavior.id + "' must not use reserved runtime parameter '" + condition.parameter + "' in match conditions"));
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
            compiled_program.spatialization.mode = behavior.spatialization.mode;
            compiled_program.spatialization.min_distance = behavior.spatialization.min_distance;
            compiled_program.spatialization.max_distance = behavior.spatialization.max_distance;
            compiled_program.spatialization.attenuation = behavior.spatialization.attenuation;

            if (compiled_program.spatialization.mode == SpatializationMode::Pan)
            {
                if (compiled_program.spatialization.min_distance < 0.0f)
                    result.diagnostics.push_back(MakeError(behavior.spatialization.location, "behavior '" + behavior.id + "' spatialization minDistance must be >= 0"));

                if (compiled_program.spatialization.max_distance <= compiled_program.spatialization.min_distance)
                    result.diagnostics.push_back(MakeError(behavior.spatialization.location, "behavior '" + behavior.id + "' spatialization maxDistance must be > minDistance"));
            }

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

    std::string DumpCompiledBank(const CompiledBank &bank)
    {
        std::ostringstream stream;
        stream << "CompiledBank\n";
        stream << "  behaviors: " << bank.behaviors.size() << '\n';
        stream << "  programs: " << bank.programs.size() << '\n';
        stream << "  containers: " << bank.containers.size() << '\n';
        stream << "  assets: " << bank.asset_paths.size() << '\n';
        stream << "  tags: " << bank.tag_name_to_id.size() << '\n';
        stream << "  parameters: " << bank.parameter_name_to_id.size() << '\n';

        for (const CompiledBehavior &behavior : bank.behaviors)
        {
            stream << "Behavior[" << behavior.id << "]\n";
            stream << "  program: " << behavior.program_id << '\n';
            const CompiledProgram &program = bank.GetProgram(behavior.program_id);
            stream << "  spatialization: mode=" << ToString(program.spatialization.mode);
            if (program.spatialization.mode != SpatializationMode::None)
            {
                stream << " minDistance=" << program.spatialization.min_distance
                       << " maxDistance=" << program.spatialization.max_distance
                       << " attenuation=" << ToString(program.spatialization.attenuation);
            }
            stream << '\n';
            stream << "  tags:";

            for (TagId tag_id : bank.GetBehaviorTags(behavior.id))
                stream << ' ' << tag_id;

            stream << '\n';
            stream << "  conditions:\n";

            for (const CompiledCondition &condition : bank.GetBehaviorConditions(behavior.id))
            {
                stream << "    parameter=" << condition.parameter_id
                       << ' ' << ToString(condition.op)
                       << ' ' << condition.literal
                       << '\n';
            }

            stream << "  containers:\n";

            for (const CompiledContainer &container : bank.GetProgramContainers(behavior.program_id))
            {
                stream << "    type=" << ToString(container.type)
                       << " volume=" << container.volume
                       << " loopCount=" << container.loop_count
                       << " assets=";

                for (AssetId asset_id : bank.GetContainerAssets(container))
                    stream << ' ' << asset_id << '[' << bank.GetAssetPath(asset_id) << ']';

                stream << '\n';
            }
        }

        return stream.str();
    }
} // namespace decl_audio::compiler

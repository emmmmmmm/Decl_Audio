#include "pch.h"

#include "Compiler.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>

namespace decl_audio::compiler
{
    namespace
    {
        [[nodiscard]] const char *ToString(ComparisonOp op)
        {
            switch (op)
            {
            case ComparisonOp::Less:           return "<";
            case ComparisonOp::LessOrEqual:    return "<=";
            case ComparisonOp::Equal:          return "==";
            case ComparisonOp::GreaterOrEqual: return ">=";
            case ComparisonOp::Greater:        return ">";
            }
            return "<invalid>";
        }

        [[nodiscard]] const char *ToString(NodeType type)
        {
            switch (type)
            {
            case NodeType::Sequence: return "sequence";
            case NodeType::Select:   return "select";
            case NodeType::Blend:    return "blend";
            case NodeType::OneShot:  return "oneshot";
            case NodeType::Loop:     return "loop";
            case NodeType::Random:   return "random";
            }
            return "<invalid>";
        }

        [[nodiscard]] const char *ToString(SpatializationMode mode)
        {
            switch (mode)
            {
            case SpatializationMode::None: return "none";
            case SpatializationMode::Pan:  return "pan";
            }
            return "<invalid>";
        }

        [[nodiscard]] const char *ToString(AttenuationMode attenuation)
        {
            switch (attenuation)
            {
            case AttenuationMode::Linear: return "linear";
            }
            return "<invalid>";
        }

        [[nodiscard]] const char *ToString(StopMode mode)
        {
            switch (mode)
            {
            case StopMode::Immediate: return "immediate";
            case StopMode::Graceful:  return "graceful";
            }
            return "<invalid>";
        }
    } // namespace

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

    std::string DumpCompiledBank(const CompiledBank &bank)
    {
        std::ostringstream stream;
        stream << "CompiledBank\n";
        stream << "  behaviors: " << bank.behaviors.size() << '\n';
        stream << "  programs: "  << bank.programs.size() << '\n';
        stream << "  nodes: "     << bank.nodes.size() << '\n';
        stream << "  assets: "    << bank.asset_paths.size() << '\n';
        stream << "  tags: "      << bank.tag_name_to_id.size() << '\n';
        stream << "  parameters: "<< bank.parameter_name_to_id.size() << '\n';

        for (const CompiledBehavior &behavior : bank.behaviors)
        {
            stream << "Behavior[" << behavior.id << "]\n";
            stream << "  program: " << behavior.program_id << '\n';
            const CompiledProgram &program = bank.GetProgram(behavior.program_id);
            stream << "  stop: mode=" << ToString(program.stop_mode)
                   << " fadeFrames=" << program.stop_fade_frames
                   << " startFadeFrames=" << program.start_fade_frames << '\n';
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
            stream << "  nodes:\n";
            for (const CompiledNode &node : bank.GetProgramNodes(behavior.program_id))
            {
                stream << "    type=" << ToString(node.type)
                       << " parent=" << node.parent
                       << " gain=" << node.authored_gain
                       << " children=" << node.child_count
                       << " paramSlot=" << node.parameter_slot
                       << " loopCount=" << node.loop_count
                       << " assets=";
                for (AssetId asset_id : bank.GetNodeAssets(node))
                    stream << ' ' << asset_id << '['
                           << (bank.asset_paths.empty() ? "<packed>" : bank.GetAssetPath(asset_id))
                           << ']';
                stream << '\n';
            }
        }

        return stream.str();
    }
} // namespace decl_audio::compiler

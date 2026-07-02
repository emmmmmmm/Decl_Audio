#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "../compiler/CompiledBank.hpp"
#include "../compiler/CompilerTypes.hpp"

namespace decl_audio::runtime
{
    // Owns the global vocabulary: the name->id maps for tags and parameters plus
    // the name-derived tag hierarchy (`tag_depths`, `tag_group_head`). Vocabulary
    // ids are shared across all banks and the world state ("rpm" means one thing
    // everywhere); content ids (programs, nodes, assets) stay bank-local and never
    // pass through here.
    //
    // Single-writer: mutated only on the control thread - via AdoptBank (load) and
    // GetOrIntern* (host-command drain). The resolver only reads it. Monotonic:
    // once minted, an id never moves (that is about id stability, not thread
    // safety; the single-writer rule is what makes lock-free access safe).
    //
    // MergeBank interns a freshly loaded bank's vocabulary additively and rewrites
    // the bank's vocabulary fields in place to global ids. The merged tag set drives
    // the recomputed hierarchy, so exclusive namespaces span banks (bank A's
    // "movement.x" and bank B's "movement.y" share one group).
    class VocabularyRegistry final
    {
    public:
        // Intern `bank`'s vocabulary into the registry (additive) and rewrite ONLY
        // its vocabulary fields - behavior_tags, conditions[].parameter_id, and
        // program_parameters - from bank-local ids to global ids. Content arrays
        // (programs, nodes, node_children, node_assets, audio buffers) and
        // node.parameter_slot are LEFT ALONE; the instance carries the bank, so
        // local content ids never need re-indexing. The bank's own name->id maps
        // keep their stale local ids but are no longer the runtime path.
        void MergeBank(compiler::CompiledBank &bank)
        {
            // Bank-local ids are dense (0..size-1), assigned in JSON parse order at
            // compile time - deterministic on every platform. But bank.tag_name_to_id
            // itself is an unordered_map, so iterating it directly (as this used to do)
            // visits names in hash/bucket order, which is STL-implementation-defined and
            // differs between MSVC and libstdc++. That let the global id assigned to a
            // given name vary by platform even though the local id never does. Reorder
            // by local id first so GetOrInternTag/GetOrInternParam are always called in
            // the same, platform-independent sequence.
            std::vector<std::string_view> tag_names_by_local_id(bank.tag_name_to_id.size());
            for (const auto &[name, local_id] : bank.tag_name_to_id)
            {
                tag_names_by_local_id[local_id] = name;
            }
            std::vector<compiler::TagId> tag_remap(bank.tag_name_to_id.size());
            for (std::size_t local_id = 0; local_id < tag_names_by_local_id.size(); ++local_id)
            {
                tag_remap[local_id] = GetOrInternTag(tag_names_by_local_id[local_id]);
            }

            std::vector<std::string_view> param_names_by_local_id(bank.parameter_name_to_id.size());
            for (const auto &[name, local_id] : bank.parameter_name_to_id)
            {
                param_names_by_local_id[local_id] = name;
            }
            std::vector<compiler::ParameterId> param_remap(bank.parameter_name_to_id.size());
            for (std::size_t local_id = 0; local_id < param_names_by_local_id.size(); ++local_id)
            {
                param_remap[local_id] = GetOrInternParam(param_names_by_local_id[local_id]);
            }

            for (compiler::TagId &tag_id : bank.behavior_tags)
            {
                tag_id = tag_remap[tag_id];
            }
            for (compiler::CompiledCondition &condition : bank.conditions)
            {
                condition.parameter_id = param_remap[condition.parameter_id];
            }
            for (compiler::ParameterId &parameter_id : bank.program_parameters)
            {
                parameter_id = param_remap[parameter_id];
            }
        }

        [[nodiscard]] compiler::TagId GetOrInternTag(std::string_view name)
        {
            const auto it = tag_name_to_id_.find(std::string(name));
            if (it != tag_name_to_id_.end())
            {
                return it->second;
            }

            const compiler::TagId id = static_cast<compiler::TagId>(tag_name_to_id_.size());
            tag_name_to_id_.emplace(std::string(name), id);

            const std::uint8_t depth = static_cast<std::uint8_t>(std::count(name.begin(), name.end(), '.'));
            const auto [head_it, inserted] = prefix_to_head_.emplace(GroupPrefix(name), id);
            tag_depths_.push_back(depth);
            tag_group_head_.push_back(head_it->second);
            return id;
        }

        [[nodiscard]] compiler::ParameterId GetOrInternParam(std::string_view name)
        {
            const auto it = parameter_name_to_id_.find(std::string(name));
            if (it != parameter_name_to_id_.end())
            {
                return it->second;
            }

            const compiler::ParameterId id = static_cast<compiler::ParameterId>(parameter_name_to_id_.size());
            parameter_name_to_id_.emplace(std::string(name), id);
            return id;
        }

        // Canonical TagId for the exclusive namespace group a tag belongs to (tags
        // sharing a first component evict one another). Every id returned by
        // GetOrInternTag has an entry, so callers need no bounds guard.
        [[nodiscard]] compiler::TagId TagGroupHead(compiler::TagId tag_id) const noexcept
        {
            return tag_group_head_[tag_id];
        }

    private:
        // A tag's group is its first component (everything before the first '.').
        // Bare tags form their own singleton group. Matches the compiler's rule.
        [[nodiscard]] static std::string GroupPrefix(std::string_view name)
        {
            const std::size_t dot_pos = name.find('.');
            return std::string(dot_pos != std::string_view::npos ? name.substr(0, dot_pos) : name);
        }

        std::unordered_map<std::string, compiler::TagId> tag_name_to_id_;
        std::unordered_map<std::string, compiler::ParameterId> parameter_name_to_id_;
        std::vector<std::uint8_t> tag_depths_;
        std::vector<compiler::TagId> tag_group_head_;
        std::unordered_map<std::string, compiler::TagId> prefix_to_head_;
    };
} // namespace decl_audio::runtime

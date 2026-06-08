#pragma once

#include <string>

#include "../assets/AssetBank.hpp"
#include "../compiler/CompiledBank.hpp"
#include "BankId.hpp"

namespace decl_audio
{
    // Control-side per-bank status. The audio thread runs its own
    // Active/Retiring/Drained machine per slot (see AudioRuntime); this is the
    // control half that owns bucket memory and the slot table.
    enum class BankStatus : std::uint8_t
    {
        Active,   // resolvable; mints instances
        Retiring, // unload requested; resolver skips it, waiting for the audio drain
    };

    // The ownership unit: one heap-owned, address-stable bank. Content ids
    // (programs/nodes/assets) stay LOCAL to this bank; only vocabulary ids were
    // rewritten to global ids at merge time. The audio thread holds raw pointers
    // into `compiled`/`assets`, so these must not move - kept behind a unique_ptr
    // in the engine's slot table.
    struct LoadedBank final
    {
        BankId id;
        compiler::CompiledBank compiled; // content ids stay local
        assets::AssetBank assets;
        std::string source_path; // how the host names this bank for unload
        BankStatus status = BankStatus::Active;
    };
} // namespace decl_audio

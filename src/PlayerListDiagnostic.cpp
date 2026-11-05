#include "PlayerListDiagnostic.h"

#include <atomic>
#include "ll/api/io/Logger.h"
#include "ll/api/io/LoggerRegistry.h"
#include "ll/api/memory/Hook.h"
#include "mc/network/packet/PlayerListPacket.h"

namespace ResourcePackFix {
static std::atomic<unsigned int> diagnosticCount{0};
static bool hookInstalled = false;

void PlayerListDiagnostic::log(PlayerListPacket const& packet, char const* phase, int overrideMode) {
    if (diagnosticCount.fetch_add(1, std::memory_order_relaxed) >= 64) return;
    auto logger = ll::io::LoggerRegistry::getInstance().getOrCreate("PlayerListDiag");
    auto const& entries = packet.mEntries.get();
    size_t adds = 0;
    size_t removes = 0;
    PlayerListPacketPayload::AddEntry const* first = nullptr;
    for (auto const& item : entries) {
        if (auto add = std::get_if<PlayerListPacketPayload::AddEntry>(&item)) {
            ++adds;
            if (!first) first = add;
        } else {
            ++removes;
        }
    }
    logger->info("phase={} packet={} mode={} override={} entries={} add={} remove={}",
        phase, static_cast<void const*>(&packet), static_cast<int>(packet.mSerializationMode),
        overrideMode, entries.size(), adds, removes);
    if (first) {
        logger->info("firstAdd action={} platform={} nameBytes={} xuidBytes={} platformIdBytes={}",
            static_cast<int>(first->mAction), static_cast<int>(first->mBuildPlatform),
            first->mName.get().size(), first->mXUID.get().size(), first->mPlatformOnlineId.get().size());
    }
}

LL_TYPE_INSTANCE_HOOK(
    PlayerListSerializationHook,
    ll::memory::HookPriority::Normal,
    PlayerListPacket,
    &PlayerListPacket::$writeWithSerializationMode,
    void,
    BinaryStream& stream,
    cereal::ReflectionCtx const& reflectionCtx,
    std::optional<SerializationMode> overrideMode
) {
    PlayerListDiagnostic::log(*this, "serialize", overrideMode ? static_cast<int>(*overrideMode) : -1);
    origin(stream, reflectionCtx, overrideMode);
}

bool PlayerListDiagnostic::install() {
    if (hookInstalled) return true;
    hookInstalled = PlayerListSerializationHook::hook() == 0;
    ll::io::LoggerRegistry::getInstance().getOrCreate("PlayerListDiag")->info(
        "diagnostic-v1 hookInstalled={} (read-only, no packet fields changed)", hookInstalled);
    return hookInstalled;
}

void PlayerListDiagnostic::uninstall() {
    if (hookInstalled) {
        PlayerListSerializationHook::unhook();
        hookInstalled = false;
    }
}
} // namespace ResourcePackFix

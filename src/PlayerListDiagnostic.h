#pragma once
class PlayerListPacket;

namespace ResourcePackFix {
class PlayerListDiagnostic {
public:
    static bool install();
    static void uninstall();
    static void log(PlayerListPacket const& packet, char const* phase, int overrideMode = -1);
};
} // namespace ResourcePackFix

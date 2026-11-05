#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "mc/platform/UUID.h"

class FakePlayerManager {
    struct PendingFakeEntry {
        std::string name;
        std::string xuid;
        mce::UUID   uuid;
        int64_t     actorId;
    };

    std::unordered_map<std::string, PendingFakeEntry> liveEntries_;
    std::vector<PendingFakeEntry>                     pendingAdds_;
    std::unordered_map<std::string, std::string>      stagedNames_;

    std::mutex mutex_;

    bool     bulkMode_        = false;
    bool     syncTransaction_ = false;
    bool     deferredClear_   = false;
    uint64_t clearTicket_     = 0;
    int64_t  nextActorId_     = 673493175LL;

    FakePlayerManager() = default;

    int64_t nextActorId();
    void commitDeferredClear(uint64_t ticket);
    void sendAddPackets(std::vector<PendingFakeEntry> const& entries, char const* phase);
    void sendRemovePackets(std::vector<mce::UUID> const& uuids, char const* phase);

public:
    static FakePlayerManager& getInstance();

    FakePlayerManager(FakePlayerManager const&)            = delete;
    FakePlayerManager& operator=(FakePlayerManager const&) = delete;

    void beginFakeList();
    void endFakeList();
    void flushFakeList();

    void pushFakeList(std::string const& name, std::string const& xuid = "");
    void clearFakeList();
};


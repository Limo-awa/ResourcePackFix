#include "FakePlayerManager.h"
#include "PlayerListDiagnostic.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <ll/api/thread/ServerThreadExecutor.h>
#include <mc/network/MinecraftPackets.h>
#include <mc/network/packet/PlayerListPacket.h>

namespace {
constexpr size_t kBatchSize = 160;
}

FakePlayerManager& FakePlayerManager::getInstance() {
    static FakePlayerManager instance;
    return instance;
}

void FakePlayerManager::commitDeferredClear(uint64_t ticket) {
    std::vector<mce::UUID> toRemove;
    std::vector<PendingFakeEntry> toAdd;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!deferredClear_ || ticket != clearTicket_ || bulkMode_) {
            return;
        }

        toRemove.reserve(liveEntries_.size());
        for (auto const& [xuid, entry] : liveEntries_) {
            (void)xuid;
            toRemove.push_back(entry.uuid);
        }

        liveEntries_.clear();
        toAdd = std::move(pendingAdds_);
        pendingAdds_.clear();
        for (auto const& entry : toAdd) {
            if (!entry.xuid.empty()) liveEntries_[entry.xuid] = entry;
        }
        deferredClear_ = false;
        syncTransaction_ = false;
        stagedNames_.clear();
    }

    sendRemovePackets(toRemove, "clearRemove");
    sendAddPackets(toAdd, "clearAdd");
}

void FakePlayerManager::beginFakeList() {
    std::lock_guard<std::mutex> lock(mutex_);
    bulkMode_ = true;
    pendingAdds_.clear();
    stagedNames_.clear();
    syncTransaction_ = deferredClear_;
    if (syncTransaction_) {
        stagedNames_.reserve(liveEntries_.size());
    } else if (!liveEntries_.empty()) {
        pendingAdds_.reserve(liveEntries_.size());
    }
}

void FakePlayerManager::endFakeList() {
    std::vector<mce::UUID> toRemove;
    std::vector<PendingFakeEntry> toAdd;
    bool shouldProcess = false;
    bool wasSync = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!bulkMode_) {
            return;
        }

        bulkMode_ = false;

        if (syncTransaction_) {
            wasSync = true;
            shouldProcess = true;

            toRemove.reserve(liveEntries_.size());
            for (auto it = liveEntries_.begin(); it != liveEntries_.end();) {
                auto staged = stagedNames_.find(it->first);
                if (staged == stagedNames_.end() || staged->second != it->second.name) {
                    toRemove.push_back(it->second.uuid);
                    it = liveEntries_.erase(it);
                } else {
                    ++it;
                }
            }

            toAdd.reserve(stagedNames_.size());
            for (auto const& [xuid, name] : stagedNames_) {
                if (liveEntries_.find(xuid) != liveEntries_.end()) {
                    continue;
                }
                PendingFakeEntry entry{name, xuid, mce::UUID::random(), nextActorId()};
                liveEntries_.emplace(xuid, entry);
                toAdd.push_back(std::move(entry));
            }

            stagedNames_.clear();
            pendingAdds_.clear();
            deferredClear_ = false;
            syncTransaction_ = false;
        } else {
            shouldProcess = true;
            toAdd = std::move(pendingAdds_);
            pendingAdds_.clear();

            for (auto const& e : toAdd) {
                if (!e.xuid.empty()) {
                    auto previous = liveEntries_.find(e.xuid);
                    if (previous != liveEntries_.end()) toRemove.push_back(previous->second.uuid);
                    liveEntries_[e.xuid] = e;
                }
            }
        }
    }

    if (!shouldProcess) return;

    if (!toRemove.empty()) {
        sendRemovePackets(toRemove, "syncRemove");
    }
    if (!toAdd.empty()) {
        sendAddPackets(toAdd, wasSync ? "syncAdd" : "flushAdd");
    }
}

void FakePlayerManager::flushFakeList() {
    bool hasDeferredClear = false;
    uint64_t ticket = 0;
    std::vector<PendingFakeEntry> toAdd;
    std::vector<mce::UUID> toRemove;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (bulkMode_) {
            return;
        }

        hasDeferredClear = deferredClear_;
        ticket = clearTicket_;

        if (!hasDeferredClear && !pendingAdds_.empty()) {
            toAdd = std::move(pendingAdds_);
            pendingAdds_.clear();
            for (auto const& e : toAdd) {
                if (!e.xuid.empty()) {
                    auto previous = liveEntries_.find(e.xuid);
                    if (previous != liveEntries_.end()) toRemove.push_back(previous->second.uuid);
                    liveEntries_[e.xuid] = e;
                }
            }
        }
    }

    if (hasDeferredClear) {
        commitDeferredClear(ticket);
    } else if (!toAdd.empty()) {
        sendRemovePackets(toRemove, "flushReplaceRemove");
        sendAddPackets(toAdd, "flushAdd");
    }
}


void FakePlayerManager::pushFakeList(std::string const& name, std::string const& xuid) {
    std::vector<PendingFakeEntry> toSend;
    toSend.reserve(1);
    std::vector<mce::UUID> toRemove;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (bulkMode_) {
            if (syncTransaction_) {
                stagedNames_[xuid] = name;
            } else {
                pendingAdds_.push_back(PendingFakeEntry{name, xuid, mce::UUID::random(), nextActorId()});
            }
            return;
        }

        PendingFakeEntry entry{name, xuid, mce::UUID::random(), nextActorId()};
        if (deferredClear_) {
            pendingAdds_.push_back(std::move(entry));
            return;
        }
        toSend.push_back(std::move(entry));
        if (!xuid.empty()) {
            auto previous = liveEntries_.find(xuid);
            if (previous != liveEntries_.end()) toRemove.push_back(previous->second.uuid);
            liveEntries_[xuid] = toSend.back();
        }
    }

    sendRemovePackets(toRemove, "pushReplaceRemove");
    sendAddPackets(toSend, "pushSingleAdd");
}

void FakePlayerManager::clearFakeList() {
    uint64_t ticket = 0;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingAdds_.clear();
        stagedNames_.clear();
        deferredClear_ = true;
        syncTransaction_ = false;
        ticket = ++clearTicket_;
    }

    ll::thread::ServerThreadExecutor::getDefault().execute([this, ticket] {
        this->commitDeferredClear(ticket);
    });
}





int64_t FakePlayerManager::nextActorId() {
    if (nextActorId_ == std::numeric_limits<int64_t>::max()) nextActorId_ = 673493175LL;
    return nextActorId_++;
}

void FakePlayerManager::sendAddPackets(std::vector<PendingFakeEntry> const& entries, char const* phase) {
    for (size_t i = 0; i < entries.size();) {
        auto packet = MinecraftPackets::createPacket(MinecraftPacketIds::PlayerList);
        if (!packet) return;
        auto& list = static_cast<PlayerListPacket&>(*packet);
        ResourcePackFix::PlayerListDiagnostic::log(list, "factory-add");
        list.mSerializationMode = SerializationMode::CerealOnly;
        const size_t end = std::min(i + kBatchSize, entries.size());
        auto& payload = list.mEntries.get();
        payload.reserve(end - i);
        for (; i < end; ++i) {
            auto const& item = entries[i];
            PlayerListPacketPayload::AddEntry entry;
            entry.mUUID = item.uuid;
            entry.mId = ActorUniqueID(item.actorId);
            entry.mName = item.name;
            entry.mXUID = std::string{};
            entry.mPlatformOnlineId = std::string{};
            entry.mBuildPlatform = BuildPlatform::Win32;
            entry.mSkin = SerializedSkinRef::createTrustedDefaultSerializedSkin();
            entry.mColor = mce::Color(1.0f, 1.0f, 1.0f, 1.0f);
            payload.emplace_back(entry);
        }
        ResourcePackFix::PlayerListDiagnostic::log(list, phase);
        packet->sendToClients();
    }
}

void FakePlayerManager::sendRemovePackets(std::vector<mce::UUID> const& uuids, char const* phase) {
    for (size_t i = 0; i < uuids.size();) {
        auto packet = MinecraftPackets::createPacket(MinecraftPacketIds::PlayerList);
        if (!packet) return;
        auto& list = static_cast<PlayerListPacket&>(*packet);
        ResourcePackFix::PlayerListDiagnostic::log(list, "factory-remove");
        list.mSerializationMode = SerializationMode::CerealOnly;
        const size_t end = std::min(i + kBatchSize, uuids.size());
        auto& payload = list.mEntries.get();
        payload.reserve(end - i);
        for (; i < end; ++i) {
            PlayerListPacketPayload::RemoveEntry entry;
            entry.mAction = PlayerListPacketType::Remove;
            entry.mUUID = uuids[i];
            payload.emplace_back(entry);
        }
        ResourcePackFix::PlayerListDiagnostic::log(list, phase);
        packet->sendToClients();
    }
}


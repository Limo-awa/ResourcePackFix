/**
 * @file Entry.cpp
 * @brief ResourcePackFix 插件入口实现
 * @note 重建自反编译代码
 */

#define LL_MEMORY_OPERATORS
#include "ll/api/memory/MemoryOperators.h"

#include "Entry.h"
#include "FixHook.h"
#include "FakePlayerManager.h"
#include "PlayerListDiagnostic.h"


#include "ll/api/mod/NativeMod.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/io/Logger.h"
#include "ll/api/io/LoggerRegistry.h"
#include "SimpleRemoteCall.h"

namespace ResourcePackFix {

// 获取日志记录器
#ifdef ENABLE_LOGGING
static std::shared_ptr<ll::io::Logger> getLogger() {
    static auto logger = ll::io::LoggerRegistry::getInstance().getOrCreate("ResourcePackFix");
    return logger;
}
#endif

Entry& Entry::getInstance() {
    static Entry instance;
    return instance;
}

bool Entry::load() {
#ifdef ENABLE_LOGGING
    auto logger = getLogger();
    logger->info("ResourcePackFix 正在加载...");
#endif
    
    // 获取模组目录
    auto mod = ll::mod::NativeMod::current();
    if (!mod) {
#ifdef ENABLE_LOGGING
        logger->error("无法获取模组实例!");
#endif
        return false;
    }
    
    // 设置缓存路径
    cachePath_ = mod->getModDir() / "cache";
    
    // 确保缓存目录存在
    std::error_code ec;
    std::filesystem::create_directories(cachePath_, ec);
    if (ec) {
#ifdef ENABLE_LOGGING
        logger->warn("创建缓存目录时出现问题: {}", ec.message());
#endif
    }
    
#ifdef ENABLE_LOGGING
    logger->info("缓存目录: {}", cachePath_.string());
#endif
    
    // 安装 Hook
    if (!FixHook::install()) {
#ifdef ENABLE_LOGGING
        logger->error("安装 Hook 失败!");
#endif
        return false;
    }
    
    PlayerListDiagnostic::install();

    // 注册远程调用 API
    RemoteCall::exportAs("ResourcePackFix", "pushFakeList", [](std::string const& name, std::string const& xuid) {
        FakePlayerManager::getInstance().pushFakeList(name, xuid);
        return true;
    });
    
    RemoteCall::exportAs("ResourcePackFix", "beginFakeList", []() {
        FakePlayerManager::getInstance().beginFakeList();
        return true;
    });

    RemoteCall::exportAs("ResourcePackFix", "endFakeList", []() {
        FakePlayerManager::getInstance().endFakeList();
        return true;
    });

    RemoteCall::exportAs("ResourcePackFix", "flushFakeList", []() {
        FakePlayerManager::getInstance().flushFakeList();
        return true;
    });
    RemoteCall::exportAs("ResourcePackFix", "clearFakeList", []() {
        FakePlayerManager::getInstance().clearFakeList();
        return true;
    });
    
#ifdef ENABLE_LOGGING
    logger->info("ResourcePackFix 加载成功!");
#endif
    initialized_ = true;
    return true;
}

bool Entry::enable() {
#ifdef ENABLE_LOGGING
    getLogger()->info("ResourcePackFix 已启用");
#endif
    return true;
}

bool Entry::disable() {
#ifdef ENABLE_LOGGING
    getLogger()->info("ResourcePackFix 已禁用");
#endif
    return true;
}

bool Entry::unload() {
#ifdef ENABLE_LOGGING
    auto logger = getLogger();
    logger->info("ResourcePackFix 正在卸载...");
#endif
    
    PlayerListDiagnostic::uninstall();
    FixHook::uninstall();
    
    // 移除远程调用
    RemoteCall::removeNameSpace("ResourcePackFix");
    
    initialized_ = false;
    
#ifdef ENABLE_LOGGING
    logger->info("ResourcePackFix 已卸载");
#endif
    return true;
}

} // namespace ResourcePackFix

LL_REGISTER_MOD(
    ResourcePackFix::Entry,
    ResourcePackFix::Entry::getInstance()
);

#include "FixHook.h"
#include "Entry.h"

#include "ll/api/memory/Hook.h"
#include "mc/util/ResourcePackFileUploadManager.h"

namespace ResourcePackFix {

LL_TYPE_INSTANCE_HOOK(
    ResourcePackZipPathHook,
    ll::memory::HookPriority::Normal,
    ResourcePackFileUploadManager,
    &ResourcePackFileUploadManager::_addZipPath,
    void,
    Core::PathBuffer<std::string> const& resourceZipPath
) {
    auto cachePath = Entry::getInstance().getCachePath();
    std::error_code ec;
    if (!std::filesystem::exists(cachePath, ec)) {
        std::filesystem::create_directories(cachePath, ec);
    }
    if (std::filesystem::exists(cachePath, ec)) {
        cachePath = std::filesystem::canonical(cachePath, ec);
    }
    std::filesystem::path originalPath(resourceZipPath.value);
    auto newZipPath = cachePath / originalPath.filename();
    if (std::filesystem::exists(originalPath, ec) && !std::filesystem::exists(newZipPath, ec)) {
        std::filesystem::copy_file(originalPath, newZipPath, std::filesystem::copy_options::overwrite_existing, ec);
    }
    Core::PathBuffer<std::string> newPath;
    newPath.value = newZipPath.string();
    origin(newPath);
}

bool FixHook::install() {
    try {
        return ResourcePackZipPathHook::hook() == 0;
    } catch (std::exception const&) {
        return false;
    }
}

bool FixHook::uninstall() {
    return ResourcePackZipPathHook::unhook() == 0;
}

} // namespace ResourcePackFix

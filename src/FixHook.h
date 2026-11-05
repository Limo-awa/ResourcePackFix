#pragma once

namespace ResourcePackFix {

/**
 * @brief 将资源包 ZIP 路径重定向到模组缓存目录
 */
class FixHook {
public:
    static bool install();
    static bool uninstall();
};

} // namespace ResourcePackFix

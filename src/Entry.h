#pragma once

#include <memory>
#include <string>
#include <filesystem>

namespace ResourcePackFix {

/**
 * @brief 插件入口类 - 单例模式
 * 
 * 管理插件的生命周期：加载、启用、禁用、卸载
 */
class Entry {
public:
    /**
     * @brief 获取单例实例
     */
    static Entry& getInstance();
    
    // 禁止拷贝和移动
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;

    /**
     * @brief 加载插件
     * @return 是否成功加载
     */
    bool load();
    
    /**
     * @brief 启用插件
     * @return 是否成功启用
     */
    bool enable();
    
    /**
     * @brief 禁用插件
     * @return 是否成功禁用
     */
    bool disable();
    
    /**
     * @brief 卸载插件
     * @return 是否成功卸载
     */
    bool unload();

    /**
     * @brief 获取缓存目录路径
     * @return 缓存目录的绝对路径
     */
    const std::filesystem::path& getCachePath() const { return cachePath_; }

private:
    Entry() = default;
    ~Entry() = default;

    std::filesystem::path cachePath_;  ///< 资源包缓存路径
    bool initialized_ = false;         ///< 是否已初始化
};

} // namespace ResourcePackFix

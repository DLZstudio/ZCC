#ifndef ZCC_CACHEMANAGER_HPP
#define ZCC_CACHEMANAGER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <functional>

namespace zcc {

// 缓存条目元数据
struct CacheEntryMetadata {
    std::string sourceHash;
    std::string compilerVersion;
    std::string targetTriple;
    std::string backend;
    int optimizationLevel;
    bool debugInfo;
    uint64_t timestamp;
    uint64_t accessTime;
    size_t dataSize;
    
    std::string serialize() const {
        std::ostringstream ss;
        ss << sourceHash << "|"
           << compilerVersion << "|"
           << targetTriple << "|"
           << backend << "|"
           << optimizationLevel << "|"
           << (debugInfo ? "1" : "0") << "|"
           << timestamp << "|"
           << accessTime << "|"
           << dataSize;
        return ss.str();
    }
    
    static std::optional<CacheEntryMetadata> deserialize(const std::string& data) {
        std::istringstream ss(data);
        std::string token;
        CacheEntryMetadata meta;
        
        std::getline(ss, token, '|'); meta.sourceHash = token;
        std::getline(ss, token, '|'); meta.compilerVersion = token;
        std::getline(ss, token, '|'); meta.targetTriple = token;
        std::getline(ss, token, '|'); meta.backend = token;
        std::getline(ss, token, '|'); meta.optimizationLevel = std::stoi(token);
        std::getline(ss, token, '|'); meta.debugInfo = (token == "1");
        std::getline(ss, token, '|'); meta.timestamp = std::stoull(token);
        std::getline(ss, token, '|'); meta.accessTime = std::stoull(token);
        std::getline(ss, token, '|'); meta.dataSize = std::stoull(token);
        
        return meta;
    }
};

// 缓存结果
struct CacheResult {
    bool hit;
    std::vector<uint8_t> data;
    std::string outputPath;
    
    static CacheResult miss() {
        return CacheResult{false, {}, ""};
    }
    
    static CacheResult hit(const std::vector<uint8_t>& data, const std::string& path) {
        return CacheResult{true, data, path};
    }
};

// 哈希计算
class HashCalculator {
public:
    // 计算字符串哈希（简化版，实际应使用更好的哈希算法）
    static std::string hashString(const std::string& input) {
        uint64_t hash = 0xcbf29ce484222325ULL; // FNV-1a 基础
        for (char c : input) {
            hash ^= static_cast<uint64_t>(c);
            hash *= 0x100000001b3ULL;
        }
        
        std::ostringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(16) << hash;
        return ss.str();
    }
    
    // 计算文件哈希
    static std::string hashFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return "";
        
        std::ostringstream ss;
        char buffer[4096];
        while (file.read(buffer, sizeof(buffer))) {
            for (size_t i = 0; i < file.gcount(); ++i) {
                uint64_t hash = 0xcbf29ce484222325ULL;
                hash ^= static_cast<uint64_t>(buffer[i]);
                hash *= 0x100000001b3ULL;
                ss << std::hex << hash;
            }
        }
        
        return hashString(ss.str());
    }
    
    // 计算多个字符串的组合哈希
    static std::string hashCombine(const std::vector<std::string>& inputs) {
        std::string combined;
        for (const auto& input : inputs) {
            combined += input;
        }
        return hashString(combined);
    }
};

// 缓存管理器
class CacheManager {
public:
    static CacheManager& getInstance() {
        static CacheManager instance;
        return instance;
    }
    
    // 设置缓存根目录
    void setCacheRoot(const std::filesystem::path& root) {
        cacheRoot_ = root;
        initializeCacheDirectories();
    }
    
    // 获取缓存根目录
    const std::filesystem::path& getCacheRoot() const {
        return cacheRoot_;
    }
    
    // 检查缓存
    CacheResult check(const std::string& sourcePath,
                     const std::string& backend,
                     const std::string& targetTriple,
                     int optimizationLevel,
                     bool debugInfo) {
        
        // 计算源文件哈希
        std::string sourceHash = HashCalculator::hashFile(sourcePath);
        
        // 构建缓存键
        std::string cacheKey = HashCalculator::hashCombine({
            sourceHash,
            ZCC_VERSION,
            backend,
            targetTriple,
            std::to_string(optimizationLevel),
            debugInfo ? "1" : "0"
        });
        
        // 获取缓存路径
        std::filesystem::path cachePath = getCachePath(cacheKey, backend);
        
        // 检查缓存是否存在
        if (!std::filesystem::exists(cachePath)) {
            return CacheResult::miss();
        }
        
        // 读取元数据
        std::filesystem::path metaPath = cachePath.parent_path() / "metadata";
        if (!std::filesystem::exists(metaPath)) {
            return CacheResult::miss();
        }
        
        std::ifstream metaFile(metaPath);
        std::string metaContent((std::istreambuf_iterator<char>(metaFile)),
                                std::istreambuf_iterator<char>());
        
        auto meta = CacheEntryMetadata::deserialize(metaContent);
        if (!meta || meta->sourceHash != sourceHash) {
            return CacheResult::miss();
        }
        
        // 更新访问时间
        updateAccessTime(cacheKey, backend);
        
        // 读取缓存数据
        std::ifstream dataFile(cachePath, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(dataFile)),
                                   std::istreambuf_iterator<char>());
        
        return CacheResult::hit(data, cachePath.string());
    }
    
    // 保存到缓存
    bool save(const std::string& sourcePath,
              const std::string& backend,
              const std::string& targetTriple,
              int optimizationLevel,
              bool debugInfo,
              const std::vector<uint8_t>& data) {
        
        // 计算源文件哈希
        std::string sourceHash = HashCalculator::hashFile(sourcePath);
        
        // 构建缓存键
        std::string cacheKey = HashCalculator::hashCombine({
            sourceHash,
            ZCC_VERSION,
            backend,
            targetTriple,
            std::to_string(optimizationLevel),
            debugInfo ? "1" : "0"
        });
        
        // 获取缓存路径
        std::filesystem::path cachePath = getCachePath(cacheKey, backend);
        
        // 创建目录
        std::filesystem::create_directories(cachePath.parent_path());
        
        // 写入数据
        std::ofstream dataFile(cachePath, std::ios::binary);
        if (!dataFile) return false;
        dataFile.write(reinterpret_cast<const char*>(data.data()), data.size());
        
        // 写入元数据
        CacheEntryMetadata meta;
        meta.sourceHash = sourceHash;
        meta.compilerVersion = ZCC_VERSION;
        meta.targetTriple = targetTriple;
        meta.backend = backend;
        meta.optimizationLevel = optimizationLevel;
        meta.debugInfo = debugInfo;
        meta.timestamp = getCurrentTimestamp();
        meta.accessTime = meta.timestamp;
        meta.dataSize = data.size();
        
        std::filesystem::path metaPath = cachePath.parent_path() / "metadata";
        std::ofstream metaFile(metaPath);
        if (!metaFile) return false;
        metaFile << meta.serialize();
        
        return true;
    }
    
    // 清理过期缓存
    void cleanup(uint64_t maxAgeDays = 30, size_t maxSizeMB = 1000) {
        uint64_t maxAge = maxAgeDays * 24 * 60 * 60; // 转换为秒
        uint64_t currentTime = getCurrentTimestamp();
        size_t totalSize = 0;
        size_t maxSize = maxSizeMB * 1024 * 1024;
        
        std::vector<std::filesystem::path> toDelete;
        
        // 遍历缓存目录
        if (!std::filesystem::exists(cacheRoot_)) return;
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(cacheRoot_)) {
            if (entry.is_regular_file()) {
                if (entry.path().filename() == "metadata") {
                    // 读取元数据检查时间
                    std::ifstream file(entry.path());
                    std::string content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                    
                    auto meta = CacheEntryMetadata::deserialize(content);
                    if (meta && (currentTime - meta->accessTime > maxAge)) {
                        toDelete.push_back(entry.path().parent_path());
                    }
                }
            }
        }
        
        // 删除过期缓存
        for (const auto& path : toDelete) {
            std::filesystem::remove_all(path);
        }
        
        // 检查大小限制
        totalSize = getCacheSize();
        if (totalSize > maxSize) {
            // TODO: 实现 LRU 淘汰策略
        }
    }
    
    // 清空缓存
    void clear() {
        if (std::filesystem::exists(cacheRoot_)) {
            std::filesystem::remove_all(cacheRoot_);
        }
        initializeCacheDirectories();
    }
    
    // 获取缓存统计
    struct CacheStats {
        size_t totalEntries;
        size_t totalSize;
        size_t hitCount;
        size_t missCount;
        
        double hitRate() const {
            size_t total = hitCount + missCount;
            return total > 0 ? (double)hitCount / total : 0.0;
        }
    };
    
    CacheStats getStats() {
        CacheStats stats = {0, 0, hitCount_, missCount_};
        
        if (std::filesystem::exists(cacheRoot_)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(cacheRoot_)) {
                if (entry.is_regular_file() && entry.path().filename() == "metadata") {
                    stats.totalEntries++;
                    stats.totalSize += entry.file_size();
                }
            }
        }
        
        return stats;
    }
    
    // 记录命中/未命中
    void recordHit() { hitCount_++; }
    void recordMiss() { missCount_++; }
    
private:
    CacheManager() {
        // 默认缓存目录
        #ifdef _WIN32
        cacheRoot_ = std::filesystem::path(getenv("LOCALAPPDATA")) / "ZCC" / "cache";
        #else
        cacheRoot_ = std::filesystem::path(getenv("HOME")) / ".zcc" / "cache";
        #endif
        
        initializeCacheDirectories();
    }
    
    void initializeCacheDirectories() {
        std::filesystem::create_directories(cacheRoot_ / "blobs");
        std::filesystem::create_directories(cacheRoot_ / "locks");
        std::filesystem::create_directories(cacheRoot_ / "metadata");
    }
    
    std::filesystem::path getCachePath(const std::string& key, const std::string& backend) {
        // 使用两级目录结构避免单目录文件过多
        std::filesystem::path path = cacheRoot_ / "blobs" / backend;
        if (key.size() >= 2) {
            path /= key.substr(0, 2);
        }
        if (key.size() >= 4) {
            path /= key.substr(2, 2);
        }
        path /= key;
        return path;
    }
    
    uint64_t getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
    
    void updateAccessTime(const std::string& key, const std::string& backend) {
        std::filesystem::path metaPath = getCachePath(key, backend).parent_path() / "metadata";
        if (!std::filesystem::exists(metaPath)) return;
        
        std::ifstream file(metaPath);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        auto meta = CacheEntryMetadata::deserialize(content);
        if (!meta) return;
        
        meta->accessTime = getCurrentTimestamp();
        
        std::ofstream outFile(metaPath);
        outFile << meta->serialize();
    }
    
    size_t getCacheSize() {
        size_t total = 0;
        if (std::filesystem::exists(cacheRoot_)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(cacheRoot_)) {
                if (entry.is_regular_file()) {
                    total += entry.file_size();
                }
            }
        }
        return total;
    }
    
    std::filesystem::path cacheRoot_;
    size_t hitCount_ = 0;
    size_t missCount_ = 0;
};

// 便捷函数
inline CacheManager& getCacheManager() {
    return CacheManager::getInstance();
}

} // namespace zcc

#endif // ZCC_CACHEMANAGER_HPP

#ifndef ZCC_PLATFORM_HPP
#define ZCC_PLATFORM_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <variant>
#include <memory>

namespace zcc {

// ==================== 平台抽象 ====================

// 目标架构
enum class TargetArch {
    X86,
    X86_64,
    ARM,
    ARM64,
    WASM32,
    WASM64,
    RISC_V,
    MIPS,
    I386,      // BIOS
    MC,        // Minecraft
    MICROPYTHON, // 微控制器/MicroPython
    CUSTOM,    // 自定义架构
};

// 目标供应商
enum class TargetVendor {
    PC,
    APPLE,
    LINUX,
    ANDROID,
    MICROSOFT,
    MINECRAFT,
    EMBEDDED,
    CUSTOM,
};

// 目标操作系统
enum class TargetOS {
    WINDOWS,
    LINUX,
    MACOS,
    ANDROID,
    IOS,
    WASM,
    NONE,      // 无操作系统 (BIOS/嵌入式)
    MINECRAFT,
    CUSTOM,
};

// 目标 ABI
enum class TargetABI {
    DEFAULT,
    GNU,
    MSVC,
    ANDROID,
    WASM,
    MC,
    CUSTOM,
};

// 目标平台定义
struct TargetPlatform {
    TargetArch arch;
    TargetVendor vendor;
    TargetOS os;
    TargetABI abi;
    
    std::string toString() const {
        return getArchString() + "-" + getVendorString() + "-" + getOSString() + "-" + getABIString();
    }
    
    bool isWindows() const { return os == TargetOS::WINDOWS; }
    bool isLinux() const { return os == TargetOS::LINUX; }
    bool isMacOS() const { return os == TargetOS::MACOS; }
    bool isAndroid() const { return os == TargetOS::ANDROID; }
    bool isWASM() const { return os == TargetOS::WASM; }
    bool isBIOS() const { return arch == TargetArch::I386 && os == TargetOS::NONE; }
    bool isMC() const { return os == TargetOS::MINECRAFT; }
    bool isMicroPython() const { return arch == TargetArch::MICROPYTHON; }
    
    static TargetPlatform getCurrent();
    static TargetPlatform parse(const std::string& triple);
    
private:
    std::string getArchString() const;
    std::string getVendorString() const;
    std::string getOSString() const;
    std::string getABIString() const;
};

// ==================== 平台抽象接口 ====================

// 文件系统接口
class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    
    // 读取文件
    virtual std::optional<std::vector<uint8_t>> readFile(const std::string& path) = 0;
    
    // 写入文件
    virtual bool writeFile(const std::string& path, const std::vector<uint8_t>& data) = 0;
    
    // 读取文本文件
    virtual std::optional<std::string> readTextFile(const std::string& path) = 0;
    
    // 写入文本文件
    virtual bool writeTextFile(const std::string& path, const std::string& content) = 0;
    
    // 检查文件是否存在
    virtual bool exists(const std::string& path) = 0;
    
    // 获取文件大小
    virtual size_t fileSize(const std::string& path) = 0;
    
    // 获取当前目录
    virtual std::string getCurrentDir() = 0;
    
    // 设置当前目录
    virtual bool setCurrentDir(const std::string& path) = 0;
};

// 控制台接口
class IConsole {
public:
    virtual ~IConsole() = default;
    
    // 写入文本
    virtual void write(const std::string& text) = 0;
    
    // 写入行
    virtual void writeln(const std::string& text) = 0;
    
    // 读取一行
    virtual std::string readLine() = 0;
    
    // 读取一行（带提示）
    virtual std::string readLine(const std::string& prompt);
};

// 内存管理接口
class IMemoryManager {
public:
    virtual ~IMemoryManager() = default;
    
    // 分配内存
    virtual void* allocate(size_t size) = 0;
    
    // 释放内存
    virtual void free(void* ptr) = 0;
    
    // 重新分配
    virtual void* reallocate(void* ptr, size_t newSize) = 0;
};

// 时间接口
class ITime {
public:
    virtual ~ITime() = default;
    
    // 获取当前时间（毫秒）
    virtual uint64_t getCurrentTimeMs() = 0;
    
    // 睡眠
    virtual void sleep(uint64_t ms) = 0;
};

// ==================== 平台实现 ====================

// 标准平台实现（PC/服务器）
class StandardPlatform : public IFileSystem, public IConsole, public IMemoryManager, public ITime {
public:
    // IFileSystem
    std::optional<std::vector<uint8_t>> readFile(const std::string& path) override;
    bool writeFile(const std::string& path, const std::vector<uint8_t>& data) override;
    std::optional<std::string> readTextFile(const std::string& path) override;
    bool writeTextFile(const std::string& path, const std::string& content) override;
    bool exists(const std::string& path) override;
    size_t fileSize(const std::string& path) override;
    std::string getCurrentDir() override;
    bool setCurrentDir(const std::string& path) override;
    
    // IConsole
    void write(const std::string& text) override;
    void writeln(const std::string& text) override;
    std::string readLine() override;
    
    // IMemoryManager
    void* allocate(size_t size) override;
    void free(void* ptr) override;
    void* reallocate(void* ptr, size_t newSize) override;
    
    // ITime
    uint64_t getCurrentTimeMs() override;
    void sleep(uint64_t ms) override;
};

// Minecraft 平台实现
class MCPlatform : public IFileSystem, public IConsole, public IMemoryManager, public ITime {
public:
    // IFileSystem - MC 使用特定的资源系统
    std::optional<std::vector<uint8_t>> readFile(const std::string& path) override;
    bool writeFile(const std::string& path, const std::vector<uint8_t>& data) override;
    std::optional<std::string> readTextFile(const std::string& path) override;
    bool writeTextFile(const std::string& path, const std::string& content) override;
    bool exists(const std::string& path) override;
    size_t fileSize(const std::string& path) override;
    std::string getCurrentDir() override;
    bool setCurrentDir(const std::string& path) override;
    
    // IConsole - MC 使用聊天系统或日志
    void write(const std::string& text) override;
    void writeln(const std::string& text) override;
    std::string readLine() override;
    
    // IMemoryManager - 使用 Java 堆或 MC 内存池
    void* allocate(size_t size) override;
    void free(void* ptr) override;
    void* reallocate(void* ptr, size_t newSize) override;
    
    // ITime - MC 使用游戏刻
    uint64_t getCurrentTimeMs() override;
    void sleep(uint64_t ms) override;
    
private:
    // MC 特定方法
    void sendChatMessage(const std::string& text);
    void logMessage(const std::string& text);
};

// MicroPython/微控制器平台实现
class MicroPythonPlatform : public IFileSystem, public IConsole, public IMemoryManager, public ITime {
public:
    // 简化的文件系统（可能只有 Flash）
    std::optional<std::vector<uint8_t>> readFile(const std::string& path) override;
    bool writeFile(const std::string& path, const std::vector<uint8_t>& data) override;
    std::optional<std::string> readTextFile(const std::string& path) override;
    bool writeTextFile(const std::string& path, const std::string& content) override;
    bool exists(const std::string& path) override;
    size_t fileSize(const std::string& path) override;
    std::string getCurrentDir() override;
    bool setCurrentDir(const std::string& path) override;
    
    // 简化的控制台（可能只有 UART）
    void write(const std::string& text) override;
    void writeln(const std::string& text) override;
    std::string readLine() override;
    
    // 简化的内存管理（静态分配）
    void* allocate(size_t size) override;
    void free(void* ptr) override;
    void* reallocate(void* ptr, size_t newSize) override;
    
    // 简化的时间
    uint64_t getCurrentTimeMs() override;
    void sleep(uint64_t ms) override;
};

// BIOS 平台实现
class BIOSPlatform : public IFileSystem, public IConsole, public IMemoryManager, public ITime {
public:
    // BIOS 没有文件系统，从磁盘扇区读取
    std::optional<std::vector<uint8_t>> readFile(const std::string& path) override;
    bool writeFile(const std::string& path, const std::vector<uint8_t>& data) override;
    std::optional<std::string> readTextFile(const std::string& path) override;
    bool writeTextFile(const std::string& path, const std::string& content) override;
    bool exists(const std::string& path) override;
    size_t fileSize(const std::string& path) override;
    std::string getCurrentDir() override;
    bool setCurrentDir(const std::string& path) override;
    
    // BIOS 使用中断调用显示
    void write(const std::string& text) override;
    void writeln(const std::string& text) override;
    std::string readLine() override;
    
    // BIOS 使用实模式内存
    void* allocate(size_t size) override;
    void free(void* ptr) override;
    void* reallocate(void* ptr, size_t newSize) override;
    
    // BIOS 使用 RTC
    uint64_t getCurrentTimeMs() override;
    void sleep(uint64_t ms) override;
    
private:
    void biosInterrupt(uint8_t intNum, uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx);
    std::vector<uint8_t> readDiskSector(int sector);
};

// ==================== 平台管理器 ====================

class PlatformManager {
public:
    static PlatformManager& getInstance();
    
    // 获取当前平台
    IFileSystem& getFileSystem();
    IConsole& getConsole();
    IMemoryManager& getMemoryManager();
    ITime& getTime();
    
    // 设置平台实现
    void setPlatform(std::unique_ptr<IFileSystem> fs,
                    std::unique_ptr<IConsole> console,
                    std::unique_ptr<IMemoryManager> mem,
                    std::unique_ptr<ITime> time);
    
    // 初始化平台
    void initialize(TargetPlatform target);
    
private:
    PlatformManager() = default;
    
    std::unique_ptr<IFileSystem> fileSystem_;
    std::unique_ptr<IConsole> console_;
    std::unique_ptr<IMemoryManager> memoryManager_;
    std::unique_ptr<ITime> time_;
    TargetPlatform currentPlatform_;
};

// ==================== 便捷函数 ====================

// 平台无关的文件操作
inline std::optional<std::vector<uint8_t>> readFile(const std::string& path) {
    return PlatformManager::getInstance().getFileSystem().readFile(path);
}

inline bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    return PlatformManager::getInstance().getFileSystem().writeFile(path, data);
}

inline std::optional<std::string> readTextFile(const std::string& path) {
    return PlatformManager::getInstance().getFileSystem().readTextFile(path);
}

inline bool writeTextFile(const std::string& path, const std::string& content) {
    return PlatformManager::getInstance().getFileSystem().writeTextFile(path, content);
}

// 平台无关的控制台操作
inline void print(const std::string& text) {
    PlatformManager::getInstance().getConsole().write(text);
}

inline void println(const std::string& text) {
    PlatformManager::getInstance().getConsole().writeln(text);
}

inline std::string readLine() {
    return PlatformManager::getInstance().getConsole().readLine();
}

} // namespace zcc

#endif // ZCC_PLATFORM_HPP

#ifndef ZCC_BACKEND_HPP
#define ZCC_BACKEND_HPP

#include <string>
#include <vector>
#include <memory>
#include "ZCCIR.hpp"
#include "Platform.hpp"

namespace zcc {

// 编译选项
enum class OptimizationLevel {
    O0,  // 无优化
    O1,  // 基础优化
    O2,  // 中级优化
    O3,  // 高级优化
    Os,  // 大小优化
    Oz,  // 极致大小优化
};

struct CompileOptions {
    OptimizationLevel optimization = OptimizationLevel::O0;
    bool debugInfo = false;
    bool emitAssembly = false;
    std::vector<std::string> includePaths;
    std::vector<std::string> defines;
    std::string outputPath;
    std::string targetTriple;
};

// 编译结果
struct CompileResult {
    bool success;
    std::string output;
    std::vector<uint8_t> binary;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::string errorMessage;
    
    bool hasErrors() const { return !errors.empty(); }
    bool hasWarnings() const { return !warnings.empty(); }
};

// ==================== 后端接口 ====================

// 后端基类
class Backend {
public:
    virtual ~Backend() = default;
    
    // 初始化后端
    virtual bool initialize() = 0;
    
    // 编译模块
    virtual CompileResult compile(const ZCCIRModule& module,
                                  const CompileOptions& options) = 0;
    
    // 获取后端名称
    virtual std::string getName() const = 0;
    
    // 检查是否支持当前平台
    virtual bool isSupported() const = 0;
    
    // 获取支持的优化级别
    virtual std::vector<OptimizationLevel> getSupportedOptimizations() const;
    
    // 获取文件扩展名
    virtual std::string getFileExtension() const = 0;
    
    // 获取后端描述
    virtual std::string getDescription() const = 0;
};

// ==================== 后端管理器 ====================

class BackendManager {
public:
    static BackendManager& getInstance();
    
    // 注册后端
    void registerBackend(std::unique_ptr<Backend> backend);
    
    // 获取后端
    Backend* getBackend(const std::string& name);
    
    // 获取所有后端
    std::vector<std::string> getAvailableBackends() const;
    
    // 根据目标平台选择最佳后端
    Backend* selectBackendForTarget(const TargetPlatform& target);
    
    // 编译
    CompileResult compile(const ZCCIRModule& module,
                         const std::string& backendName,
                         const CompileOptions& options);
    
private:
    BackendManager() = default;
    
    std::unordered_map<std::string, std::unique_ptr<Backend>> backends_;
};

// ==================== Windows 后端 (通过 MSVC/GCC) ====================

// Windows 后端 - 生成 MSVC 或 MinGW 可执行文件
class WindowsBackend : public Backend {
public:
    WindowsBackend() = default;
    
    bool initialize() override { return true; }
    
    CompileResult compile(const ZCCIRModule& module,
                         const CompileOptions& options) override;
    
    std::string getName() const override { return "windows"; }
    bool isSupported() const override { return true; }
    std::string getFileExtension() const override { return ".exe"; }
    std::string getDescription() const override { return "Windows x86_64 Executable"; }
    
private:
    // 通过 C 后端生成中间 C 代码，然后调用 MSVC/GCC
    CompileResult compileViaC(const ZCCIRModule& module,
                             const CompileOptions& options);
};

// ==================== Linux 后端 (通过 GCC/Clang) ====================

// Linux 后端 - 生成 ELF 可执行文件
class LinuxBackend : public Backend {
public:
    LinuxBackend() = default;
    
    bool initialize() override { return true; }
    
    CompileResult compile(const ZCCIRModule& module,
                         const CompileOptions& options) override;
    
    std::string getName() const override { return "linux"; }
    bool isSupported() const override { return true; }
    std::string getFileExtension() const override { return ""; }
    std::string getDescription() const override { return "Linux x86_64 ELF Executable"; }
    
private:
    // 通过 C 后端生成中间 C 代码，然后调用 GCC/Clang
    CompileResult compileViaC(const ZCCIRModule& module,
                             const CompileOptions& options);
};

// ==================== BIOS 后端 ====================

// BIOS 后端 - 生成 x86 实模式引导二进制
class BIOSBackend : public Backend {
public:
    BIOSBackend() = default;
    
    bool initialize() override { return true; }
    
    CompileResult compile(const ZCCIRModule& module,
                         const CompileOptions& options) override;
    
    std::string getName() const override { return "bios"; }
    bool isSupported() const override { return true; }
    std::string getFileExtension() const override { return ".bin"; }
    std::string getDescription() const override { return "x86 BIOS Boot Sector (16-bit real mode)"; }
    
private:
    // 生成 x86 实模式汇编
    std::string generateAssembly(const ZCCIRModule& module);
    
    // 生成引导扇区（512 字节，包含引导签名）
    std::vector<uint8_t> generateBootSector(const std::string& asmCode);
    
    // x86 指令编码
    uint8_t encodeMOV_AX_Immediate(uint16_t value);
    uint8_t encodeINT(uint8_t num);
    uint8_t encodeJMP_Short(int8_t offset);
    uint8_t encodeJMP_Far(uint16_t segment, uint16_t offset);
    
    std::vector<uint8_t> output_;
};

// ==================== 第三方后端注册接口 ====================

// 第三方后端注册函数类型
using BackendFactory = std::unique_ptr<Backend>(*)();

// 注册第三方后端
inline void registerThirdPartyBackend(const std::string& name, BackendFactory factory) {
    BackendManager::getInstance().registerBackend(factory());
}

// 第三方后端宏
#define REGISTER_ZCC_BACKEND(BackendClass, name) \
    static std::unique_ptr<Backend> create_##BackendClass() { \
        return std::make_unique<BackendClass>(); \
    } \
    static bool registered_##BackendClass = []() { \
        registerThirdPartyBackend(name, create_##BackendClass); \
        return true; \
    }();

} // namespace zcc

#endif // ZCC_BACKEND_HPP

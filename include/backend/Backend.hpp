#ifndef ZCC_BACKEND_HPP
#define ZCC_BACKEND_HPP

#include <string>
#include <vector>
#include <memory>
#include "ZCCIR.hpp"
#include "Platform.hpp"

namespace zcc {

// 编译选项
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

// ==================== C 后端 ====================

// C 后端实现
class CBackend : public Backend {
public:
    CBackend() = default;
    
    bool initialize() override { return true; }
    
    CompileResult compile(const ZCCIRModule& module,
                         const CompileOptions& options) override;
    
    std::string getName() const override { return "c"; }
    bool isSupported() const override { return true; }
    std::string getFileExtension() const override { return ".c"; }
    
private:
    // 生成 C 代码
    std::string generateCCode(const ZCCIRModule& module);
    
    // 生成函数定义
    std::string generateFunction(const ZCCIRFunction& func);
    
    // 生成基本块
    std::string generateBlock(const ZCCIRBlock& block);
    
    // 生成指令
    std::string generateInstruction(const ZCCIRInstruction& instr);
    
    // 生成表达式
    std::string generateExpression(const ZCCIROperand& operand);
    
    // 生成全局变量
    std::string generateGlobal(const ZCCIRGlobal& global);
    
    // 类型转换
    std::string convertType(const ZCCType& type);
    
    // 转义字符串
    std::string escapeString(const std::string& str);
    
    // 缩进
    std::string indent(int level) const;
    
    std::string output_;
    int indentLevel_ = 0;
};

// ==================== WASM 后端 ====================

// WASM 后端实现
class WASMBackend : public Backend {
public:
    WASMBackend() = default;
    
    bool initialize() override { return true; }
    
    CompileResult compile(const ZCCIRModule& module,
                         const CompileOptions& options) override;
    
    std::string getName() const override { return "wasm"; }
    bool isSupported() const override { return true; }
    std::string getFileExtension() const override { return ".wasm"; }
    
private:
    // 生成 WASM 二进制
    std::vector<uint8_t> generateWasmBinary(const ZCCIRModule& module);
    
    // WASM 类型
    uint8_t encodeValueType(const ZCCType& type);
    
    // WASM 指令
    void emitInstruction(uint8_t opcode);
    
    std::vector<uint8_t> output_;
};

// ==================== MC 后端 ====================

// Minecraft 后端实现
class MCBackend : public Backend {
public:
    MCBackend() = default;
    
    bool initialize() override { return true; }
    
    CompileResult compile(const ZCCIRModule& module,
                         const CompileOptions& options) override;
    
    std::string getName() const override { return "mc"; }
    bool isSupported() const override { return true; }
    std::string getFileExtension() const override { return ".jar"; }
    
private:
    // 生成 MC Java 代码
    std::string generateMCJavaCode(const ZCCIRModule& module);
    
    // 生成方块定义
    std::string generateBlockClass(const std::string& name);
    
    // 生成物品定义
    std::string generateItemClass(const std::string& name);
    
    // 生成合成配方
    std::string generateRecipe(const std::string& name);
    
    // 打包为 JAR
    bool packageJar(const std::string& sourceDir, const std::string& outputPath);
};

// ==================== BIOS 后端 ====================

// BIOS 后端实现
class BIOSBackend : public Backend {
public:
    BIOSBackend() = default;
    
    bool initialize() override { return true; }
    
    CompileResult compile(const ZCCIRModule& module,
                         const CompileOptions& options) override;
    
    std::string getName() const override { return "bios"; }
    bool isSupported() const override { return true; }
    std::string getFileExtension() const override { return ".bin"; }
    
private:
    // 生成 x86 实模式汇编
    std::string generateAssembly(const ZCCIRModule& module);
    
    // 生成引导扇区
    std::vector<uint8_t> generateBootSector();
    
    // x86 指令编码
    uint8_t encodeMOV(uint8_t reg, uint8_t value);
    uint8_t encodeINT(uint8_t num);
    uint8_t encodeJMP(uint16_t offset);
    
    std::vector<uint8_t> output_;
};

// ==================== MicroPython 后端 ====================

// MicroPython 后端实现
class MicroPythonBackend : public Backend {
public:
    MicroPythonBackend() = default;
    
    bool initialize() override { return true; }
    
    CompileResult compile(const ZCCIRModule& module,
                         const CompileOptions& options) override;
    
    std::string getName() const override { return "micropython"; }
    bool isSupported() const override { return true; }
    std::string getFileExtension() const override { return ".mpy"; }
    
private:
    // 生成 MicroPython 字节码
    std::vector<uint8_t> generateMicroPythonBytecode(const ZCCIRModule& module);
    
    // MicroPython 指令
    void emitMPInstruction(uint8_t opcode, uint16_t arg = 0);
    
    std::vector<uint8_t> output_;
};

} // namespace zcc

#endif // ZCC_BACKEND_HPP

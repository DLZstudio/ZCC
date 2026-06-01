// ZCC JIT Backend — LLVM JIT 接口桩 (P2 预埋)
// 
// 设计意图:
//   当 ZCC_ENABLE_LLVM_JIT 定义为 1 时，链接 LLVM 库并启用
//   JITBackend 实现。当前默认关闭，所有方法返回 unimplemented。
//
// 使用方式:
//   cmake -DZCC_ENABLE_LLVM_JIT=ON ..
//   #define ZCC_ENABLE_LLVM_JIT 1   // 或在编译时手动定义

#ifndef ZCC_JIT_BACKEND_HPP
#define ZCC_JIT_BACKEND_HPP

#include "Backend.hpp"
#include "ZCCIR.hpp"
#include <string>
#include <vector>

namespace zcc {

// ============ JIT 编译选项 ============

struct JITCompileOptions {
    enum class OptLevel { None, Default, Aggressive };
    OptLevel optLevel = OptLevel::Default;
    bool verifyModule = true;           // LLVM IR 验证
    bool dumpModule = false;            // 打印 LLVM IR 到 stderr
    bool enableLazyCompilation = true;  // 延迟编译 (lazy JIT)
    std::string targetTriple;           // 目标三元组 (空=自动检测)
    std::string cpuFeatures;            // CPU 特性 (空=自动检测)
};

// ============ JIT 后端接口 ============

class JITBackend : public Backend {
public:
    JITBackend() = default;
    
    bool initialize() override { return supported(); }
    std::string getName() const override { return "jit"; }
    std::string getFileExtension() const override { return ".exe"; }
    std::string getDescription() const override { 
        return "LLVM JIT Compiler (P2 pre-embed)"; 
    }
    
    // 运行时检测 LLVM 支持
    static bool supported();
    
    // 编译 IR 模块 → 可执行代码
    CompileResult compile(const ZCCIRModule& module, 
                         const CompileOptions& options) override;
    
    // JIT 编译并直接执行
    static RuntimeValue runJIT(const ZCCIRModule& module,
                              const JITCompileOptions& opts = {});
    
private:
    // LLVM 相关内部方法 (仅当 ZCC_ENABLE_LLVM_JIT 时实现)
    static void* createLLVMContext();
    static void* createLLVMModule(const std::string& name);
    static void  compileToNative(void* llvmModule, const JITCompileOptions& opts);
    static void* lookupFunction(void* llvmModule, const std::string& name);
    static void  disposeLLVMContext(void* ctx);
};

// ============ JIT 函数类型 ============

// JIT 编译后的函数指针类型: int64_t (*)(int64_t)
using JITFunc = int64_t (*)(int64_t);

// 无参 JIT 函数: int64_t (*)()
using JITFuncVoid = int64_t (*)();

// ============ 默认实现 (桩) ============

inline bool JITBackend::supported() {
#ifdef ZCC_ENABLE_LLVM_JIT
    return true;
#else
    return false;  // 未编译 LLVM → ForceCompiler 模式或 fallback
#endif
}

inline CompileResult JITBackend::compile(const ZCCIRModule& module,
                                         const CompileOptions& options) {
    CompileResult result;
#ifndef ZCC_ENABLE_LLVM_JIT
    result.success = false;
    result.errorMessage = "JIT backend is not enabled. "
                          "Build with -DZCC_ENABLE_LLVM_JIT=ON to enable LLVM JIT.";
#else
    // TODO: P2 实现 — 调用 LLVM C API 编译 IR 模块
    // 流程:
    //   1. createLLVMContext()
    //   2. ZCCIR → LLVM IR (遍历 ZCCIRModule, 逐指令翻译)
    //   3. LLVM Pass Manager 优化
    //   4. compileToNative() → JIT 代码
    //   5. lookupFunction("main") → 可调用函数指针
    result.success = false;
    result.errorMessage = "JIT compilation not yet implemented (P2 phase 2)";
#endif
    return result;
}

inline RuntimeValue JITBackend::runJIT(const ZCCIRModule& module,
                                       const JITCompileOptions& opts) {
#ifndef ZCC_ENABLE_LLVM_JIT
    return RuntimeValue();  // JIT 不可用 → null
#else
    // TODO: P2 实现
    return RuntimeValue();
#endif
}

// LLVM 内部桩
inline void* JITBackend::createLLVMContext() { return nullptr; }
inline void* JITBackend::createLLVMModule(const std::string&) { return nullptr; }
inline void  JITBackend::compileToNative(void*, const JITCompileOptions&) {}
inline void* JITBackend::lookupFunction(void*, const std::string&) { return nullptr; }
inline void  JITBackend::disposeLLVMContext(void*) {}

} // namespace zcc

#endif // ZCC_JIT_BACKEND_HPP

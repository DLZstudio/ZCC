#ifndef ZCC_COMPILER_HPP
#define ZCC_COMPILER_HPP

#include "Lexer.hpp"
#include "Parser.hpp"
#include "AST.hpp"
#include "ZCCIR.hpp"
#include "Bytecode.hpp"
#include "Diagnostics.hpp"
#include <string>
#include <vector>
#include <memory>

namespace zcc {

// 编译器选项
struct CompilerOptions {
    std::string outputFilename;       // 输出文件名
    int optimizationLevel = 0;        // 优化级别 (0-3)
    bool debugInfo = false;           // 生成调试信息
    bool useCache = true;             // 使用缓存
    std::string backend;              // 后端名称
    std::string targetTriple;         // 目标平台
};

// 编译结果
struct CompileResult {
    bool success;
    std::vector<Diagnostic> diagnostics;
    std::vector<Diagnostic> errors;
    std::vector<Diagnostic> warnings;
    std::string errorMessage;
    
    // 前端结果
    std::vector<Token> tokens;
    ProgramPtr program;
    
    // 中端结果
    std::unique_ptr<ZCCIRModule> ir;
    
    // 后端结果
    std::string output;               // 文本输出（如 C 代码）
    std::vector<uint8_t> binaryData;  // 二进制输出
    
    // 性能统计
    uint64_t compileTimeMs = 0;
    
    bool hasErrors() const {
        for (const auto& diag : diagnostics) {
            if (diag.severity == DiagnosticSeverity::Error) {
                return true;
            }
        }
        return !errors.empty();
    }
    
    bool hasWarnings() const {
        for (const auto& diag : diagnostics) {
            if (diag.severity == DiagnosticSeverity::Warning) {
                return true;
            }
        }
        return !warnings.empty();
    }
};

// 编译器类
class Compiler {
public:
    Compiler();
    explicit Compiler(CompilerOptions options);
    
    // 编译源代码
    CompileResult compile(const std::string& sourceCode, const std::string& filename);
    
    // 编译文件
    CompileResult compileFile(const std::string& filename);
    
    // 保存字节码（兼容旧版本）
    static bool saveBytecode(const BytecodeModule& module, const std::string& filename);
    
    // 加载字节码（兼容旧版本）
    static BytecodeModule loadBytecode(const std::string& filename);
    
    // 反汇编字节码（兼容旧版本）
    static std::string disassemble(const BytecodeModule& module);
    
    // 获取诊断信息
    const std::vector<Diagnostic>& getDiagnostics() const { return diagnostics_; }
    
    // 设置选项
    void setOptions(const CompilerOptions& options) { options_ = options; }
    const CompilerOptions& getOptions() const { return options_; }
    
private:
    // 编译阶段
    CompileResult runLexer(const std::string& sourceCode, const std::string& filename);
    CompileResult runParser(const std::vector<Token>& tokens, const std::string& filename);
    CompileResult runIRGenerator(ProgramPtr program);
    CompileResult runOptimizer(std::unique_ptr<ZCCIRModule>& ir);
    CompileResult runBackend(std::unique_ptr<ZCCIRModule>& ir);
    
    CompilerOptions options_;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace zcc

#endif // ZCC_COMPILER_HPP

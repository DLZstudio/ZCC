#ifndef ZCC_COMPILER_HPP
#define ZCC_COMPILER_HPP

#include <string>
#include <vector>
#include "Lexer.hpp"
#include "Parser.hpp"
#include "CodeGenerator.hpp"
#include "Bytecode.hpp"
#include "Diagnostics.hpp"

namespace zcc {

// 编译器选项
struct CompilerOptions {
    std::string outputFilename;       // 输出文件名
    bool optimize = false;            // 启用优化
    bool debugInfo = false;           // 生成调试信息
    bool emitBytecode = true;         // 输出字节码
    bool emitAssembly = false;        // 输出汇编（预留）
    int optimizationLevel = 0;        // 优化级别 (0-3)
    std::string targetPlatform;       // 目标平台
};

// 编译结果
struct CompileResult {
    bool success;
    std::vector<Diagnostic> diagnostics;
    BytecodeModule bytecode;
    std::vector<uint8_t> binaryData;
    
    bool hasErrors() const {
        for (const auto& diag : diagnostics) {
            if (diag.severity == DiagnosticSeverity::Error) {
                return true;
            }
        }
        return !success;
    }
};

// 编译器类
class Compiler {
public:
    Compiler();
    explicit Compiler(CompilerOptions options);
    
    // 编译源文件
    CompileResult compile(const std::string& sourceCode, const std::string& filename);
    
    // 编译文件
    CompileResult compileFile(const std::string& filename);
    
    // 保存字节码到文件
    static bool saveBytecode(const BytecodeModule& module, const std::string& filename);
    
    // 从文件加载字节码
    static BytecodeModule loadBytecode(const std::string& filename);
    
    // 反汇编字节码
    static std::string disassemble(const BytecodeModule& module);
    
    // 获取诊断信息
    const std::vector<Diagnostic>& getDiagnostics() const { return diagnostics_; }
    
    // 设置选项
    void setOptions(const CompilerOptions& options) { options_ = options; }
    const CompilerOptions& getOptions() const { return options_; }

private:
    CompilerOptions options_;
    std::vector<Diagnostic> diagnostics_;
    
    // 编译阶段
    CompileResult runLexer(const std::string& sourceCode, const std::string& filename);
    CompileResult runParser(const std::vector<Token>& tokens, const std::string& filename);
    CompileResult runCodeGenerator(ProgramPtr program);
    
    // 工具函数
    std::string readFile(const std::string& filename);
    void addDiagnostic(const Diagnostic& diag);
};

} // namespace zcc

#endif // ZCC_COMPILER_HPP

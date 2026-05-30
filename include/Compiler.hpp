#ifndef ZCC_COMPILER_HPP
#define ZCC_COMPILER_HPP

#include <string>
#include <vector>
#include <memory>
#include "Lexer.hpp"
#include "Parser.hpp"
#include "AST.hpp"
#include "ZCCIR.hpp"
#include "Diagnostics.hpp"
#include "Common.hpp"

namespace zcc {

// 编译器选项（扩展 CompileOptions）
struct CompilerOptions : public CompileOptions {
    std::string outputFilename;
    bool useCache = true;
    std::string backend;
};

// 编译结果（扩展 CompileResult）
struct CompilerCompileResult : public CompileResult {
    std::vector<Diagnostic> diagnostics;
    std::unique_ptr<ZCCIRModule> ir;
    
    bool hasErrors() const {
        for (const auto& diag : diagnostics) {
            if (diag.severity == DiagnosticSeverity::Error) {
                return true;
            }
        }
        return CompileResult::hasErrors();
    }
};

// 编译器类
class Compiler {
public:
    Compiler();
    explicit Compiler(CompilerOptions options);

    // 编译源文件
    CompilerCompileResult compile(const std::string& sourceCode, const std::string& filename);

    // 编译文件
    CompilerCompileResult compileFile(const std::string& filename);

    // 获取诊断信息
    const std::vector<Diagnostic>& getDiagnostics() const { return diagnostics_; }

    // 设置选项
    void setOptions(const CompilerOptions& options) { options_ = options; }
    const CompilerOptions& getOptions() const { return options_; }

private:
    CompilerOptions options_;
    std::vector<Diagnostic> diagnostics_;

    // 编译阶段
    CompilerCompileResult runLexer(const std::string& sourceCode, const std::string& filename);
    CompilerCompileResult runParser(const std::vector<Token>& tokens, const std::string& filename);
    CompilerCompileResult runIRGenerator(ProgramPtr program);
    CompilerCompileResult runOptimizer(std::unique_ptr<ZCCIRModule>& ir);
    CompilerCompileResult runBackend(std::unique_ptr<ZCCIRModule>& ir);

    // 工具函数
    std::string readFile(const std::string& filename);
    void addDiagnostic(const Diagnostic& diag);
};

} // namespace zcc

#endif // ZCC_COMPILER_HPP

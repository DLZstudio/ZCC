// ZCC 编译器驱动程序
// 编排完整的编译链路: 源码 → AST → IR → 目标语言 → 外部编译器 → 可执行文件

#ifndef ZCC_COMPILER_DRIVER_HPP
#define ZCC_COMPILER_DRIVER_HPP

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include "IRGenerator.hpp"
#include "LanguageBackend.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"

namespace zcc {

// 编译请求
struct BuildRequest {
    std::string sourceFile;           // 输入 .zcc 文件
    std::string outputFile;           // 输出文件 (可选, 自动推断)
    std::string targetLanguage;       // 目标语言: "c", "cpp", 未来 "python", "java", ...
    int         optLevel = 1;         // 优化级别 0-3
    bool        keepIntermediate = true;  // 保留中间文件 (.c/.ir)
    std::string runtimeDir;           // zcc_runtime 所在目录
};

// 编译结果
struct BuildResult {
    bool        success = false;
    std::string outputFile;           // 最终产物路径
    std::string irFile;               // .ir 文件路径
    std::string sourceOutputFile;     // 生成的源码文件路径 (.c/.py/...)
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// 编译器驱动 — 编排完整编译链路
class CompilerDriver {
public:
    CompilerDriver();
    
    // 设置运行时目录
    void setRuntimeDir(const std::string& dir) { runtimeDir_ = dir; }
    
    // 完整编译: .zcc → 可执行文件
    BuildResult build(const BuildRequest& request);
    
    // 仅生成 IR
    BuildResult generateIR(const std::string& sourceFile, const std::string& outputIR);
    
    // 列出可用的目标语言
    std::vector<std::string> getAvailableLanguages() const;
    
private:
    // 管线步骤
    bool runLexer(const std::string& source, const std::string& filename,
                  std::vector<Token>& tokens, BuildResult& result);
    bool runParser(const std::vector<Token>& tokens, const std::string& filename,
                   ProgramPtr& program, BuildResult& result);
    bool runIRGenerator(ProgramPtr program, const std::string& filename,
                        std::unique_ptr<ZCCIRModule>& ir, BuildResult& result);
    bool runLanguageBackend(const ZCCIRModule& ir, ILanguageBackend* backend,
                            const BuildRequest& request, BuildResult& result);
    bool runExternalCompiler(ILanguageBackend* backend,
                             const BuildRequest& request, BuildResult& result);
    
    // 工具
    std::string readFile(const std::string& path);
    bool writeFile(const std::string& path, const std::string& content);
    std::string withoutExtension(const std::string& path);
    
    std::string runtimeDir_;
};

// ==================== 实现 ====================

inline CompilerDriver::CompilerDriver() {
    initializeBuiltinBackends();
}

inline std::vector<std::string> CompilerDriver::getAvailableLanguages() const {
    return LanguageBackendRegistry::instance().getAvailableLanguages();
}

inline BuildResult CompilerDriver::build(const BuildRequest& request) {
    BuildResult result;
    
    // 1. 读取源文件
    std::string source = readFile(request.sourceFile);
    if (source.empty()) {
        result.errors.push_back("Cannot read file: " + request.sourceFile);
        return result;
    }
    
    // 2. 词法分析
    std::vector<Token> tokens;
    if (!runLexer(source, request.sourceFile, tokens, result)) return result;
    
    // 3. 语法分析
    ProgramPtr program;
    if (!runParser(tokens, request.sourceFile, program, result)) return result;
    
    // 4. IR 生成
    std::unique_ptr<ZCCIRModule> ir;
    if (!runIRGenerator(program, request.sourceFile, ir, result)) return result;
    
    // 5. 选择语言后端
    auto* backend = LanguageBackendRegistry::instance().getBackend(request.targetLanguage);
    if (!backend) {
        // 尝试用默认后端
        backend = LanguageBackendRegistry::instance().getDefault();
        if (!backend) {
            result.errors.push_back("No language backend available");
            return result;
        }
    }
    
    std::cout << "Target: " << backend->getLanguageName() 
              << " (" << backend->getDescription() << ")\n";
    
    // 6. 生成目标语言源码
    if (!runLanguageBackend(*ir, backend, request, result)) return result;
    
    // 7. 外部编译
    if (backend->needsCompilation()) {
        if (!runExternalCompiler(backend, request, result)) return result;
    }
    
    result.success = true;
    return result;
}

inline bool CompilerDriver::runLexer(const std::string& source, const std::string& filename,
                                      std::vector<Token>& tokens, BuildResult& result) {
    Lexer lexer(source, filename);
    tokens = lexer.tokenize();
    
    if (lexer.hasErrors()) {
        for (const auto& e : lexer.getErrors())
            result.errors.push_back("Lexer: " + e.message);
        return false;
    }
    return true;
}

inline bool CompilerDriver::runParser(const std::vector<Token>& tokens, const std::string& filename,
                                       ProgramPtr& program, BuildResult& result) {
    Parser parser(tokens, filename);
    program = parser.parse();
    
    if (parser.hasErrors()) {
        for (const auto& e : parser.getErrors())
            result.errors.push_back("Parser: " + e.message);
        return false;
    }
    return true;
}

inline bool CompilerDriver::runIRGenerator(ProgramPtr program, const std::string& filename,
                                            std::unique_ptr<ZCCIRModule>& ir, BuildResult& result) {
    IRGeneratorConfig irConfig;
    irConfig.moduleName = filename;
    irConfig.target = TargetPlatform::getCurrent();
    
    IRGenerator irGen(irConfig);
    ir = irGen.generate(program);
    
    if (irGen.hasErrors()) {
        for (const auto& e : irGen.getErrors())
            result.errors.push_back("IR: " + e.message);
        return false;
    }
    return true;
}

inline bool CompilerDriver::runLanguageBackend(const ZCCIRModule& ir, ILanguageBackend* backend,
                                                const BuildRequest& request, BuildResult& result) {
    CompileOptions opts;
    opts.optimization = static_cast<OptimizationLevel>(request.optLevel);
    
    result.sourceOutputFile = withoutExtension(request.sourceFile) + backend->getFileExtension();
    std::string sourceCode = backend->generateSource(ir, opts);
    
    if (!writeFile(result.sourceOutputFile, sourceCode)) {
        result.errors.push_back("Cannot write: " + result.sourceOutputFile);
        return false;
    }
    
    std::cout << "Generated: " << result.sourceOutputFile << "\n";
    return true;
}

inline bool CompilerDriver::runExternalCompiler(ILanguageBackend* backend,
                                                  const BuildRequest& request, BuildResult& result) {
    auto cc = backend->getCompilerConfig();
    if (!cc) {
        // 解释型语言, 不需要编译步骤
        result.outputFile = result.sourceOutputFile;
        return true;
    }
    
    result.outputFile = request.outputFile.empty() ?
        withoutExtension(request.sourceFile) + backend->getExecutableExtension() :
        request.outputFile;
    
    // 收集运行时文件
    std::vector<std::string> runtimeFiles;
    for (const auto& f : backend->getRuntimeSourceFiles()) {
        runtimeFiles.push_back(runtimeDir_ + "/" + f);
    }
    
    std::string cmd = cc->buildCommand(result.sourceOutputFile, result.outputFile,
                                        runtimeFiles, request.optLevel);
    cmd += " -O" + std::to_string(request.optLevel);
    
    std::cout << "Compiling: " << cmd << "\n";
    int ret = std::system(cmd.c_str());
    
    if (ret != 0) {
        result.errors.push_back("External compiler failed with code " + std::to_string(ret));
        return false;
    }
    
    std::cout << "Output: " << result.outputFile << "\n";
    return true;
}

// ==================== 工具方法 ====================

inline std::string CompilerDriver::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    return std::string((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
}

inline bool CompilerDriver::writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file) return false;
    file << content;
    return file.good();
}

inline std::string CompilerDriver::withoutExtension(const std::string& path) {
    auto dot = path.rfind('.');
    auto slash = path.rfind('/');
    auto bslash = path.rfind('\\');
    if (dot == std::string::npos) return path;
    if (slash != std::string::npos && dot < slash) return path;
    if (bslash != std::string::npos && dot < bslash) return path;
    return path.substr(0, dot);
}

inline BuildResult CompilerDriver::generateIR(const std::string& sourceFile,
                                               const std::string& outputIR) {
    BuildResult result;
    
    // 快速参考: 走 irCommand 路径
    std::string source = readFile(sourceFile);
    if (source.empty()) {
        result.errors.push_back("Cannot read: " + sourceFile);
        return result;
    }
    
    std::vector<Token> tokens;
    if (!runLexer(source, sourceFile, tokens, result)) return result;
    
    ProgramPtr program;
    if (!runParser(tokens, sourceFile, program, result)) return result;
    
    std::unique_ptr<ZCCIRModule> ir;
    if (!runIRGenerator(program, sourceFile, ir, result)) return result;
    
    std::string irText = ir->toText();
    if (!writeFile(outputIR, irText)) {
        result.errors.push_back("Cannot write IR to: " + outputIR);
        return result;
    }
    
    result.irFile = outputIR;
    result.success = true;
    return result;
}

} // namespace zcc

#endif // ZCC_COMPILER_DRIVER_HPP

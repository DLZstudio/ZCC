#include "Compiler.hpp"
#include "IRGenerator.hpp"
#include "Optimizer.hpp"
#include "Backend.hpp"
#include "CacheManager.hpp"
#include <chrono>

namespace zcc {

Compiler::Compiler() : options_{} {}

Compiler::Compiler(CompilerOptions options) : options_(std::move(options)) {}

CompileResult Compiler::compile(const std::string& sourceCode, const std::string& filename) {
    CompileResult result;
    result.success = true;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 检查缓存
    if (options_.useCache) {
        // TODO: 实现缓存查找
        // auto cacheResult = getCacheManager().check(...);
        // if (cacheResult.hit) { ... }
    }
    
    // 2. 词法分析
    auto lexerResult = runLexer(sourceCode, filename);
    if (!lexerResult.success) {
        result.success = false;
        result.errors = lexerResult.diagnostics;
        return result;
    }
    
    // 3. 语法分析
    auto parserResult = runParser(lexerResult.tokens, filename);
    if (!parserResult.success) {
        result.success = false;
        result.errors = parserResult.diagnostics;
        return result;
    }
    
    // 4. 生成 ZCC-IR
    auto irResult = runIRGenerator(parserResult.program);
    if (!irResult.success) {
        result.success = false;
        result.errors = irResult.diagnostics;
        return result;
    }
    
    // 5. 优化 IR
    auto optimizeResult = runOptimizer(irResult.ir);
    if (!optimizeResult.success) {
        result.success = false;
        result.errors = optimizeResult.diagnostics;
        return result;
    }
    
    // 6. 后端编译
    auto backendResult = runBackend(optimizeResult.ir);
    if (!backendResult.success) {
        result.success = false;
        result.errors = backendResult.errors;
        return result;
    }
    
    // 7. 保存到缓存
    if (options_.useCache) {
        // TODO: 实现缓存保存
    }
    
    // 设置结果
    result.binaryData = backendResult.binary;
    result.output = backendResult.output;
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.compileTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime
    ).count();
    
    return result;
}

CompileResult Compiler::compileFile(const std::string& filename) {
    // 读取文件
    std::ifstream file(filename);
    if (!file) {
        CompileResult result;
        result.success = false;
        result.diagnostics.push_back(Diagnostic::error(
            "Failed to read file: " + filename,
            SourceLocation(filename, 0, 0, 0)
        ));
        return result;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    return compile(content, filename);
}

CompileResult Compiler::runLexer(const std::string& sourceCode, const std::string& filename) {
    CompileResult result;
    result.success = true;
    
    Lexer lexer(sourceCode, filename);
    result.tokens = lexer.tokenize();
    result.diagnostics = lexer.getErrors();
    
    if (lexer.hasErrors()) {
        result.success = false;
    }
    
    return result;
}

CompileResult Compiler::runParser(const std::vector<Token>& tokens, const std::string& filename) {
    CompileResult result;
    result.success = true;
    
    Parser parser(tokens, filename);
    result.program = parser.parse();
    result.diagnostics = parser.getErrors();
    
    if (parser.hasErrors()) {
        result.success = false;
    }
    
    return result;
}

CompileResult Compiler::runIRGenerator(ProgramPtr program) {
    CompileResult result;
    result.success = true;
    
    IRGeneratorConfig config;
    config.emitDebugInfo = options_.debugInfo;
    config.optimize = options_.optimizationLevel > 0;
    config.moduleName = "main";
    config.target = TargetPlatform::getCurrent();
    
    IRGenerator generator(config);
    result.ir = generator.generate(program);
    result.diagnostics = generator.getErrors();
    
    if (generator.hasErrors()) {
        result.success = false;
    }
    
    return result;
}

CompileResult Compiler::runOptimizer(std::unique_ptr<ZCCIRModule>& ir) {
    CompileResult result;
    result.success = true;
    
    if (options_.optimizationLevel == 0) {
        return result;  // 无优化
    }
    
    OptimizationLevel level = static_cast<OptimizationLevel>(options_.optimizationLevel);
    auto optimizer = createOptimizer(level);
    
    bool modified = optimizer->optimize(*ir);
    
    // 记录优化统计
    auto stats = optimizer->getStats();
    // TODO: 输出优化信息
    
    return result;
}

CompileResult Compiler::runBackend(std::unique_ptr<ZCCIRModule>& ir) {
    CompileResult result;
    result.success = true;
    
    // 选择后端
    auto& backendManager = BackendManager::getInstance();
    Backend* backend = nullptr;
    
    if (!options_.backend.empty()) {
        backend = backendManager.getBackend(options_.backend);
    }
    
    if (!backend) {
        // 根据目标平台选择
        TargetPlatform target = TargetPlatform::getCurrent();
        backend = backendManager.selectBackendForTarget(target);
    }
    
    if (!backend) {
        result.success = false;
        result.errorMessage = "No suitable backend found";
        return result;
    }
    
    // 准备编译选项
    CompileOptions backendOptions;
    backendOptions.optimization = static_cast<OptimizationLevel>(options_.optimizationLevel);
    backendOptions.debugInfo = options_.debugInfo;
    backendOptions.outputPath = options_.outputFilename;
    
    // 编译
    auto compileResult = backend->compile(*ir, backendOptions);
    
    if (!compileResult.success) {
        result.success = false;
        result.errors = compileResult.errors;
        result.errorMessage = compileResult.errorMessage;
        return result;
    }
    
    result.output = compileResult.output;
    result.binaryData = compileResult.binary;
    
    return result;
}

bool Compiler::saveBytecode(const BytecodeModule& module, const std::string& filename) {
    // TODO: 实现字节码保存（用于旧版本兼容）
    return false;
}

BytecodeModule Compiler::loadBytecode(const std::string& filename) {
    // TODO: 实现字节码加载（用于旧版本兼容）
    return BytecodeModule();
}

std::string Compiler::disassemble(const BytecodeModule& module) {
    return Disassembler::disassemble(module);
}

} // namespace zcc

#include "Compiler.hpp"
#include "Bytecode.hpp"
#include "CodeGenerator.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

namespace zcc {

Compiler::Compiler() {}

Compiler::Compiler(CompilerOptions options) : options_(std::move(options)) {}

CompilerCompileResult Compiler::compile(const std::string& sourceCode, const std::string& filename) {
    CompilerCompileResult result;
    result.success = true;
    diagnostics_.clear();
    
    // 阶段 1: 词法分析
    Lexer lexer(sourceCode, filename);
    std::vector<Token> tokens = lexer.tokenize();
    
    // 收集词法分析错误
    for (const auto& diag : lexer.getErrors()) {
        addDiagnostic(diag);
        result.diagnostics.push_back(diag);
    }
    
    if (lexer.hasErrors()) {
        result.success = false;
        return result;
    }
    
    // 阶段 2: 语法分析
    Parser parser(tokens, filename);
    ProgramPtr program = parser.parse();
    
    // 收集语法分析错误
    for (const auto& diag : parser.getErrors()) {
        addDiagnostic(diag);
        result.diagnostics.push_back(diag);
    }
    
    if (parser.hasErrors()) {
        result.success = false;
        return result;
    }
    
    // 阶段 3: 代码生成
    CodeGenerator codeGen;
    BytecodeModule bytecode = codeGen.generate(program);
    
    // 收集代码生成错误
    for (const auto& diag : codeGen.getErrors()) {
        addDiagnostic(diag);
        result.diagnostics.push_back(diag);
    }
    
    if (codeGen.hasErrors()) {
        result.success = false;
        return result;
    }
    
    // 序列化字节码
    result.binary = bytecode.serialize();
    
    return result;
}

CompilerCompileResult Compiler::compileFile(const std::string& filename) {
    std::string sourceCode = readFile(filename);
    if (sourceCode.empty()) {
        CompilerCompileResult result;
        result.success = false;
        result.diagnostics.push_back(Diagnostic::error(
            "Failed to read file: " + filename, 
            SourceLocation(filename, 0, 0, 0)));
        return result;
    }
    return compile(sourceCode, filename);
}

std::string Compiler::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // 处理 Windows 换行符 \r\n -> \n
    std::string result;
    result.reserve(content.size());
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\r' && i + 1 < content.size() && content[i + 1] == '\n') {
            // 跳过 \r，只保留 \n
            continue;
        }
        result += content[i];
    }
    
    return result;
}

void Compiler::addDiagnostic(const Diagnostic& diag) {
    diagnostics_.push_back(diag);
}

} // namespace zcc

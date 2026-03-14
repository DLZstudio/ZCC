#include "Compiler.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

namespace zcc {

Compiler::Compiler() {}

Compiler::Compiler(CompilerOptions options) : options_(std::move(options)) {}

CompileResult Compiler::compile(const std::string& sourceCode, const std::string& filename) {
    CompileResult result;
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
    result.bytecode = codeGen.generate(program);
    
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
    result.binaryData = result.bytecode.serialize();
    
    return result;
}

CompileResult Compiler::compileFile(const std::string& filename) {
    std::string sourceCode = readFile(filename);
    if (sourceCode.empty()) {
        CompileResult result;
        result.success = false;
        result.diagnostics.push_back(Diagnostic::error(
            "Failed to read file: " + filename, 
            SourceLocation(filename, 0, 0, 0)));
        return result;
    }
    return compile(sourceCode, filename);
}

bool Compiler::saveBytecode(const BytecodeModule& module, const std::string& filename) {
    std::vector<uint8_t> data = module.serialize();
    
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

BytecodeModule Compiler::loadBytecode(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open bytecode file: " + filename);
    }
    
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    
    return BytecodeModule::deserialize(data);
}

std::string Compiler::disassemble(const BytecodeModule& module) {
    return Disassembler::disassemble(module);
}

std::string Compiler::readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void Compiler::addDiagnostic(const Diagnostic& diag) {
    diagnostics_.push_back(diag);
}

} // namespace zcc

// ZCC 爆破测试运行器 - 最小化测试工具
// 仅使用 Lexer + Parser + IRGenerator（避开有问题的模块系统）
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include "Lexer.hpp"
#include "Parser.hpp"
#include "IRGenerator.hpp"
#include "Diagnostics.hpp"
#include "core/Common.hpp"
#include "core/Platform.hpp"

using namespace zcc;

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(70, '=') << "\n";
}

int testFile(const std::string& filepath) {
    printSeparator("Testing: " + filepath);

    // 1. 读取源文件
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "ERROR: Cannot open file: " << filepath << "\n";
        return 1;
    }
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    std::cout << "Source size: " << source.size() << " bytes\n";

    // 2. 词法分析
    int lexerErrors = 0;
    Lexer lexer(source, filepath);
    auto tokens = lexer.tokenize();

    if (lexer.hasErrors()) {
        std::cout << "\n  [LEXER ERRORS] (" << lexer.getErrors().size() << ")\n";
        std::cout << "  " << std::string(60, '-') << "\n";
        for (auto& e : lexer.getErrors()) {
            std::cout << "  Line " << e.location.line << ":" << e.location.column
                      << " - " << e.message << "\n";
            lexerErrors++;
        }
    }

    std::cout << "\n  Tokens: " << tokens.size() << " (including EOF)\n";
    if (tokens.size() <= 20) {
        for (auto& t : tokens) {
            std::cout << "    " << t.toString() << "\n";
        }
    } else {
        std::cout << "    (first 10) ";
        for (size_t i = 0; i < std::min(size_t(10), tokens.size()); i++) {
            std::cout << tokens[i].toString() << (i < 9 ? " | " : "");
        }
        std::cout << "\n    (last 5) ";
        for (size_t i = tokens.size() > 5 ? tokens.size() - 5 : 0; i < tokens.size(); i++) {
            std::cout << tokens[i].toString() << (i < tokens.size() - 1 ? " | " : "");
        }
        std::cout << "\n";
    }

    // 3. 语法分析
    int parserErrors = 0;
    Parser parser(tokens, filepath);
    auto program = parser.parse();

    if (parser.hasErrors()) {
        std::cout << "\n  [PARSER ERRORS] (" << parser.getErrors().size() << ")\n";
        std::cout << "  " << std::string(60, '-') << "\n";
        for (auto& e : parser.getErrors()) {
            std::cout << "  " << e.message << "\n";
            parserErrors++;
        }
    }

    std::cout << "\n  AST Declarations: " << program->declarations.size() << "\n";
    for (size_t i = 0; i < std::min(size_t(20), program->declarations.size()); i++) {
        std::cout << "    [" << i << "] " << program->declarations[i]->toString().substr(0, 70);
        if (program->declarations[i]->toString().length() > 70) std::cout << "...";
        std::cout << "\n";
    }
    if (program->declarations.size() > 20) {
        std::cout << "    ... and " << (program->declarations.size() - 20) << " more\n";
    }

    // 4. IR 生成
    int irErrors = 0;
    IRGeneratorConfig irConfig;
    irConfig.moduleName = filepath;
    irConfig.target = TargetPlatform::getCurrent();
    IRGenerator irGen(irConfig);
    auto irModule = irGen.generate(program);

    if (irGen.hasErrors()) {
        std::cout << "\n  [IR ERRORS] (" << irGen.getErrors().size() << ")\n";
        std::cout << "  " << std::string(60, '-') << "\n";
        for (auto& e : irGen.getErrors()) {
            std::cout << "  " << e.message << "\n";
            irErrors++;
        }
    } else {
        std::string irText = irModule->toText();
        // 保存 IR 输出以便审计
        if (!irText.empty()) {
            std::string irPath = filepath + ".ir";
            std::ofstream irFile(irPath);
            if (irFile) {
                irFile << irText;
                irFile.close();
            }
        }
        std::cout << "\n  IR generated: " << irText.size() << " bytes\n";
        std::cout << "  IR Lines: " << (std::count(irText.begin(), irText.end(), '\n') + 1) << "\n";
    }

    // 5. 总结
    printSeparator("SUMMARY: " + filepath);
    int totalErrors = lexerErrors + parserErrors + irErrors;
    if (totalErrors == 0) {
        std::cout << "  Result: PASS (No errors)\n";
    } else {
        std::cout << "  Result: " << totalErrors << " ERRORS"
                  << " (Lexer:" << lexerErrors
                  << " Parser:" << parserErrors
                  << " IR:" << irErrors << ")\n";
    }
    std::cout << std::string(70, '=') << "\n";
    return totalErrors > 0 ? 1 : 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <test_file.zcc> [test_file2.zcc ...]\n";
        return 1;
    }

    int totalFailures = 0;
    for (int i = 1; i < argc; i++) {
        totalFailures += testFile(argv[i]);
    }

    std::cout << "\n\n";
    printSeparator("FINAL RESULT");
    std::cout << "  Total files tested: " << (argc - 1) << "\n";
    std::cout << "  Total failures: " << totalFailures << "\n";
    std::cout << std::string(70, '=') << "\n";

    return 0;
}

#include <iostream>
#include <string>
#include <cstring>
#include "Compiler.hpp"
#include "Bytecode.hpp"

void printUsage(const char* programName) {
    std::cout << "ZCC - ZCC Language Compiler v0.1.0\n\n";
    std::cout << "用法:\n";
    std::cout << "  " << programName << " <命令> [选项] <源文件>\n\n";
    std::cout << "命令:\n";
    std::cout << "  compile, c    编译源文件为字节码\n";
    std::cout << "  run, r        编译并运行 (需要解释器)\n";
    std::cout << "  disasm, d     反汇编字节码文件\n";
    std::cout << "  help, h       显示帮助信息\n\n";
    std::cout << "选项:\n";
    std::cout << "  -o <文件>     指定输出文件名\n";
    std::cout << "  -O0           无优化\n";
    std::cout << "  -O1           基础优化\n";
    std::cout << "  -O2           中级优化\n";
    std::cout << "  -O3           高级优化\n";
    std::cout << "  -g            生成调试信息\n";
    std::cout << "  --target <平台>  目标平台 (windows/linux/macos/wasm)\n";
    std::cout << "  --version     显示版本信息\n\n";
    std::cout << "示例:\n";
    std::cout << "  " << programName << " compile hello.zcc -o hello.zcb\n";
    std::cout << "  " << programName << " disasm hello.zcb\n";
    std::cout << "  " << programName << " compile main.zcc -O2 -g\n";
}

void printVersion() {
    std::cout << "ZCC Compiler v0.1.0\n";
    std::cout << "Zero-Core Command Language\n";
    std::cout << "Built with C++17\n";
}

int compileCommand(int argc, char* argv[], int argIndex, zcc::CompilerOptions& options) {
    if (argIndex >= argc) {
        std::cerr << "错误：未指定源文件\n";
        return 1;
    }
    
    std::string inputFile = argv[argIndex];
    
    // 解析编译选项
    for (int i = argIndex + 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-o" && i + 1 < argc) {
            options.outputFilename = argv[++i];
        } else if (arg == "-O0") {
            options.optimizationLevel = 0;
            options.optimize = false;
        } else if (arg == "-O1") {
            options.optimizationLevel = 1;
            options.optimize = true;
        } else if (arg == "-O2") {
            options.optimizationLevel = 2;
            options.optimize = true;
        } else if (arg == "-O3") {
            options.optimizationLevel = 3;
            options.optimize = true;
        } else if (arg == "-g") {
            options.debugInfo = true;
        } else if (arg == "--target" && i + 1 < argc) {
            options.targetPlatform = argv[++i];
        }
    }
    
    // 默认输出文件名
    if (options.outputFilename.empty()) {
        size_t dotPos = inputFile.rfind('.');
        if (dotPos != std::string::npos) {
            options.outputFilename = inputFile.substr(0, dotPos) + ".zcb";
        } else {
            options.outputFilename = inputFile + ".zcb";
        }
    }
    
    // 创建编译器并编译
    zcc::Compiler compiler(options);
    zcc::CompileResult result = compiler.compileFile(inputFile);
    
    // 输出诊断信息
    for (const auto& diag : result.diagnostics) {
        std::cerr << diag.toString() << std::endl;
    }
    
    if (!result.success) {
        std::cerr << "\n编译失败，共有 " << result.diagnostics.size() << " 个错误\n";
        return 1;
    }
    
    // 保存字节码
    if (zcc::Compiler::saveBytecode(result.bytecode, options.outputFilename)) {
        std::cout << "编译成功！输出文件：" << options.outputFilename << std::endl;
        std::cout << "字节码大小：" << result.binaryData.size() << " 字节" << std::endl;
        return 0;
    } else {
        std::cerr << "错误：无法保存输出文件\n";
        return 1;
    }
}

int disasmCommand(int argc, char* argv[], int argIndex) {
    if (argIndex >= argc) {
        std::cerr << "错误：未指定字节码文件\n";
        return 1;
    }
    
    std::string inputFile = argv[argIndex];
    
    try {
        zcc::BytecodeModule module = zcc::Compiler::loadBytecode(inputFile);
        std::string disassembly = zcc::Compiler::disassemble(module);
        std::cout << disassembly << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误：" << e.what() << std::endl;
        return 1;
    }
}

int runCommand(int argc, char* argv[], int argIndex, zcc::CompilerOptions& options) {
    if (argIndex >= argc) {
        std::cerr << "错误：未指定源文件\n";
        return 1;
    }
    
    std::string inputFile = argv[argIndex];
    
    // 编译
    zcc::Compiler compiler(options);
    zcc::CompileResult result = compiler.compileFile(inputFile);
    
    // 输出诊断信息
    for (const auto& diag : result.diagnostics) {
        std::cerr << diag.toString() << std::endl;
    }
    
    if (!result.success) {
        std::cerr << "\n编译失败\n";
        return 1;
    }
    
    std::cout << "编译成功！\n";
    std::cout << "注意：运行功能需要 ZCC 解释器，请使用第三方解释器运行生成的字节码文件。\n";
    std::cout << "字节码大小：" << result.binaryData.size() << " 字节" << std::endl;
    
    // 保存临时字节码文件
    std::string tempFile = inputFile + ".zcb";
    if (zcc::Compiler::saveBytecode(result.bytecode, tempFile)) {
        std::cout << "临时字节码文件：" << tempFile << std::endl;
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    zcc::CompilerOptions options;
    
    // 处理全局选项
    if (command == "--version" || command == "-v") {
        printVersion();
        return 0;
    }
    
    if (command == "--help" || command == "-h" || command == "help") {
        printUsage(argv[0]);
        return 0;
    }
    
    // 查找命令结束位置
    int argIndex = 2;
    
    if (command == "compile" || command == "c") {
        return compileCommand(argc, argv, argIndex, options);
    } else if (command == "disasm" || command == "d") {
        return disasmCommand(argc, argv, argIndex);
    } else if (command == "run" || command == "r") {
        return runCommand(argc, argv, argIndex, options);
    } else {
        // 可能是文件名（默认编译）
        std::string inputFile = command;
        if (inputFile.find(".zcb") == std::string::npos) {
            // 看起来像源文件
            return compileCommand(argc, argv, 1, options);
        } else {
            // 看起来像字节码文件
            return disasmCommand(argc, argv, 1);
        }
    }
    
    return 0;
}

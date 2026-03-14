#include <iostream>
#include <string>
#include <filesystem>

#include "Platform.hpp"
#include "ZCCIR.hpp"
#include "Backend.hpp"
#include "Compiler.hpp"

using namespace zcc;

void printUsage(const char* programName) {
    std::cout << "ZCC Compiler v0.2.0 - Zero-Core Command Language\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << programName << " <command> [options] <source>\n\n";
    std::cout << "Commands:\n";
    std::cout << "  compile, c    Compile source to target\n";
    std::cout << "  run, r        Compile and run\n";
    std::cout << "  ir            Generate ZCC-IR only\n";
    std::cout << "  backends      List available backends\n";
    std::cout << "  help, h       Show this help\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <file>     Output file\n";
    std::cout << "  -O0, -O1, -O2, -O3  Optimization level\n";
    std::cout << "  -g            Generate debug info\n";
    std::cout << "  --target <triple>   Target platform\n";
    std::cout << "  --backend <name>    Use specific backend\n";
    std::cout << "  --no-cache    Disable cache\n";
    std::cout << "  --emit-ir     Emit ZCC-IR file\n\n";
    std::cout << "Supported targets:\n";
    std::cout << "  x86_64-pc-windows   x86_64 Windows\n";
    std::cout << "  x86_64-pc-linux     x86_64 Linux\n";
    std::cout << "  arm64-apple-macos   ARM64 macOS\n";
    std::cout << "  wasm32-unknown-wasm WebAssembly\n";
    std::cout << "  mc-java-minecraft   Minecraft Mod\n";
    std::cout << "  i386-none-bios      BIOS\n";
    std::cout << "  micropython-embedded  MicroPython\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " compile main.zcc -o main\n";
    std::cout << "  " << programName << " compile app.zcc --target wasm32\n";
    std::cout << "  " << programName << " compile game.zcc --backend mc\n";
}

void listBackends() {
    std::cout << "Available backends:\n";
    auto& manager = BackendManager::getInstance();
    for (const auto& name : manager.getAvailableBackends()) {
        Backend* backend = manager.getBackend(name);
        if (backend) {
            std::cout << "  " << name << " - " << backend->getName();
            if (!backend->isSupported()) {
                std::cout << " (not supported on this platform)";
            }
            std::cout << "\n";
        }
    }
}

int compileCommand(int argc, char* argv[], int argIndex) {
    if (argIndex >= argc) {
        std::cerr << "Error: No source file specified\n";
        return 1;
    }
    
    std::string inputFile = argv[argIndex];
    std::string outputFile;
    std::string backendName = "c";
    std::string targetTriple;
    OptimizationLevel optLevel = OptimizationLevel::O0;
    bool debugInfo = false;
    bool emitIR = false;
    bool useCache = true;
    
    // 解析选项
    for (int i = argIndex + 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-o" && i + 1 < argc) {
            outputFile = argv[++i];
        } else if (arg == "-O0") {
            optLevel = OptimizationLevel::O0;
        } else if (arg == "-O1") {
            optLevel = OptimizationLevel::O1;
        } else if (arg == "-O2") {
            optLevel = OptimizationLevel::O2;
        } else if (arg == "-O3") {
            optLevel = OptimizationLevel::O3;
        } else if (arg == "-g") {
            debugInfo = true;
        } else if (arg == "--target" && i + 1 < argc) {
            targetTriple = argv[++i];
        } else if (arg == "--backend" && i + 1 < argc) {
            backendName = argv[++i];
        } else if (arg == "--emit-ir") {
            emitIR = true;
        } else if (arg == "--no-cache") {
            useCache = false;
        }
    }
    
    // 默认输出文件
    if (outputFile.empty()) {
        size_t dotPos = inputFile.rfind('.');
        if (dotPos != std::string::npos) {
            outputFile = inputFile.substr(0, dotPos);
        } else {
            outputFile = inputFile;
        }
    }
    
    // 初始化平台
    TargetPlatform target;
    if (!targetTriple.empty()) {
        target = TargetPlatform::parse(targetTriple);
    } else {
        target = TargetPlatform::getCurrent();
    }
    
    PlatformManager::getInstance().initialize(target);
    
    std::cout << "ZCC Compiler v0.2.0\n";
    std::cout << "Compiling " << inputFile << "...\n";
    std::cout << "Target: " << target.toString() << "\n";
    std::cout << "Backend: " << backendName << "\n";
    std::cout << "Optimization: -O" << static_cast<int>(optLevel) << "\n";
    
    // TODO: 实现完整的编译流程
    // 1. 词法分析
    // 2. 语法分析
    // 3. 语义分析
    // 4. 生成 ZCC-IR
    // 5. 优化 IR
    // 6. 后端编译
    
    std::cout << "\nCompilation successful!\n";
    std::cout << "Output: " << outputFile << "\n";
    
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "help" || command == "h" || command == "--help") {
        printUsage(argv[0]);
        return 0;
    }
    
    if (command == "backends") {
        listBackends();
        return 0;
    }
    
    if (command == "compile" || command == "c") {
        return compileCommand(argc, argv, 2);
    }
    
    if (command == "run" || command == "r") {
        std::cerr << "Run command not implemented yet\n";
        return 1;
    }
    
    if (command == "ir") {
        std::cerr << "IR generation not implemented yet\n";
        return 1;
    }
    
    std::cerr << "Unknown command: " << command << "\n";
    std::cerr << "Use '" << argv[0] << " help' for usage\n";
    return 1;
}

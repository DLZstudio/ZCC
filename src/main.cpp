#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>

#include "Platform.hpp"
#include "ZCCIR.hpp"
#include "Backend.hpp"
#include "NativeBackend.hpp"
#include "Compiler.hpp"
#include "IRGenerator.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"

using namespace zcc;

void printUsage(const char* programName) {
    std::cout << "ZCC Compiler v0.2.0 - Zero-Core Command Language\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << programName << " <command> [options] <source>\n\n";
    std::cout << "Commands:\n";
    std::cout << "  compile, c    Compile source to native executable\n";
    std::cout << "  run, r        Compile and run immediately\n";
    std::cout << "  ir            Generate ZCC-IR only\n";
    std::cout << "  exec, x       Execute .ir file directly\n";
    std::cout << "  backends      List available backends\n";
    std::cout << "  help, h       Show this help\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <file>     Output file\n";
    std::cout << "  -O0, -O1, -O2, -O3  Optimization level\n";
    std::cout << "  -g            Generate debug info\n";
    std::cout << "  --target <triple>   Target platform\n";
    std::cout << "  --backend <name>    Use specific backend\n";
    std::cout << "  --no-cache    Disable cache\n";
    std::cout << "  --keep-ir     Keep IR file after compilation\n\n";
    std::cout << "Supported backends:\n";
    std::cout << "  native        Native executable with embedded interpreter (default)\n";
    std::cout << "  windows       Windows executable via MSVC/MinGW\n";
    std::cout << "  linux         Linux executable via GCC/Clang\n";
    std::cout << "  bios          BIOS boot sector\n";
    std::cout << "  c             C source code\n\n";
    std::cout << "File formats:\n";
    std::cout << "  .zcc          ZCC source file\n";
    std::cout << "  .ir           ZCC intermediate representation\n";
    std::cout << "  .exe          Native executable (Windows)\n";
    std::cout << "  (no ext)      Native executable (Linux)\n";
    std::cout << "  .bin          BIOS binary\n";
    std::cout << "  .c            C source\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " compile main.zcc -o main\n";
    std::cout << "  " << programName << " run app.zcc\n";
    std::cout << "  " << programName << " compile boot.zcc --backend bios -o boot.bin\n";
    std::cout << "  " << programName << " exec program.ir\n";
    std::cout << "  " << programName << " compile lib.zcc --keep-ir  (generates lib.exe + lib.ir)\n";
}

void listBackends() {
    std::cout << "Available backends:\n";
    auto& manager = BackendManager::getInstance();
    
    // 注册所有后端
    manager.registerBackend(std::make_unique<NativeBackend>());
    manager.registerBackend(std::make_unique<BIOSBackend>());
    manager.registerBackend(std::make_unique<CBackend>());
    
    for (const auto& name : manager.getAvailableBackends()) {
        Backend* backend = manager.getBackend(name);
        if (backend) {
            std::cout << "  " << name << " - " << backend->getDescription();
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
    std::string backendName = "native";  // 默认使用原生后端
    std::string targetTriple;
    OptimizationLevel optLevel = OptimizationLevel::O1;
    bool debugInfo = false;
    bool keepIR = true;  // 默认保留 IR 文件
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
        } else if (arg == "--keep-ir") {
            keepIR = true;
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
    std::cout << "Backend: " << backendName << "\n";
    std::cout << "Target: " << target.toString() << "\n";
    std::cout << "Optimization: -O" << static_cast<int>(optLevel) << "\n";
    std::cout << "Keep IR: " << (keepIR ? "yes" : "no") << "\n";
    
    // 创建编译器
    CompilerOptions options;
    options.outputFilename = outputFile;
    options.optimization = optLevel;
    options.debugInfo = debugInfo;
    options.useCache = useCache;
    options.backend = backendName;
    
    Compiler compiler(options);
    auto result = compiler.compileFile(inputFile);
    
    if (!result.success) {
        std::cerr << "\nCompilation failed!\n";
        for (const auto& error : result.errors) {
            std::cerr << "  " << error << "\n";
        }
        for (const auto& diag : result.diagnostics) {
            std::cerr << "  " << diag.message << " (line " << diag.location.line << ")\n";
        }
        return 1;
    }
    
    std::cout << "\nCompilation successful!\n";
    std::cout << "Output: " << outputFile << result.output << "\n";
    std::cout << "Compile time: " << result.compileTimeMs << "ms\n";
    
    if (keepIR && backendName == "native") {
        std::cout << "IR file: " << outputFile << ".ir\n";
    }
    
    return 0;
}

int runCommand(int argc, char* argv[], int argIndex) {
    if (argIndex >= argc) {
        std::cerr << "Error: No source file specified\n";
        return 1;
    }
    
    std::string inputFile = argv[argIndex];
    
    // 检查文件扩展名
    bool isIR = inputFile.size() > 3 && inputFile.substr(inputFile.size() - 3) == ".ir";
    
    if (isIR) {
        // 直接运行 IR 文件
        std::cout << "Running IR file: " << inputFile << "\n";
        
        // TODO: 加载并运行 IR
        std::cerr << "Error: Running .ir files directly is not implemented yet\n";
        return 1;
    }
    
    // 编译并运行
    std::cout << "Compiling and running: " << inputFile << "\n";
    
    // 临时输出文件
    std::string tempOutput = inputFile + ".tmp";
    
    // 编译
    CompilerOptions options;
    options.outputFilename = tempOutput;
    options.optimization = OptimizationLevel::O1;
    options.backend = "native";
    options.useCache = false;
    
    Compiler compiler(options);
    auto compileResult = compiler.compileFile(inputFile);
    
    if (!compileResult.success) {
        std::cerr << "\nCompilation failed!\n";
        for (const auto& error : compileResult.errors) {
            std::cerr << error << "\n";
        }
        return 1;
    }
    
    std::cout << "Compilation successful, running...\n\n";
    
    // TODO: 运行编译后的程序
    std::cerr << "Error: Running compiled programs is not implemented yet\n";
    
    // 清理临时文件
    std::filesystem::remove(tempOutput);
    std::filesystem::remove(tempOutput + ".ir");
    
    return 0;
}

int execCommand(int argc, char* argv[], int argIndex) {
    if (argIndex >= argc) {
        std::cerr << "Error: No IR file specified\n";
        return 1;
    }
    
    std::string irFile = argv[argIndex];
    
    std::cout << "Executing IR file: " << irFile << "\n";
    
    // TODO: 加载并执行 IR
    std::cerr << "Error: Executing .ir files is not implemented yet\n";
    return 1;
}

int irCommand(int argc, char* argv[], int argIndex) {
    if (argIndex >= argc) {
        std::cerr << "Error: No source file specified\n";
        return 1;
    }
    
    std::string inputFile = argv[argIndex];
    std::string outputFile;
    
    // 解析选项
    for (int i = argIndex + 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outputFile = argv[++i];
        }
    }
    
    if (outputFile.empty()) {
        size_t dotPos = inputFile.rfind('.');
        if (dotPos != std::string::npos) {
            outputFile = inputFile.substr(0, dotPos) + ".ir";
        } else {
            outputFile = inputFile + ".ir";
        }
    }
    
    std::cout << "Generating IR: " << inputFile << " -> " << outputFile << "\n";
    
    // 读取源文件
    std::ifstream file(inputFile, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file: " << inputFile << "\n";
        return 1;
    }
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    // 词法分析
    Lexer lexer(source, inputFile);
    auto tokens = lexer.tokenize();
    if (lexer.hasErrors()) {
        std::cerr << "Lexer errors:\n";
        for (auto& e : lexer.getErrors()) std::cerr << "  " << e.message << "\n";
        return 1;
    }
    
    // 语法分析
    Parser parser(tokens, inputFile);
    auto program = parser.parse();
    if (parser.hasErrors()) {
        std::cerr << "Parser errors:\n";
        for (auto& e : parser.getErrors()) std::cerr << "  " << e.message << "\n";
        return 1;
    }
    
    // IR 生成
    IRGeneratorConfig irConfig;
    irConfig.moduleName = inputFile;
    irConfig.target = TargetPlatform::getCurrent();
    IRGenerator irGen(irConfig);
    auto irModule = irGen.generate(program);
    
    if (irGen.hasErrors()) {
        std::cerr << "IR generation errors:\n";
        for (auto& e : irGen.getErrors()) std::cerr << "  " << e.message << "\n";
        return 1;
    }
    
    // 输出 IR 文本
    std::string irText = irModule->toText();
    std::ofstream outFile(outputFile);
    if (!outFile) {
        std::cerr << "Error: Cannot write to: " << outputFile << "\n";
        return 1;
    }
    outFile << irText;
    outFile.close();
    
    std::cout << "IR generated successfully: " << outputFile << "\n";
    std::cout << "IR size: " << irText.size() << " bytes\n";
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
        return runCommand(argc, argv, 2);
    }
    
    if (command == "exec" || command == "x") {
        return execCommand(argc, argv, 2);
    }
    
    if (command == "ir") {
        return irCommand(argc, argv, 2);
    }
    
    std::cerr << "Unknown command: " << command << "\n";
    std::cerr << "Use '" << argv[0] << " help' for usage\n";
    return 1;
}

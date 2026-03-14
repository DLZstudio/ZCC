#include "Backend.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace zcc {

// ==================== Windows 后端实现 ====================

CompileResult WindowsBackend::compile(const ZCCIRModule& module,
                                      const CompileOptions& options) {
    return compileViaC(module, options);
}

CompileResult WindowsBackend::compileViaC(const ZCCIRModule& module,
                                          const CompileOptions& options) {
    CompileResult result;
    result.success = true;
    
    try {
        // 1. 使用 C 后端生成 C 代码
        CBackend cBackend;
        auto cResult = cBackend.compile(module, options);
        
        if (!cResult.success) {
            return cResult;
        }
        
        // 2. 保存 C 代码到临时文件
        std::filesystem::path tempDir = std::filesystem::temp_directory_path();
        std::string cFile = (tempDir / "zcc_temp.c").string();
        std::string exeFile = options.outputPath.empty() ? "output.exe" : options.outputPath;
        
        std::ofstream outFile(cFile);
        outFile << cResult.output;
        outFile.close();
        
        // 3. 调用 MSVC 或 GCC 编译
        std::string compiler;
        std::string compileCmd;
        
        // 检测编译器
        if (std::system("cl.exe >nul 2>&1") == 0) {
            // MSVC
            compiler = "cl.exe";
            compileCmd = "cl.exe /O2 /Fe:" + exeFile + " " + cFile;
        } else if (std::system("g++ --version >nul 2>&1") == 0) {
            // MinGW
            compiler = "g++.exe";
            compileCmd = "g++.exe -O2 -o " + exeFile + " " + cFile;
        } else {
            result.success = false;
            result.errorMessage = "No C compiler found (need cl.exe or g++.exe)";
            return result;
        }
        
        // 4. 执行编译命令
        int ret = std::system(compileCmd.c_str());
        
        // 5. 清理临时文件
        std::filesystem::remove(cFile);
        
        if (ret != 0) {
            result.success = false;
            result.errorMessage = "C compiler failed with code " + std::to_string(ret);
            return result;
        }
        
        // 6. 读取生成的可执行文件
        std::ifstream exeFileIn(exeFile, std::ios::binary);
        result.binary.assign(
            std::istreambuf_iterator<char>(exeFileIn),
            std::istreambuf_iterator<char>()
        );
        exeFileIn.close();
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }
    
    return result;
}

// ==================== Linux 后端实现 ====================

CompileResult LinuxBackend::compile(const ZCCIRModule& module,
                                    const CompileOptions& options) {
    return compileViaC(module, options);
}

CompileResult LinuxBackend::compileViaC(const ZCCIRModule& module,
                                        const CompileOptions& options) {
    CompileResult result;
    result.success = true;
    
    try {
        // 1. 使用 C 后端生成 C 代码
        CBackend cBackend;
        auto cResult = cBackend.compile(module, options);
        
        if (!cResult.success) {
            return cResult;
        }
        
        // 2. 保存 C 代码到临时文件
        std::filesystem::path tempDir = std::filesystem::temp_directory_path();
        std::string cFile = (tempDir / "zcc_temp.c").string();
        std::string exeFile = options.outputPath.empty() ? "output" : options.outputPath;
        
        std::ofstream outFile(cFile);
        outFile << cResult.output;
        outFile.close();
        
        // 3. 调用 GCC 或 Clang 编译
        std::string compiler;
        std::string compileCmd;
        
        // 检测编译器
        if (std::system("which clang > /dev/null 2>&1") == 0) {
            // Clang
            compiler = "clang";
            compileCmd = "clang -O2 -o " + exeFile + " " + cFile;
        } else if (std::system("which gcc > /dev/null 2>&1") == 0) {
            // GCC
            compiler = "gcc";
            compileCmd = "gcc -O2 -o " + exeFile + " " + cFile;
        } else {
            result.success = false;
            result.errorMessage = "No C compiler found (need gcc or clang)";
            return result;
        }
        
        // 4. 执行编译命令
        int ret = std::system(compileCmd.c_str());
        
        // 5. 清理临时文件
        std::filesystem::remove(cFile);
        
        if (ret != 0) {
            result.success = false;
            result.errorMessage = "C compiler failed with code " + std::to_string(ret);
            return result;
        }
        
        // 6. 读取生成的可执行文件
        std::ifstream exeFileIn(exeFile, std::ios::binary);
        result.binary.assign(
            std::istreambuf_iterator<char>(exeFileIn),
            std::istreambuf_iterator<char>()
        );
        exeFileIn.close();
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }
    
    return result;
}

} // namespace zcc

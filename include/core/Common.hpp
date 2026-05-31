#ifndef ZCC_COMMON_HPP
#define ZCC_COMMON_HPP

#include <string>
#include <vector>
#include <cstdint>
#include "Diagnostics.hpp"

namespace zcc {

// 编译优化级别
enum class OptimizationLevel {
    O0,  // 无优化
    O1,  // 基础优化
    O2,  // 中级优化
    O3,  // 高级优化
    Os,  // 大小优化
    Oz,  // 极致大小优化
};

// 编译选项
struct CompileOptions {
    OptimizationLevel optimization = OptimizationLevel::O0;
    bool debugInfo = false;
    bool emitAssembly = false;
    std::vector<std::string> includePaths;
    std::vector<std::string> defines;
    std::string outputPath;
    std::string targetTriple;
};

// 编译结果
struct CompileResult {
    bool success;
    std::string output;
    std::vector<uint8_t> binary;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::string errorMessage;
    std::vector<struct Diagnostic> diagnostics;
    uint64_t compileTimeMs = 0;

    bool hasErrors() const { return !errors.empty() || !diagnostics.empty(); }
    bool hasWarnings() const { return !warnings.empty(); }
};

} // namespace zcc

#endif // ZCC_COMMON_HPP

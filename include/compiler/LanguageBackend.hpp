// ZCC 语言后端插件协议
// 实现此接口即可添加新的目标语言支持 (Python/Java/Go/Rust/...)
// 无需修改 ZCC 编译器核心代码

#ifndef ZCC_LANGUAGE_BACKEND_HPP
#define ZCC_LANGUAGE_BACKEND_HPP

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include "ZCCIR.hpp"
#include "Common.hpp"

namespace zcc {

// ==================== 外部编译器配置 ====================

// 描述如何调用外部编译器
struct CompilerConfig {
    // 编译器可执行文件路径 (如 "gcc", "g++", "python3", "javac", "go")
    std::string compiler;
    
    // 编译命令模板
    // 可用占位符: {input} = 源文件, {output} = 输出文件, {flags} = 编译选项
    // 示例: "gcc {input} -o {output} -O2 -lm -lzcc_runtime"
    std::string compileTemplate;
    
    // 额外的编译标志
    std::vector<std::string> extraFlags;
    
    // 编译器是否为解释器 (Python/Node.js 等直接运行源码)
    bool isInterpreter;
    
    // 构建编译命令行
    std::string buildCommand(const std::string& inputFile,
                             const std::string& outputFile,
                             const std::vector<std::string>& runtimeFiles,
                             int optLevel = 1) const;
};

// ==================== 语言后端插件接口 ====================

class ILanguageBackend {
public:
    virtual ~ILanguageBackend() = default;
    
    // ---- 基本信息 ----
    
    // 目标语言名称: "C", "Python", "Java", "Go", "Rust", "JavaScript", ...
    virtual std::string getLanguageName() const = 0;
    
    // 源文件扩展名: ".c", ".py", ".java", ".go", ".rs", ".js", ...
    virtual std::string getFileExtension() const = 0;
    
    // 简短描述
    virtual std::string getDescription() const = 0;
    
    // ---- IR 翻译 ----
    
    // IR → 目标语言源码
    virtual std::string generateSource(const ZCCIRModule& ir,
                                        const CompileOptions& options) = 0;
    
    // 生成项目文件 (多文件输出时使用)
    virtual std::vector<std::pair<std::string, std::string>> generateProject(
        const ZCCIRModule& ir, const CompileOptions& options) {
        // 默认: 单文件输出 main.xxx
        std::string ext = getFileExtension();
        return {{"main" + ext, generateSource(ir, options)}};
    }
    
    // ---- 外部编译 ----
    
    // 获取编译器配置 (返回 nullopt 表示语言是解释型的,不需要编译步骤)
    virtual std::optional<CompilerConfig> getCompilerConfig() const {
        return std::nullopt;
    }
    
    // 运行时库源文件 (如 zcc_runtime.c, zcc_runtime.py)
    // 这些文件会和生成的源码一起编译/打包
    virtual std::vector<std::string> getRuntimeSourceFiles() const {
        return {};
    }
    
    // 是否需要运行时头文件 (C/C++ 等编译型语言需要)
    virtual std::vector<std::string> getRuntimeHeaderFiles() const {
        return {};
    }
    
    // ---- 编译产物 ----
    
    // 可执行文件扩展名 (Windows: ".exe", Linux: "", Python: ".py")
    virtual std::string getExecutableExtension() const {
#ifdef _WIN32
        return ".exe";
#else
        return "";
#endif
    }
    
    // 是否需要编译步骤 (编译型语言需要, 解释型不需要)
    virtual bool needsCompilation() const { return true; }
};

// ==================== 默认编译器命令生成 ====================

inline std::string CompilerConfig::buildCommand(
    const std::string& inputFile,
    const std::string& outputFile,
    const std::vector<std::string>& runtimeFiles,
    int optLevel) const {
    
    std::string cmd = compileTemplate;
    
    // 替换占位符
    auto replace = [&cmd](const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = cmd.find(from, pos)) != std::string::npos) {
            cmd.replace(pos, from.length(), to);
            pos += to.length();
        }
    };
    
    // 构建输入文件列表 (源文件 + 运行时文件)
    std::string inputs = inputFile;
    for (const auto& f : runtimeFiles) {
        inputs += " " + f;
    }
    
    replace("{input}", inputs);
    replace("{output}", outputFile);
    replace("{flags}", std::to_string(optLevel));
    
    return cmd;
}

// ==================== 语言后端注册表 ====================

class LanguageBackendRegistry {
public:
    static LanguageBackendRegistry& instance();
    
    // 注册语言后端
    void registerBackend(std::unique_ptr<ILanguageBackend> backend);
    
    // 获取后端
    ILanguageBackend* getBackend(const std::string& languageName);
    
    // 列出所有注册的语言
    std::vector<std::string> getAvailableLanguages() const;
    
    // 根据文件扩展名推断语言
    ILanguageBackend* findByExtension(const std::string& filename);
    
    // 默认语言
    ILanguageBackend* getDefault() const;
    
private:
    LanguageBackendRegistry() = default;
    std::unordered_map<std::string, std::unique_ptr<ILanguageBackend>> backends_;
    std::string defaultLanguage_ = "c";
};

// 初始化所有内置语言后端
void initializeBuiltinBackends();

} // namespace zcc

#endif // ZCC_LANGUAGE_BACKEND_HPP

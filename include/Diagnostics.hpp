#ifndef ZCC_DIAGNOSTICS_HPP
#define ZCC_DIAGNOSTICS_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace zcc {

// 诊断信息严重级别
enum class DiagnosticSeverity {
    Error,
    Warning,
    Info,
    Hint
};

// 诊断信息位置
struct SourceLocation {
    std::string filename;
    size_t line;
    size_t column;
    size_t length;
    
    SourceLocation(std::string filename = "<unknown>", 
                   size_t line = 0, size_t column = 0, size_t length = 0)
        : filename(std::move(filename)), line(line), column(column), length(length) {}
};

// 诊断信息类
class Diagnostic {
public:
    DiagnosticSeverity severity;
    std::string message;
    SourceLocation location;
    std::string context;  // 上下文代码
    
    Diagnostic(DiagnosticSeverity severity, std::string message, 
               SourceLocation location, std::string context = "")
        : severity(severity), message(std::move(message)), 
          location(std::move(location)), context(std::move(context)) {}
    
    // 创建错误
    static Diagnostic error(const std::string& message, const SourceLocation& loc) {
        return Diagnostic(DiagnosticSeverity::Error, message, loc);
    }
    
    // 创建警告
    static Diagnostic warning(const std::string& message, const SourceLocation& loc) {
        return Diagnostic(DiagnosticSeverity::Warning, message, loc);
    }
    
    // 创建信息
    static Diagnostic info(const std::string& message, const SourceLocation& loc) {
        return Diagnostic(DiagnosticSeverity::Info, message, loc);
    }
    
    // 获取严重级别字符串
    std::string severityString() const {
        switch (severity) {
            case DiagnosticSeverity::Error: return "错误";
            case DiagnosticSeverity::Warning: return "警告";
            case DiagnosticSeverity::Info: return "信息";
            case DiagnosticSeverity::Hint: return "提示";
            default: return "未知";
        }
    }
    
    // 格式化为字符串
    std::string toString() const {
        std::string result;
        result += location.filename + ":";
        result += std::to_string(location.line) + ":";
        result += std::to_string(location.column) + ": ";
        result += severityString() + ": ";
        result += message;
        if (!context.empty()) {
            result += "\n  " + context;
        }
        return result;
    }
};

// 诊断信息引擎
class DiagnosticEngine {
public:
    void addDiagnostic(const Diagnostic& diagnostic) {
        diagnostics_.push_back(diagnostic);
    }
    
    void addError(const std::string& message, const SourceLocation& loc) {
        diagnostics_.push_back(Diagnostic::error(message, loc));
    }
    
    void addWarning(const std::string& message, const SourceLocation& loc) {
        diagnostics_.push_back(Diagnostic::warning(message, loc));
    }
    
    const std::vector<Diagnostic>& getDiagnostics() const {
        return diagnostics_;
    }
    
    bool hasErrors() const {
        for (const auto& diag : diagnostics_) {
            if (diag.severity == DiagnosticSeverity::Error) {
                return true;
            }
        }
        return false;
    }
    
    void clear() {
        diagnostics_.clear();
    }
    
    // 打印所有诊断信息
    void printAll() const {
        for (const auto& diag : diagnostics_) {
            printf("%s\n", diag.toString().c_str());
        }
    }

private:
    std::vector<Diagnostic> diagnostics_;
};

} // namespace zcc

#endif // ZCC_DIAGNOSTICS_HPP

#ifndef ZCC_MODULE_HPP
#define ZCC_MODULE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <fstream>
#include <regex>
#include <atomic>
#include <queue>
#include <set>
#include <variant>
#include <mutex>
#include <cstdio>

namespace zcc {

// ============== 基础类型 ==============

// 模块类型
enum class ModuleType {
    INSTRUCTION_SET,  // .sca 指令集文件
    EXECUTABLE,       // .exe 可执行文件
    DYNAMIC_LIB,      // .dll/.so 动态库
    SCRIPT,           // .js/.py/.lua 脚本
    DATA_FILE,        // .txt/.json 数据文件
    WASM,             // .wasm WebAssembly
    UNKNOWN           // 未知类型
};

// 依赖类型
enum class DependencyType {
    MODULE,           // 模块依赖
    EXECUTABLE,       // 可执行文件依赖
    LIBRARY,          // 库依赖
    SCRIPT,           // 脚本依赖
};

// 沙箱策略
enum class SandboxPolicy {
    NONE,             // 无沙箱
    RESTRICTED,       // 限制模式
    SANDBOXED,        // 完全沙箱
};

// ============== 符号与依赖 ==============

// 导出符号
struct ExportedSymbol {
    std::string name;
    std::string type;  // function, variable, class, etc.
    std::string signature;  // 函数签名
    bool is_public = true;
    std::string documentation;  // 文档注释
    
    ExportedSymbol() = default;
    ExportedSymbol(const std::string& n, const std::string& t, const std::string& sig = "")
        : name(n), type(t), signature(sig) {}
};

// 未解析符号（虚空接口）
struct UnresolvedSymbol {
    std::string name;
    std::string source_file;
    size_t source_line;
    std::string expected_source;  // 预期来源（可选）
    DependencyType dep_type = DependencyType::MODULE;
    
    UnresolvedSymbol() : source_line(0) {}
    UnresolvedSymbol(const std::string& n, const std::string& src, size_t line, 
                    DependencyType dt = DependencyType::MODULE)
        : name(n), source_file(src), source_line(line), dep_type(dt) {}
};

// 模块依赖
struct ModuleDependency {
    std::string name;
    std::filesystem::path path;
    bool is_optional = false;
    std::string version_constraint;
    DependencyType type = DependencyType::MODULE;
    
    ModuleDependency() = default;
    ModuleDependency(const std::string& n, const std::filesystem::path& p, 
                    bool opt = false, const std::string& ver = "")
        : name(n), path(p), is_optional(opt), version_constraint(ver) {}
};

// ============== 模块元数据 ==============

// 模块元数据
struct ModuleMetadata {
    std::string name;
    std::string version;
    std::string source_path;
    std::string description;
    ModuleType type;
    std::string target_platform;  // windows, linux, macos, all
    
    std::vector<ExportedSymbol> exports;
    std::vector<UnresolvedSymbol> imports;  // 虚空接口
    std::vector<ModuleDependency> dependencies;
    
    std::map<std::string, std::string> attributes;
    std::vector<std::string> tags;
    
    // 安全信息
    bool is_signed = false;
    std::string signer;
    std::vector<std::string> required_permissions;
    
    ModuleMetadata() : type(ModuleType::UNKNOWN) {}
};

// 加载选项
struct LoadOptions {
    bool lazy_load = false;          // 延迟加载（仅解析元数据）
    bool resolve_virtual = true;     // 尝试解析"虚空接口"
    std::string target_platform;     // 目标平台过滤
    SandboxPolicy sandbox_policy = SandboxPolicy::NONE;  // 沙箱安全策略
    bool cache_enabled = true;       // 启用缓存
    int priority = 50;               // 优先级
};

// 验证结果
struct ValidationResult {
    bool is_valid = true;
    std::vector<std::string> errors;
    std::shared_ptr<ModuleMetadata> metadata;
    std::vector<std::string> warnings;
    std::vector<std::string> info;
};

// ============== 代码块（用于.sca 文件） ==============

// 代码语言类型
enum class CodeLanguage {
    ZCC,
    IR,
    C,
    CPP,
    ASM,
    JAVASCRIPT,
    PYTHON,
    LUA,
    UNKNOWN
};

// 代码块
struct CodeBlock {
    std::string language;
    std::string code;
    size_t source_line;
    std::map<std::string, std::string> attributes;
    CodeLanguage lang_type = CodeLanguage::UNKNOWN;
    
    CodeBlock() : source_line(0) {}
};

// ============== SCA v0.3 Manifest 系统 ==============

// Manifest 中的依赖声明
struct SCAManifestDependency {
    std::string name;                // 依赖名 (如 org.std.crypto)
    std::string type = "sca";       // 类型: sca / executable
    std::string version;             // 版本约束 (如 >=1.5.0)
    std::string min_version;         // 最低版本
    std::string ipc_method;          // IPC 方式 (executable 类型用)
    std::string version_cmd;         // 版本检查命令 (executable 类型用)
};

// Manifest 中的接口映射
struct SCAManifestInterface {
    std::string symbol;              // 虚空接口符号名 (如 hash)
    std::string provider;            // 提供者 (依赖名)
    std::string export_name;         // 提供者中的导出名
    std::string endpoint;            // 端点 (executable 类型用)
};

// SCA v0.3 Manifest — 模块清单
struct SCAManifest {
    // 模块自身信息
    std::string name;                // module.name
    std::string version = "0.0.0";  // module.version
    std::string sca_version = "0.3"; // module.sca_version
    std::vector<std::string> modes;  // module.modes = compile,ir-exec,...
    
    // 依赖列表
    std::vector<SCAManifestDependency> dependencies;
    
    // 接口映射 (替代正则猜测的虚空接口)
    std::vector<SCAManifestInterface> interfaces;
    
    // 控制字段
    bool has_manifest = false;       // 文件是否包含 manifest 段
    bool native_only = true;         // v0.3 铁律: 只允许 zcc + ir 块
    
    // 从 modes 推断的能力
    bool supportsCompile()  const { return hasMode("compile"); }
    bool supportsIRExec()   const { return hasMode("ir-exec"); }
    
private:
    bool hasMode(const std::string& m) const {
        for (auto& mode : modes)
            if (mode == m) return true;
        return false;
    }
};

// ============== SemVer 工具 ==============

struct SemVer {
    int major = 0, minor = 0, patch = 0;
    
    static SemVer parse(const std::string& s) {
        SemVer v;
        sscanf(s.c_str(), "%d.%d.%d", &v.major, &v.minor, &v.patch);
        return v;
    }
    
    bool operator<(const SemVer& o) const {
        if (major != o.major) return major < o.major;
        if (minor != o.minor) return minor < o.minor;
        return patch < o.patch;
    }
    bool operator==(const SemVer& o) const {
        return major == o.major && minor == o.minor && patch == o.patch;
    }
    bool operator<=(const SemVer& o) const { return *this < o || *this == o; }
    bool operator>(const SemVer& o) const { return o < *this; }
    bool operator>=(const SemVer& o) const { return o < *this || *this == o; }
};

// 检查版本约束: ">=1.5.0", ">1.0.0", "==2.0.0"
inline bool checkVersionConstraint(const std::string& constraint, const std::string& actual) {
    if (constraint.empty() || actual.empty()) return true;  // 无约束或无版本 → 视为通过
    
    auto v = SemVer::parse(actual);
    
    if (constraint.size() >= 2 && constraint[0] == '>' && constraint[1] == '=') {
        return v >= SemVer::parse(constraint.substr(2));
    } else if (constraint[0] == '>') {
        return v > SemVer::parse(constraint.substr(1));
    } else if (constraint.size() >= 2 && constraint[0] == '=' && constraint[1] == '=') {
        return v == SemVer::parse(constraint.substr(2));
    } else {
        // 无操作符 → 视为最低版本 >=
        return v >= SemVer::parse(constraint);
    }
}

// ============== SCA 模块 ==============

struct SCAModule {
    std::string source_name;
    std::vector<CodeBlock> blocks;
    std::shared_ptr<ModuleMetadata> metadata;
    std::string cache_key;
    SCAManifest manifest;  // SCA v0.3 Manifest
    
    SCAModule() : metadata(std::make_shared<ModuleMetadata>()) {}
};

// ============== 其他模块类型 ==============

struct ExecutableModule {
    std::filesystem::path path;
    void* ipc_connection = nullptr;
    std::vector<ExportedSymbol> exported_functions;
    bool is_running = false;
};

struct DynamicLibModule {
    std::filesystem::path path;
    void* handle = nullptr;
    std::vector<ExportedSymbol> exported_symbols;
};

struct ScriptModule {
    std::string language;
    std::string source_code;
    void* engine = nullptr;  // 脚本引擎
    std::vector<ExportedSymbol> exports;
};

// 通用模块
struct Module {
    std::filesystem::path path;
    ModuleType type;
    std::shared_ptr<ModuleMetadata> metadata;
    bool is_loaded = false;
    
    Module() : type(ModuleType::UNKNOWN) {}
    Module(const std::filesystem::path& p, ModuleType t) 
        : path(p), type(t) {}
    virtual ~Module() = default;
};

// ============== 加载结果 ==============
// (必须放在 SCAModule/ExecutableModule/DynamicLibModule/ScriptModule 之后,
//  因为 std::variant 要求完整类型)

struct LoadResult {
    bool success;
    std::unique_ptr<Module> module;
    std::vector<UnresolvedSymbol> unresolved_symbols;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    std::shared_ptr<ModuleMetadata> metadata;
    
    std::variant<std::monostate, SCAModule, ExecutableModule, 
                DynamicLibModule, ScriptModule> specific_data;
    
    LoadResult() : success(false) {}
    
    static LoadResult create_success(std::unique_ptr<Module> mod) {
        LoadResult result;
        result.success = true;
        result.module = std::move(mod);
        return result;
    }
    
    static LoadResult create_failure(const std::string& error) {
        LoadResult result;
        result.success = false;
        result.errors.push_back(error);
        return result;
    }
};

// ============== 扩展模块加载器基类 ==============

class ExtendedModuleLoader {
public:
    virtual ~ExtendedModuleLoader() = default;
    
    // 1. 能力检测
    virtual bool supports(const std::filesystem::path& path) = 0;
    virtual int get_priority() const { return 50; }  // 优先级，用于解决冲突
    
    // 2. 核心加载
    virtual LoadResult load(const std::filesystem::path& path, 
                           const LoadOptions& options = {}) = 0;
    
    // 3. 元数据提取
    virtual ModuleMetadata get_metadata(const std::filesystem::path& path) = 0;
    
    // 4. 符号解析（关键：用于"虚空接口"）
    virtual std::vector<ExportedSymbol> get_exports(const std::filesystem::path& path) = 0;
    virtual std::vector<UnresolvedSymbol> get_imports(const std::filesystem::path& path) = 0;
    
    // 5. 依赖分析
    virtual std::vector<ModuleDependency> analyze_dependencies(
        const std::filesystem::path& path) = 0;
    
    // 6. 验证与安全
    virtual ValidationResult validate(const std::filesystem::path& path) = 0;
    
    // 7. 缓存支持
    virtual std::string calculate_cache_key(const std::filesystem::path& path) = 0;
    
    // 8. 资源清理
    virtual void unload(const std::filesystem::path& path) = 0;
    
    // 获取加载器类型
    virtual ModuleType getModuleType() = 0;
};

// ============== 文件类型检测器 ==============

// 魔法数字条目
struct MagicNumberEntry {
    std::vector<uint8_t> magic;
    ExtendedModuleLoader* loader;
    
    MagicNumberEntry(const std::vector<uint8_t>& m, ExtendedModuleLoader* l) 
        : magic(m), loader(l) {}
};

class FileTypeDetector {
private:
    static std::unordered_map<std::string, ExtendedModuleLoader*> loaders_by_ext;
    static std::vector<MagicNumberEntry> magic_numbers;
    
public:
    // 注册加载器
    static void register_loader(const std::string& extension, ExtendedModuleLoader* loader);
    
    // 注册魔法数字
    static void register_magic(const std::vector<uint8_t>& magic, ExtendedModuleLoader* loader);
    
    // 获取对应加载器
    static ExtendedModuleLoader* get_loader(const std::filesystem::path& path);
    
    // 从文件内容检测
    static ExtendedModuleLoader* detect_by_content(const std::filesystem::path& path);
    
    // 检测模块类型
    static ModuleType detectModuleType(const std::filesystem::path& path) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == ".sca") {
            return ModuleType::INSTRUCTION_SET;
        } else if (ext == ".exe") {
            return ModuleType::EXECUTABLE;
        } else if (ext == ".dll") {
            return ModuleType::DYNAMIC_LIB;
        } else if (ext == ".so" || ext == ".dylib") {
            return ModuleType::DYNAMIC_LIB;
        } else if (ext == ".js" || ext == ".py" || ext == ".lua") {
            return ModuleType::SCRIPT;
        } else if (ext == ".wasm") {
            return ModuleType::WASM;
        } else if (ext.empty()) {
            return ModuleType::EXECUTABLE;  // Unix 可执行文件
        } else {
            return ModuleType::DATA_FILE;
        }
    }
};

// ============== 具体加载器实现 ==============

// 智能 SCA 指令集加载器
class SmartSCALoader : public ExtendedModuleLoader {
private:
    std::shared_ptr<SCAModule> module_;
    
public:
    bool supports(const std::filesystem::path& path) override;
    
    LoadResult load(const std::filesystem::path& path, 
                   const LoadOptions& options = {}) override;
    
    ModuleMetadata get_metadata(const std::filesystem::path& path) override;
    
    std::vector<ExportedSymbol> get_exports(const std::filesystem::path& path) override;
    
    std::vector<UnresolvedSymbol> get_imports(const std::filesystem::path& path) override;
    
    std::vector<ModuleDependency> analyze_dependencies(
        const std::filesystem::path& path) override;
    
    ValidationResult validate(const std::filesystem::path& path) override;
    
    std::string calculate_cache_key(const std::filesystem::path& path) override;
    
    void unload(const std::filesystem::path& path) override;
    
    ModuleType getModuleType() override {
        return ModuleType::INSTRUCTION_SET;
    }
    
    int get_priority() const override { return 100; }  // 高优先级
    
private:
    SCAModule parse_sca_file(const std::filesystem::path& path);
    SCAManifest parse_manifest(const std::string& content);  // SCA v0.3
    void process_zcc_block(const CodeBlock& block, LoadResult& result);
    void process_ir_block(const CodeBlock& block, LoadResult& result);
    void process_native_block(const CodeBlock& block, LoadResult& result);
    std::vector<UnresolvedSymbol> find_unresolved_symbols(const SCAModule& module);
    std::string trim(const std::string& str);
};

// 外部可执行文件加载器
class ExternalExecutableLoader : public ExtendedModuleLoader {
private:
    std::shared_ptr<Module> module_;
    
public:
    bool supports(const std::filesystem::path& path) override;
    
    LoadResult load(const std::filesystem::path& path, 
                   const LoadOptions& options = {}) override;
    
    ModuleMetadata get_metadata(const std::filesystem::path& path) override;
    
    std::vector<ExportedSymbol> get_exports(const std::filesystem::path& path) override;
    
    std::vector<UnresolvedSymbol> get_imports(const std::filesystem::path& path) override;
    
    std::vector<ModuleDependency> analyze_dependencies(
        const std::filesystem::path& path) override;
    
    ValidationResult validate(const std::filesystem::path& path) override;
    
    std::string calculate_cache_key(const std::filesystem::path& path) override;
    
    void unload(const std::filesystem::path& path) override;
    
    ModuleType getModuleType() override {
        return ModuleType::EXECUTABLE;
    }
    
    int get_priority() const override { return 80; }
    
private:
    bool is_process_running(const std::filesystem::path& path);
    bool start_process(const std::filesystem::path& path);
    void* establish_ipc_connection(const std::filesystem::path& path);
    std::vector<ExportedSymbol> query_exports_via_ipc(void* conn);
    std::vector<ExportedSymbol> read_export_table(const std::filesystem::path& path);
    bool verify_digital_signature(const std::filesystem::path& path);
    bool is_potentially_malicious(const std::filesystem::path& path);
    void wait_for_ipc_ready(const std::filesystem::path& path, int timeout_ms);
};

// 动态库加载器
class DynamicLibLoader : public ExtendedModuleLoader {
private:
    std::shared_ptr<Module> module_;
    
public:
    bool supports(const std::filesystem::path& path) override;
    
    LoadResult load(const std::filesystem::path& path, 
                   const LoadOptions& options = {}) override;
    
    ModuleMetadata get_metadata(const std::filesystem::path& path) override;
    
    std::vector<ExportedSymbol> get_exports(const std::filesystem::path& path) override;
    
    std::vector<UnresolvedSymbol> get_imports(const std::filesystem::path& path) override;
    
    std::vector<ModuleDependency> analyze_dependencies(
        const std::filesystem::path& path) override;
    
    ValidationResult validate(const std::filesystem::path& path) override;
    
    std::string calculate_cache_key(const std::filesystem::path& path) override;
    
    void unload(const std::filesystem::path& path) override;
    
    ModuleType getModuleType() override {
        return ModuleType::DYNAMIC_LIB;
    }
    
    int get_priority() const override { return 70; }
};

// 脚本加载器
class ScriptLoader : public ExtendedModuleLoader {
private:
    std::shared_ptr<Module> module_;
    
public:
    bool supports(const std::filesystem::path& path) override;
    
    LoadResult load(const std::filesystem::path& path, 
                   const LoadOptions& options = {}) override;
    
    ModuleMetadata get_metadata(const std::filesystem::path& path) override;
    
    std::vector<ExportedSymbol> get_exports(const std::filesystem::path& path) override;
    
    std::vector<UnresolvedSymbol> get_imports(const std::filesystem::path& path) override;
    
    std::vector<ModuleDependency> analyze_dependencies(
        const std::filesystem::path& path) override;
    
    ValidationResult validate(const std::filesystem::path& path) override;
    
    std::string calculate_cache_key(const std::filesystem::path& path) override;
    
    void unload(const std::filesystem::path& path) override;
    
    ModuleType getModuleType() override {
        return ModuleType::SCRIPT;
    }
    
    int get_priority() const override { return 60; }
};

// ============== 加载器注册表 ==============

class LoaderRegistry {
private:
    std::vector<std::unique_ptr<ExtendedModuleLoader>> loaders;
    std::mutex mutex_;
    
public:
    static LoaderRegistry& getInstance() {
        static LoaderRegistry instance;
        return instance;
    }
    
    // 注册加载器
    void register_loader(std::unique_ptr<ExtendedModuleLoader> loader);
    
    // 自动选择加载器
    ExtendedModuleLoader* select_loader(const std::filesystem::path& path);
    
    // 批量处理依赖
    std::vector<LoadResult> load_with_dependencies(
        const std::filesystem::path& main_path,
        const LoadOptions& options = {});
    
    // 获取所有加载器
    const std::vector<std::unique_ptr<ExtendedModuleLoader>>& get_loaders() const {
        return loaders;
    }
    
    // 初始化内置加载器
    void initialize_builtin_loaders();
};

// ============== 模块管理器 ==============

class ModuleManager {
public:
    static ModuleManager& getInstance() {
        static ModuleManager instance;
        return instance;
    }
    
    // 导入模块
    bool importModule(const std::filesystem::path& path, const std::string& alias = "",
                     const LoadOptions& options = {});
    
    // 从模块导入特定符号
    bool importFromModule(const std::filesystem::path& modulePath, 
                         const std::vector<std::string>& symbols);
    
    // 解析虚空接口
    bool resolveVoidInterfaces();
    
    // 检查所有接口是否已解析
    bool checkAllInterfacesResolved();
    
    // 获取模块信息
    ModuleMetadata* getModule(const std::string& name);
    
    // 获取所有模块
    const std::unordered_map<std::string, ModuleMetadata>& getModules() const {
        return modules_;
    }
    
    // 获取所有虚空接口
    const std::vector<UnresolvedSymbol>& getVoidInterfaces() const {
        return voidInterfaces_;
    }
    
    // 添加虚空接口
    void addVoidInterface(const UnresolvedSymbol& iface);
    
    // 卸载模块
    bool unloadModule(const std::string& name);
    
    // 卸载所有模块
    void unloadAllModules();
    
    // 获取加载器注册表
    LoaderRegistry& getLoaderRegistry() {
        return LoaderRegistry::getInstance();
    }

private:
    ModuleManager() = default;
    
    std::unordered_map<std::string, ModuleMetadata> modules_;
    std::vector<UnresolvedSymbol> voidInterfaces_;
    std::atomic<uint32_t> module_counter{0};
    std::mutex mutex_;
};

} // namespace zcc

#endif // ZCC_MODULE_HPP

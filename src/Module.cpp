#include "Module.hpp"
#include "IPC.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif
#include <iostream>

namespace zcc {

// ==================== FileTypeDetector 实现 ====================

std::unordered_map<std::string, ExtendedModuleLoader*> FileTypeDetector::loaders_by_ext;
std::vector<MagicNumberEntry> FileTypeDetector::magic_numbers;

void FileTypeDetector::register_loader(const std::string& extension, ExtendedModuleLoader* loader) {
    loaders_by_ext[extension] = loader;
}

void FileTypeDetector::register_magic(const std::vector<uint8_t>& magic, ExtendedModuleLoader* loader) {
    magic_numbers.emplace_back(magic, loader);
}

ExtendedModuleLoader* FileTypeDetector::get_loader(const std::filesystem::path& path) {
    // 1. 检查扩展名
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    auto it = loaders_by_ext.find(ext);
    if (it != loaders_by_ext.end()) {
        return it->second;
    }
    
    // 2. 检查魔法数字
    std::ifstream file(path, std::ios::binary);
    if (file) {
        std::vector<char> buffer(256);
        file.read(buffer.data(), 256);
        std::string_view data(buffer.data(), file.gcount());
        
        for (const auto& entry : magic_numbers) {
            if (data.size() >= entry.magic.size()) {
                bool match = true;
                for (size_t i = 0; i < entry.magic.size(); ++i) {
                    if (static_cast<uint8_t>(data[i]) != entry.magic[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    return entry.loader;
                }
            }
        }
    }
    
    return nullptr;
}

ExtendedModuleLoader* FileTypeDetector::detect_by_content(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) return nullptr;
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    // 检查是否包含 [语言] 块
    std::regex block_pattern(R"(\s*\[\s*(\w+)\s*\])");
    if (std::regex_search(content, block_pattern)) {
        // 可能是.sca 文件
        auto& registry = LoaderRegistry::getInstance();
        return registry.select_loader(path);
    }
    
    return nullptr;
}

// ==================== SmartSCALoader 实现 ====================

bool SmartSCALoader::supports(const std::filesystem::path& path) {
    // 1. 检查扩展名
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".sca") return true;
    
    // 2. 检查文件魔术头 "ZCCS"
    std::ifstream file(path, std::ios::binary);
    if (file) {
        char magic[4];
        if (file.read(magic, 4) && memcmp(magic, "ZCCS", 4) == 0) {
            return true;
        }
    }
    
    return false;
}

std::string SmartSCALoader::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

// ==================== SCA v0.3 Manifest 解析器 ====================

SCAManifest SmartSCALoader::parse_manifest(const std::string& content) {
    SCAManifest manifest;
    
    // 提取 [manifest] ... [/manifest] 段
    std::regex manifest_regex(R"(\[manifest\]([\s\S]*?)\[/manifest\])");
    std::smatch match;
    if (!std::regex_search(content, match, manifest_regex)) {
        return manifest;  // 无 manifest 段 → has_manifest = false
    }
    
    manifest.has_manifest = true;
    std::string body = match[1].str();
    
    // 当前正在填充哪个 section
    enum class Section { NONE, DEPENDENCY, INTERFACE };
    Section currentSection = Section::NONE;
    SCAManifestDependency currentDep;
    SCAManifestInterface currentIface;
    
    // 按行解析
    std::istringstream stream(body);
    std::string line;
    while (std::getline(stream, line)) {
        // 去除首尾空白和注释
        line = trim(line);
        if (line.empty()) continue;
        
        // 跳过注释行
        if (line.size() >= 2 && line[0] == '/' && line[1] == '/') continue;
        if (line.size() >= 2 && line[0] == '/' && line[1] == '*') continue;
        
        // 检测 section 头
        if (line == "[dependency]") {
            // 保存上一个 dependency
            if (currentSection == Section::DEPENDENCY && !currentDep.name.empty()) {
                manifest.dependencies.push_back(std::move(currentDep));
                currentDep = SCAManifestDependency{};
            }
            if (currentSection == Section::INTERFACE && !currentIface.symbol.empty()) {
                manifest.interfaces.push_back(std::move(currentIface));
                currentIface = SCAManifestInterface{};
            }
            currentSection = Section::DEPENDENCY;
            continue;
        }
        if (line == "[interface]") {
            if (currentSection == Section::DEPENDENCY && !currentDep.name.empty()) {
                manifest.dependencies.push_back(std::move(currentDep));
                currentDep = SCAManifestDependency{};
            }
            if (currentSection == Section::INTERFACE && !currentIface.symbol.empty()) {
                manifest.interfaces.push_back(std::move(currentIface));
                currentIface = SCAManifestInterface{};
            }
            currentSection = Section::INTERFACE;
            continue;
        }
        
        // 解析 key = value
        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        std::string key = trim(line.substr(0, eqPos));
        std::string value = trim(line.substr(eqPos + 1));
        
        // 分发到对应的 section
        switch (currentSection) {
            case Section::NONE:
                // 顶级 key: module.*
                if (key == "module.name")          manifest.name = value;
                else if (key == "module.version")  manifest.version = value;
                else if (key == "module.sca_version") manifest.sca_version = value;
                else if (key == "module.modes") {
                    // 逗号分隔的模式列表
                    std::istringstream ms(value);
                    std::string mode;
                    while (std::getline(ms, mode, ',')) {
                        mode = trim(mode);
                        if (!mode.empty()) manifest.modes.push_back(mode);
                    }
                }
                break;
                
            case Section::DEPENDENCY:
                if (key == "name")              currentDep.name = value;
                else if (key == "type")         currentDep.type = value;
                else if (key == "version")      currentDep.version = value;
                else if (key == "min_version")  currentDep.min_version = value;
                else if (key == "ipc_method")   currentDep.ipc_method = value;
                else if (key == "version_cmd")  currentDep.version_cmd = value;
                break;
                
            case Section::INTERFACE:
                if (key == "symbol")     currentIface.symbol = value;
                else if (key == "provider")  currentIface.provider = value;
                else if (key == "export")    currentIface.export_name = value;
                else if (key == "endpoint")  currentIface.endpoint = value;
                break;
        }
    }
    
    // 保存最后一个 section
    if (currentSection == Section::DEPENDENCY && !currentDep.name.empty()) {
        manifest.dependencies.push_back(std::move(currentDep));
    }
    if (currentSection == Section::INTERFACE && !currentIface.symbol.empty()) {
        manifest.interfaces.push_back(std::move(currentIface));
    }
    
    // 默认 modes: 若无声明, 仅支持 compile
    if (manifest.modes.empty()) {
        manifest.modes.push_back("compile");
    }
    
    return manifest;
}

SCAModule SmartSCALoader::parse_sca_file(const std::filesystem::path& path) {
    SCAModule module;
    module.source_name = path.string();
    
    std::ifstream file(path);
    if (!file) {
        return module;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    // 跳过魔法头（如果有）
    if (content.size() >= 4 && content.substr(0, 4) == "ZCCS") {
        content = content.substr(4);
    }
    
    // ===== SCA v0.3: 优先解析 Manifest =====
    module.manifest = parse_manifest(content);
    
    // 移除 manifest 段内容，避免被后续块解析器误读
    std::string blockContent = content;
    if (module.manifest.has_manifest) {
        std::regex manifest_block(R"(\[manifest\][\s\S]*?\[/manifest\])");
        blockContent = std::regex_replace(blockContent, manifest_block, "");
    }
    
    // 解析 [语言] 块
    // 匹配格式：[language] code [/language] 或 [language] code [next]
    std::regex block_pattern(
        R"(\s*\[\s*(\w+)\s*\](.*?)(?=\s*\[\s*\w+\s*\]|\s*\Z))",
        std::regex_constants::ECMAScript
    );
    
    std::sregex_iterator it(blockContent.begin(), blockContent.end(), block_pattern);
    std::sregex_iterator end;
    
    size_t line_number = 1;
    for (; it != end; ++it) {
        std::string language = (*it)[1];
        std::string code = (*it)[2];
        
        // 转换为小写
        std::transform(language.begin(), language.end(), language.begin(), ::tolower);
        
        // ===== SCA v0.3: 块类型白名单检查 =====
        // 只允许 zcc 和 ir; c/cpp/asm/manifest 一律拒绝
        if (language == "c" || language == "cpp" || language == "c++" ||
            language == "asm" || language == "assembly" || language == "manifest") {
            continue;  // v0.3: 静默跳过非原生块
        }
        
        // 计算源代码行号
        size_t pos = (*it).position();
        size_t prev_newlines = std::count(blockContent.begin(), blockContent.begin() + pos, '\n');
        
        CodeBlock block;
        block.language = language;
        block.code = trim(code);
        block.source_line = line_number + prev_newlines;
        
        // 确定语言类型（v0.3: 仅 ZCC 和 IR）
        if (language == "zcc") {
            block.lang_type = CodeLanguage::ZCC;
        } else if (language == "ir") {
            block.lang_type = CodeLanguage::IR;
        } else {
            block.lang_type = CodeLanguage::UNKNOWN;
        }
        
        module.blocks.push_back(std::move(block));
    }
    
    // 如果没有块，整个文件视为 ZCC 代码
    if (module.blocks.empty()) {
        CodeBlock block;
        block.language = "zcc";
        block.code = blockContent;
        block.lang_type = CodeLanguage::ZCC;
        block.source_line = 1;
        module.blocks.push_back(std::move(block));
    }
    
    return module;
}

void SmartSCALoader::process_zcc_block(const CodeBlock& block, LoadResult& result) {
    // 解析 ZCC 代码块
    // 这里进行基本的语法检查和符号收集
    
    // 简单实现：收集函数定义
    std::regex func_def(R"((?:function|函数|func)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
    std::sregex_iterator it(block.code.begin(), block.code.end(), func_def);
    std::sregex_iterator end;
    
    for (; it != end; ++it) {
        std::string func_name = (*it)[1];
        ExportedSymbol sym;
        sym.name = func_name;
        sym.type = "function";
        // 可以进一步解析参数和返回类型
    }
}

void SmartSCALoader::process_ir_block(const CodeBlock& block, LoadResult& result) {
    // 解析 IR 代码块
    // 收集 IR 级别的符号
}

void SmartSCALoader::process_native_block(const CodeBlock& block, LoadResult& result) {
    // 解析 C/C++ 代码块
    // 这些通常用于内联代码，不导出符号
}

std::vector<UnresolvedSymbol> SmartSCALoader::find_unresolved_symbols(const SCAModule& module) {
    std::vector<UnresolvedSymbol> unresolved;
    
    // ===== SCA v0.3: 如果 manifest 声明了接口映射，直接用 =====
    if (module.manifest.has_manifest && !module.manifest.interfaces.empty()) {
        for (auto& iface : module.manifest.interfaces) {
            UnresolvedSymbol us;
            us.name = iface.symbol;
            us.source_file = module.source_name;
            us.source_line = 0;
            us.expected_source = iface.provider;
            // 根据 provider 类型推断 dep_type
            us.dep_type = DependencyType::MODULE;  // 默认 sca 模块
            unresolved.push_back(us);
        }
        return unresolved;
    }
    
    // ===== 回退: v0.2 正则猜测（无 manifest 时） =====
    std::set<std::string> defined_symbols;
    std::set<std::string> called_symbols;
    
    // 1. 收集所有已定义的符号
    for (const auto& block : module.blocks) {
        if (block.lang_type == CodeLanguage::ZCC) {
            std::regex func_def(R"((?:function|函数|func)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
            std::sregex_iterator it(block.code.begin(), block.code.end(), func_def);
            std::sregex_iterator end;
            for (; it != end; ++it) defined_symbols.insert((*it)[1]);
            
            std::regex var_def(R"((?:auto|定义)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=)");
            it = std::sregex_iterator(block.code.begin(), block.code.end(), var_def);
            for (; it != end; ++it) defined_symbols.insert((*it)[1]);
        }
    }
    
    // 2. 收集所有被调用的符号
    for (const auto& block : module.blocks) {
        if (block.lang_type == CodeLanguage::ZCC) {
            std::regex func_call(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
            std::sregex_iterator it(block.code.begin(), block.code.end(), func_call);
            std::sregex_iterator end;
            for (; it != end; ++it) {
                std::string name = (*it)[1];
                if (name == "function" || name == "函数" || name == "func" ||
                    name == "auto" || name == "定义" || name == "if" || name == "如果" ||
                    name == "for" || name == "循环" || name == "while" || name == "当") {
                    continue;
                }
                called_symbols.insert(name);
            }
        }
    }
    
    // 3. 差集 → 虚空接口
    for (const auto& symbol : called_symbols) {
        if (defined_symbols.find(symbol) == defined_symbols.end()) {
            UnresolvedSymbol us;
            us.name = symbol;
            us.source_file = module.source_name;
            us.source_line = 0;
            us.dep_type = DependencyType::MODULE;
            unresolved.push_back(us);
        }
    }
    
    return unresolved;
}

LoadResult SmartSCALoader::load(const std::filesystem::path& path, 
                                const LoadOptions& options) {
    LoadResult result;
    
    try {
        // 解析.sca 文件
        auto sca_module = parse_sca_file(path);
        
        // ===== SCA v0.3: Manifest 模式验证 =====
        if (sca_module.manifest.has_manifest) {
            bool hasZCC = false, hasIR = false;
            for (auto& block : sca_module.blocks) {
                if (block.lang_type == CodeLanguage::ZCC) hasZCC = true;
                if (block.lang_type == CodeLanguage::IR)   hasIR = true;
            }
            
            if (!hasZCC && !hasIR) {
                result.warnings.push_back("SCA v0.3: 模块无有效代码块 (需要 [zcc] 或 [ir])");
            }
            
            // 验证 modes 声明
            if (sca_module.manifest.modes.empty()) {
                result.warnings.push_back("SCA v0.3: 未声明 module.modes, 默认 compile");
            }
        }
        
        module_ = std::make_shared<SCAModule>(std::move(sca_module));
        
        // 处理所有代码块
        for (const auto& block : module_->blocks) {
            switch (block.lang_type) {
                case CodeLanguage::ZCC:
                    process_zcc_block(block, result);
                    break;
                case CodeLanguage::IR:
                    process_ir_block(block, result);
                    break;
                default:
                    // v0.3: C/CPP/ASM 已在 parse_sca_file 中过滤
                    result.warnings.push_back("跳过不支持的语言块：" + block.language);
            }
        }
        
        // 虚空接口: v0.3 用 manifest interfaces, 否则回退正则
        if (options.resolve_virtual) {
            result.unresolved_symbols = find_unresolved_symbols(*module_);
        }
        
        // 设置元数据 — 优先使用 manifest 信息
        if (module_->manifest.has_manifest && !module_->manifest.name.empty()) {
            module_->metadata->name = module_->manifest.name;
            module_->metadata->version = module_->manifest.version;
        }
        module_->metadata->source_path = path.string();
        module_->metadata->type = ModuleType::INSTRUCTION_SET;
        
        // Manifest 依赖 → metadata.dependencies
        if (module_->manifest.has_manifest) {
            for (auto& dep : module_->manifest.dependencies) {
                ModuleDependency md;
                md.name = dep.name;
                md.version_constraint = dep.version;
                md.type = (dep.type == "executable") ? DependencyType::EXECUTABLE 
                                                       : DependencyType::MODULE;
                module_->metadata->dependencies.push_back(md);
            }
        }
        
        // 创建模块对象
        auto mod = std::make_unique<Module>(path, ModuleType::INSTRUCTION_SET);
        mod->metadata = module_->metadata;
        mod->is_loaded = true;
        
        result.module = std::move(mod);
        result.success = true;
        
    } catch (const std::exception& e) {
        result.errors.push_back(std::string("加载失败：") + e.what());
        result.success = false;
    }
    
    return result;
}

ModuleMetadata SmartSCALoader::get_metadata(const std::filesystem::path& path) {
    ModuleMetadata metadata;
    metadata.source_path = path.string();
    metadata.type = ModuleType::INSTRUCTION_SET;
    metadata.name = path.stem().string();
    
    // 尝试从文件内容提取更多信息
    std::ifstream file(path);
    if (file) {
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        // ===== SCA v0.3: 从 manifest 提取 =====
        auto manifest = parse_manifest(content);
        if (manifest.has_manifest) {
            if (!manifest.name.empty())     metadata.name = manifest.name;
            if (!manifest.version.empty())  metadata.version = manifest.version;
            
            // 填充 modes 到 attributes
            std::string modesStr;
            for (size_t i = 0; i < manifest.modes.size(); ++i) {
                if (i > 0) modesStr += ",";
                modesStr += manifest.modes[i];
            }
            metadata.attributes["sca_version"] = manifest.sca_version;
            metadata.attributes["modes"] = modesStr;
            
            // Manifest 依赖 → metadata.dependencies
            for (auto& dep : manifest.dependencies) {
                ModuleDependency md;
                md.name = dep.name;
                md.version_constraint = dep.version;
                md.type = (dep.type == "executable") ? DependencyType::EXECUTABLE 
                                                       : DependencyType::MODULE;
                metadata.dependencies.push_back(md);
            }
            
            return metadata;
        }
        
        // ===== 回退: v0.2 正则提取 =====
        std::regex version_pattern(R"(@version\s+(\S+))");
        std::smatch match;
        if (std::regex_search(content, match, version_pattern)) {
            metadata.version = match[1];
        }
        
        std::regex desc_pattern(R"(@description\s+(.+))");
        if (std::regex_search(content, match, desc_pattern)) {
            metadata.description = match[1];
        }
    }
    
    return metadata;
}

std::vector<ExportedSymbol> SmartSCALoader::get_exports(const std::filesystem::path& path) {
    std::vector<ExportedSymbol> exports;
    
    auto metadata = get_metadata(path);
    
    // 解析文件获取导出符号
    std::ifstream file(path);
    if (file) {
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        // 查找函数定义
        std::regex func_def(R"((?:function|函数|func)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\))");
        std::sregex_iterator it(content.begin(), content.end(), func_def);
        std::sregex_iterator end;
        
        for (; it != end; ++it) {
            ExportedSymbol sym;
            sym.name = (*it)[1];
            sym.type = "function";
            sym.signature = (*it)[2];
            sym.is_public = true;
            exports.push_back(sym);
        }
    }
    
    return exports;
}

std::vector<UnresolvedSymbol> SmartSCALoader::get_imports(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) return {};
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    std::vector<UnresolvedSymbol> imports;
    
    // 查找所有函数调用
    std::regex func_call(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
    std::sregex_iterator it(content.begin(), content.end(), func_call);
    std::sregex_iterator end;
    
    for (; it != end; ++it) {
        std::string name = (*it)[1];
        // 跳过关键词
        if (name == "function" || name == "函数" || name == "func" ||
            name == "auto" || name == "定义" || name == "if" || name == "如果") {
            continue;
        }
        
        UnresolvedSymbol us;
        us.name = name;
        us.source_file = path.string();
        imports.push_back(us);
    }
    
    return imports;
}

std::vector<ModuleDependency> SmartSCALoader::analyze_dependencies(
    const std::filesystem::path& path) {
    
    std::vector<ModuleDependency> deps;
    
    // ===== SCA v0.3: 优先使用 manifest 声明的依赖 =====
    std::ifstream file(path);
    if (file) {
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        // 尝试解析 manifest
        auto manifest = parse_manifest(content);
        if (manifest.has_manifest) {
            for (auto& dep : manifest.dependencies) {
                ModuleDependency md;
                md.name = dep.name;
                md.version_constraint = dep.version;
                md.type = (dep.type == "executable") ? DependencyType::EXECUTABLE 
                                                       : DependencyType::MODULE;
                // sca 类型: 构造默认路径
                if (dep.type == "sca" || dep.type.empty()) {
                    md.path = path.parent_path() / (dep.name + ".sca");
                }
                md.is_optional = false;
                deps.push_back(md);
            }
            return deps;
        }
    }
    
    // ===== 回退: v0.2 正则提取 import 语句 =====
    std::ifstream file2(path);
    if (file2) {
        std::string content((std::istreambuf_iterator<char>(file2)),
                           std::istreambuf_iterator<char>());
        
        std::regex import_pattern(R"(import\s+["']([^"']+)["'])");
        std::sregex_iterator it(content.begin(), content.end(), import_pattern);
        std::sregex_iterator end;
        
        for (; it != end; ++it) {
            std::string import_path = (*it)[1];
            
            std::filesystem::path full_path;
            if (std::filesystem::path(import_path).is_absolute()) {
                full_path = import_path;
            } else {
                full_path = path.parent_path() / import_path;
            }
            
            ModuleDependency dep;
            dep.name = full_path.stem().string();
            dep.path = full_path;
            dep.type = DependencyType::MODULE;
            deps.push_back(dep);
        }
    }
    
    return deps;
}

ValidationResult SmartSCALoader::validate(const std::filesystem::path& path) {
    ValidationResult result;
    
    // 1. 文件存在性检查
    if (!std::filesystem::exists(path)) {
        result.errors.push_back("文件不存在");
        result.is_valid = false;
        return result;
    }
    
    std::ifstream file(path);
    if (!file) {
        result.errors.push_back("无法打开文件");
        result.is_valid = false;
        return result;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    // 2. 基本括号匹配检查
    int paren_count = 0, brace_count = 0, bracket_count = 0;
    for (char c : content) {
        if (c == '(') paren_count++;
        else if (c == ')') paren_count--;
        else if (c == '{') brace_count++;
        else if (c == '}') brace_count--;
        else if (c == '[') bracket_count++;
        else if (c == ']') bracket_count--;
    }
    
    if (paren_count != 0)  { result.errors.push_back("括号不匹配：缺少 ')'");    result.is_valid = false; }
    if (brace_count != 0)  { result.errors.push_back("大括号不匹配：缺少 '}'");  result.is_valid = false; }
    if (bracket_count != 0){ result.errors.push_back("方括号不匹配：缺少 ']'");  result.is_valid = false; }
    
    // ===== SCA v0.3: Manifest 验证 =====
    auto manifest = parse_manifest(content);
    if (manifest.has_manifest) {
        if (manifest.name.empty()) {
            result.errors.push_back("SCA v0.3: manifest 缺少 module.name");
            result.is_valid = false;
        }
        if (manifest.version.empty() || manifest.version == "0.0.0") {
            result.warnings.push_back("SCA v0.3: 建议声明 module.version");
        }
        if (manifest.modes.empty()) {
            result.warnings.push_back("SCA v0.3: 未声明 module.modes (默认 compile)");
        }
        
        // 检查版本约束字段完整性
        for (auto& dep : manifest.dependencies) {
            if (dep.name.empty()) {
                result.warnings.push_back("SCA v0.3: dependency 缺少 name");
            }
        }
        for (auto& iface : manifest.interfaces) {
            if (iface.symbol.empty() || iface.provider.empty()) {
                result.warnings.push_back("SCA v0.3: interface 缺少 symbol 或 provider");
            }
        }
        
        // 检查是否有被禁止的代码块
        std::regex forbidden_blocks(R"(\[(c|cpp|c\+\+|asm|assembly)\])");
        if (std::regex_search(content, forbidden_blocks)) {
            result.errors.push_back("SCA v0.3: 不允许 [c]/[cpp]/[asm] 代码块 (铁律)");
            result.is_valid = false;
        }
        
        // ===== SCA v0.3: 版本约束检查 =====
        for (auto& dep : manifest.dependencies) {
            if (dep.version.empty()) {
                result.warnings.push_back("SCA v0.3: dependency '" + dep.name + "' 未声明版本约束");
                continue;
            }
            if (dep.type == "sca" || dep.type.empty()) {
                auto depPath = path.parent_path() / (dep.name + ".sca");
                if (std::filesystem::exists(depPath)) {
                    SmartSCALoader depLoader;
                    auto depMeta = depLoader.get_metadata(depPath);
                    if (!depMeta.version.empty() && depMeta.version != "0.0.0") {
                        if (!checkVersionConstraint(dep.version, depMeta.version)) {
                            result.errors.push_back(
                                "SCA v0.3: 版本不匹配 — '" + dep.name + "' 要求 " + dep.version +
                                ", 实际 " + depMeta.version);
                            result.is_valid = false;
                        } else {
                            result.info.push_back("版本检查通过: " + dep.name + " " + dep.version);
                        }
                    } else {
                        result.warnings.push_back(
                            "SCA v0.3: 依赖 '" + dep.name + "' 未声明 module.version, 忽略版本约束");
                    }
                } else {
                    result.warnings.push_back(
                        "SCA v0.3: 依赖 '" + dep.name + "' 未找到 .sca 文件");
                }
            }
        }
    }
    
    // 3. 检查是否有有效的 [语言] 块
    bool has_zcc_or_ir = false;
    std::regex block_pattern(R"(\[(zcc|ir)\])");
    if (std::regex_search(content, block_pattern)) has_zcc_or_ir = true;
    
    if (!has_zcc_or_ir && content.size() >= 4 && content.substr(0, 4) == "ZCCS") {
        result.warnings.push_back("文件包含 ZCCS 魔术头但没有有效的 [zcc] 或 [ir] 块");
    }
    
    if (result.is_valid) {
        result.info.push_back("验证通过" + 
            (manifest.has_manifest ? " (SCA v" + manifest.sca_version + ")" : ""));
    }
    
    return result;
}

std::string SmartSCALoader::calculate_cache_key(const std::filesystem::path& path) {
    // 使用文件路径和内容哈希作为缓存键
    std::string key = path.string();
    
    std::ifstream file(path, std::ios::binary);
    if (file) {
        // 简单实现：使用文件大小和修改时间
        auto size = std::filesystem::file_size(path);
        auto mtime = std::filesystem::last_write_time(path);
        
        key += "|" + std::to_string(size) + "|" + 
               std::to_string(mtime.time_since_epoch().count());
    }
    
    return key;
}

void SmartSCALoader::unload(const std::filesystem::path& path) {
    module_.reset();
}

// ==================== LoaderRegistry 实现 ====================

void LoaderRegistry::register_loader(std::unique_ptr<ExtendedModuleLoader> loader) {
    std::lock_guard<std::mutex> lock(mutex_);
    loaders.push_back(std::move(loader));
    
    // 按优先级排序
    std::sort(loaders.begin(), loaders.end(),
        [](const auto& a, const auto& b) {
            return a->get_priority() > b->get_priority();
        });
}

ExtendedModuleLoader* LoaderRegistry::select_loader(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& loader : loaders) {
        if (loader->supports(path)) {
            return loader.get();
        }
    }
    return nullptr;
}

std::vector<LoadResult> LoaderRegistry::load_with_dependencies(
    const std::filesystem::path& main_path,
    const LoadOptions& options) {
    
    std::vector<LoadResult> all_results;
    std::set<std::filesystem::path> loaded_paths;
    std::queue<std::filesystem::path> to_load;
    
    to_load.push(main_path);
    
    while (!to_load.empty()) {
        auto current_path = to_load.front();
        to_load.pop();
        
        if (loaded_paths.count(current_path)) {
            continue;
        }
        
        // 选择并调用加载器
        auto loader = select_loader(current_path);
        if (!loader) {
            // 记录错误但继续处理
            LoadResult error_result;
            error_result.success = false;
            error_result.errors.push_back("不支持的格式：" + current_path.string());
            all_results.push_back(std::move(error_result));
            continue;
        }
        
        auto result = loader->load(current_path, options);
        bool loadSuccess = result.success;
        all_results.push_back(std::move(result));
        loaded_paths.insert(current_path);
        
        if (loadSuccess) {
            // ===== SCA v0.3: 版本约束检查 =====
            if (result.module && result.module->metadata) {
                // 检查已加载的依赖是否满足版本约束
                // (由 load_with_dependencies 的父模块检查, 暂存警告)
            }
            
            // 分析依赖，加入队列
            auto deps = loader->analyze_dependencies(current_path);
            for (const auto& dep : deps) {
                if (!loaded_paths.count(dep.path)) {
                    to_load.push(dep.path);
                }
            }
        }
    }
    
    return all_results;
}

void LoaderRegistry::initialize_builtin_loaders() {
    register_loader(std::make_unique<SmartSCALoader>());
    register_loader(std::make_unique<ExternalExecutableLoader>());
    register_loader(std::make_unique<DynamicLibLoader>());
    register_loader(std::make_unique<ScriptLoader>());
}

// ==================== ExternalExecutableLoader 实现 ====================

bool ExternalExecutableLoader::supports(const std::filesystem::path& path) {
    static const std::set<std::string> exec_extensions = {
        ".exe", ".dll", ".so", ".dylib", ""
    };
    
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (exec_extensions.count(ext) > 0) {
        // 检查文件是否存在
        if (std::filesystem::exists(path)) {
            // 检查文件魔法头
            std::ifstream file(path, std::ios::binary);
            char magic[4];
            if (file.read(magic, 4)) {
                // Windows PE: "MZ"
                if (magic[0] == 'M' && magic[1] == 'Z') return true;
                // ELF: 0x7F 'E' 'L' 'F'
                if (magic[0] == 0x7F && magic[1] == 'E' && 
                    magic[2] == 'L' && magic[3] == 'F') return true;
            }
            // 无扩展名的可执行文件
            if (ext.empty()) return true;
        }
    }
    return false;
}

LoadResult ExternalExecutableLoader::load(const std::filesystem::path& path,
                                          const LoadOptions& options) {
    LoadResult result;
    
    try {
        // 1. 检查文件是否存在
        if (!std::filesystem::exists(path)) {
            return LoadResult::create_failure("文件不存在：" + path.string());
        }
        
        // 2. 检查进程是否在运行
        bool process_running = is_process_running(path);
        
        if (!process_running) {
            if (options.lazy_load) {
                // 延迟加载：只记录依赖
                result.unresolved_symbols.push_back(
                    UnresolvedSymbol(path.stem().string(), path.string(), 0, 
                                   DependencyType::EXECUTABLE));
                result.success = true;
                return result;
            }
            
            // 尝试启动进程（简化实现，实际应该由 IPCManager 处理）
            std::cout << "外部库未运行：" << path.filename().string() << std::endl;
            std::cout << "请确保外部库已启动并监听 IPC 通道" << std::endl;
        }
        
        // 3. 创建模块元数据
        auto metadata = std::make_shared<ModuleMetadata>();
        metadata->source_path = path.string();
        metadata->type = ModuleType::EXECUTABLE;
        metadata->name = path.stem().string();
        // metadata->is_loaded = true;
        
        // 4. 尝试通过 IPC 查询导出函数（如果进程在运行）
        if (process_running) {
            // 这里应该调用 IPCManager 查询导出函数
            // 简化实现：假设所有函数都是虚空接口
            ExportedSymbol sym;
            sym.name = "*";  // 通配符，表示所有函数
            sym.type = "external_functions";
            sym.is_public = true;
            metadata->exports.push_back(sym);
        }
        
        // 5. 创建模块对象
        auto mod = std::make_unique<Module>(path, ModuleType::EXECUTABLE);
        mod->metadata = metadata;
        mod->is_loaded = true;
        
        result.module = std::move(mod);
        // result.metadata = metadata;
        result.success = true;
        
    } catch (const std::exception& e) {
        result.errors.push_back(std::string("加载失败：") + e.what());
        result.success = false;
    }
    
    return result;
}

ModuleMetadata ExternalExecutableLoader::get_metadata(const std::filesystem::path& path) {
    ModuleMetadata metadata;
    metadata.source_path = path.string();
    metadata.type = ModuleType::EXECUTABLE;
    metadata.name = path.stem().string();
    return metadata;
}

std::vector<ExportedSymbol> ExternalExecutableLoader::get_exports(
    const std::filesystem::path& path) {
    
    std::vector<ExportedSymbol> exports;
    
    // 如果进程在运行，通过 IPC 查询
    if (is_process_running(path)) {
        // 简化实现：返回通配符
        ExportedSymbol sym;
        sym.name = "*";
        sym.type = "external_functions";
        sym.is_public = true;
        exports.push_back(sym);
    }
    
    return exports;
}

std::vector<UnresolvedSymbol> ExternalExecutableLoader::get_imports(
    const std::filesystem::path& path) {
    // 可执行文件通常不导入 ZCC 符号
    return {};
}

std::vector<ModuleDependency> ExternalExecutableLoader::analyze_dependencies(
    const std::filesystem::path& path) {
    // 简化实现：不分析可执行文件的依赖
    return {};
}

ValidationResult ExternalExecutableLoader::validate(const std::filesystem::path& path) {
    ValidationResult result;
    
    // 1. 文件存在性检查
    if (!std::filesystem::exists(path)) {
        result.errors.push_back("文件不存在");
        result.is_valid = false;
        return result;
    }
    
    // 2. 文件格式检查（魔法头）
    std::ifstream file(path, std::ios::binary);
    char magic[4];
    if (!file.read(magic, 4)) {
        result.errors.push_back("无法读取文件头");
        result.is_valid = false;
        return result;
    }
    
    // 3. 验证魔法头
    bool is_pe = (magic[0] == 'M' && magic[1] == 'Z');
    bool is_elf = (magic[0] == 0x7F && magic[1] == 'E' && 
                   magic[2] == 'L' && magic[3] == 'F');
    
    if (!is_pe && !is_elf) {
        result.warnings.push_back("文件可能不是有效的可执行文件");
    }
    
    // 4. Windows 数字签名验证（可选）
    #ifdef _WIN32
    // 这里可以添加数字签名验证代码
    result.info.push_back("Windows 平台：建议验证数字签名");
    #endif
    
    result.is_valid = result.errors.empty();
    return result;
}

std::string ExternalExecutableLoader::calculate_cache_key(
    const std::filesystem::path& path) {
    
    std::string key = path.string();
    
    if (std::filesystem::exists(path)) {
        auto size = std::filesystem::file_size(path);
        auto mtime = std::filesystem::last_write_time(path);
        
        key += "|" + std::to_string(size) + "|" + 
               std::to_string(mtime.time_since_epoch().count());
    }
    
    return key;
}

void ExternalExecutableLoader::unload(const std::filesystem::path& path) {
    module_.reset();
}

// 辅助函数实现
bool ExternalExecutableLoader::is_process_running(const std::filesystem::path& path) {
    // 简化实现：检查 IPC 通道是否存在
    #ifdef _WIN32
    std::string pipe_name = R"(\\.\pipe\)" + path.stem().string();
    HANDLE hPipe = CreateFileA(pipe_name.c_str(), GENERIC_READ, 0, nullptr, 
                               OPEN_EXISTING, 0, nullptr);
    if (hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipe);
        return true;
    }
    return false;
    #else
    std::string socket_path = "/tmp/" + path.stem().string() + ".sock";
    return std::filesystem::exists(socket_path);
    #endif
}

bool ExternalExecutableLoader::start_process(const std::filesystem::path& path) {
    // 简化实现
    #ifdef _WIN32
    return system(("start \"\" \"" + path.string() + "\" >nul 2>&1").c_str()) == 0;
    #else
    return system((path.string() + " &").c_str()) == 0;
    #endif
}

void* ExternalExecutableLoader::establish_ipc_connection(
    const std::filesystem::path& path) {
    // 简化实现：返回 IPCManager 的连接
    auto& ipc = IPCManager::getInstance();
    std::string address = path.stem().string();
    
    ConnectOptions options;
    auto conn_id = ipc.connect(address, options);
    
    if (conn_id != 0) {
        return reinterpret_cast<void*>(conn_id);
    }
    return nullptr;
}

std::vector<ExportedSymbol> ExternalExecutableLoader::query_exports_via_ipc(void* conn) {
    // 简化实现
    std::vector<ExportedSymbol> exports;
    ExportedSymbol sym;
    sym.name = "*";
    sym.type = "external_functions";
    sym.is_public = true;
    exports.push_back(sym);
    return exports;
}

std::vector<ExportedSymbol> ExternalExecutableLoader::read_export_table(
    const std::filesystem::path& path) {
    // 简化实现：实际应该解析 PE/ELF 文件的导出表
    return {};
}

bool ExternalExecutableLoader::verify_digital_signature(
    const std::filesystem::path& path) {
    #ifdef _WIN32
    // Windows 数字签名验证（需要额外库）
    return true;  // 简化实现
    #else
    return false;
    #endif
}

bool ExternalExecutableLoader::is_potentially_malicious(
    const std::filesystem::path& path) {
    // 简化实现：不进行恶意软件扫描
    return false;
}

void ExternalExecutableLoader::wait_for_ipc_ready(
    const std::filesystem::path& path, int timeout_ms) {
    // 等待 IPC 通道可用
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < timeout_ms) {
        if (is_process_running(path)) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ==================== DynamicLibLoader 实现 ====================

bool DynamicLibLoader::supports(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return (ext == ".dll" || ext == ".so" || ext == ".dylib");
}

LoadResult DynamicLibLoader::load(const std::filesystem::path& path,
                                  const LoadOptions& options) {
    LoadResult result;
    
    try {
        if (!std::filesystem::exists(path)) {
            return LoadResult::create_failure("文件不存在：" + path.string());
        }
        
        // 创建模块元数据
        auto metadata = std::make_shared<ModuleMetadata>();
        metadata->source_path = path.string();
        metadata->type = ModuleType::DYNAMIC_LIB;
        metadata->name = path.stem().string();
        
        // 简化实现：不实际加载动态库
        // 实际应该使用 LoadLibrary/dlopen
        
        auto mod = std::make_unique<Module>(path, ModuleType::DYNAMIC_LIB);
        mod->metadata = metadata;
        mod->is_loaded = true;
        
        result.module = std::move(mod);
        // result.metadata = metadata;
        result.success = true;
        
    } catch (const std::exception& e) {
        result.errors.push_back(std::string("加载失败：") + e.what());
        result.success = false;
    }
    
    return result;
}

ModuleMetadata DynamicLibLoader::get_metadata(const std::filesystem::path& path) {
    ModuleMetadata metadata;
    metadata.source_path = path.string();
    metadata.type = ModuleType::DYNAMIC_LIB;
    metadata.name = path.stem().string();
    return metadata;
}

std::vector<ExportedSymbol> DynamicLibLoader::get_exports(
    const std::filesystem::path& path) {
    // 简化实现：实际应该解析导出表
    return {};
}

std::vector<UnresolvedSymbol> DynamicLibLoader::get_imports(
    const std::filesystem::path& path) {
    return {};
}

std::vector<ModuleDependency> DynamicLibLoader::analyze_dependencies(
    const std::filesystem::path& path) {
    return {};
}

ValidationResult DynamicLibLoader::validate(const std::filesystem::path& path) {
    ValidationResult result;
    
    if (!std::filesystem::exists(path)) {
        result.errors.push_back("文件不存在");
        result.is_valid = false;
        return result;
    }
    
    result.is_valid = true;
    return result;
}

std::string DynamicLibLoader::calculate_cache_key(const std::filesystem::path& path) {
    std::string key = path.string();
    
    if (std::filesystem::exists(path)) {
        auto size = std::filesystem::file_size(path);
        auto mtime = std::filesystem::last_write_time(path);
        
        key += "|" + std::to_string(size) + "|" + 
               std::to_string(mtime.time_since_epoch().count());
    }
    
    return key;
}

void DynamicLibLoader::unload(const std::filesystem::path& path) {
    module_.reset();
}

// ==================== ScriptLoader 实现 ====================

bool ScriptLoader::supports(const std::filesystem::path& path) {
    static const std::set<std::string> script_extensions = {
        ".js", ".py", ".lua", ".rb", ".php"
    };
    
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return script_extensions.count(ext) > 0;
}

LoadResult ScriptLoader::load(const std::filesystem::path& path,
                              const LoadOptions& options) {
    LoadResult result;
    
    try {
        if (!std::filesystem::exists(path)) {
            return LoadResult::create_failure("文件不存在：" + path.string());
        }
        
        // 读取脚本内容
        std::ifstream file(path);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        // 创建模块元数据
        auto metadata = std::make_shared<ModuleMetadata>();
        metadata->source_path = path.string();
        metadata->type = ModuleType::SCRIPT;
        metadata->name = path.stem().string();
        
        // 简化实现：不实际解析脚本
        // 实际应该创建脚本引擎沙箱
        
        auto mod = std::make_unique<Module>(path, ModuleType::SCRIPT);
        mod->metadata = metadata;
        mod->is_loaded = true;
        
        result.module = std::move(mod);
        // result.metadata = metadata;
        result.success = true;
        
    } catch (const std::exception& e) {
        result.errors.push_back(std::string("加载失败：") + e.what());
        result.success = false;
    }
    
    return result;
}

ModuleMetadata ScriptLoader::get_metadata(const std::filesystem::path& path) {
    ModuleMetadata metadata;
    metadata.source_path = path.string();
    metadata.type = ModuleType::SCRIPT;
    metadata.name = path.stem().string();
    return metadata;
}

std::vector<ExportedSymbol> ScriptLoader::get_exports(
    const std::filesystem::path& path) {
    // 简化实现
    return {};
}

std::vector<UnresolvedSymbol> ScriptLoader::get_imports(
    const std::filesystem::path& path) {
    return {};
}

std::vector<ModuleDependency> ScriptLoader::analyze_dependencies(
    const std::filesystem::path& path) {
    return {};
}

ValidationResult ScriptLoader::validate(const std::filesystem::path& path) {
    ValidationResult result;
    
    if (!std::filesystem::exists(path)) {
        result.errors.push_back("文件不存在");
        result.is_valid = false;
        return result;
    }
    
    result.is_valid = true;
    return result;
}

std::string ScriptLoader::calculate_cache_key(const std::filesystem::path& path) {
    std::string key = path.string();
    
    if (std::filesystem::exists(path)) {
        auto size = std::filesystem::file_size(path);
        auto mtime = std::filesystem::last_write_time(path);
        
        key += "|" + std::to_string(size) + "|" + 
               std::to_string(mtime.time_since_epoch().count());
    }
    
    return key;
}

void ScriptLoader::unload(const std::filesystem::path& path) {
    module_.reset();
}

} // namespace zcc

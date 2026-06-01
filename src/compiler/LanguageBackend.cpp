// ZCC 语言后端实现
// 注册所有内置语言后端 (C/C++)

#include "LanguageBackend.hpp"
#include "Keywords.hpp"
#include <stdexcept>
#include <algorithm>
#include <filesystem>

namespace zcc {

// ==================== GCC 路径自适应检测 ====================

struct GCCInfo {
    std::string gccExe;       // gcc.exe 完整路径
    std::string libDir;       // lib/gcc/.../13.1.0/ 目录
    std::string sysLibDir;    // x86_64-w64-mingw32/lib/ 目录
    bool        valid = false;
};

static GCCInfo detectGCC() {
    namespace fs = std::filesystem;
    GCCInfo info;
    
    // 候选 GCC 路径列表 (按优先级)
    std::vector<std::string> candidates = {
        "C:/mingw1310/bin/gcc.exe",
        "H:/工具/C++_Qt/Tools/mingw1310_64/bin/gcc.exe",
        "C:/mingw64/bin/gcc.exe",
        "C:/msys64/mingw64/bin/gcc.exe",
    };
    
    for (const auto& path : candidates) {
        if (!fs::exists(path)) continue;
        
        info.gccExe = path;
        info.valid = true;
        
        fs::path gccPath(path);
        fs::path mingwDir = gccPath.parent_path().parent_path();
        
        // 搜索 lib/gcc/x86_64-w64-mingw32/X.Y.Z/
        fs::path libGcc = mingwDir / "lib" / "gcc";
        if (fs::exists(libGcc)) {
            for (auto& arch : fs::directory_iterator(libGcc)) {
                if (!fs::is_directory(arch)) continue;
                for (auto& ver : fs::directory_iterator(arch)) {
                    if (!fs::is_directory(ver)) continue;
                    info.libDir = ver.path().generic_string() + "/";
                    break;  // 取第一个版本
                }
                break;  // 取第一个架构
            }
        }
        
        // 系统库: x86_64-w64-mingw32/lib/
        fs::path sysLib = mingwDir / "x86_64-w64-mingw32" / "lib";
        if (fs::exists(sysLib)) {
            info.sysLibDir = sysLib.generic_string() + "/";
        }
        
        return info;
    }
    
    return info;
}

// 全局缓存, 只检测一次
static GCCInfo g_cachedGCC;
static bool g_gccDetected = false;

static const GCCInfo& getGCC() {
    if (!g_gccDetected) {
        g_cachedGCC = detectGCC();
        g_gccDetected = true;
    }
    return g_cachedGCC;
}

// ==================== 注册表实现 ====================

LanguageBackendRegistry& LanguageBackendRegistry::instance() {
    static LanguageBackendRegistry registry;
    return registry;
}

void LanguageBackendRegistry::registerBackend(std::unique_ptr<ILanguageBackend> backend) {
    if (backend) {
        backends_[backend->getLanguageName()] = std::move(backend);
    }
}

ILanguageBackend* LanguageBackendRegistry::getBackend(const std::string& languageName) {
    auto it = backends_.find(languageName);
    if (it != backends_.end()) return it->second.get();
    return nullptr;
}

std::vector<std::string> LanguageBackendRegistry::getAvailableLanguages() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : backends_) names.push_back(name);
    return names;
}

ILanguageBackend* LanguageBackendRegistry::findByExtension(const std::string& filename) {
    auto dot = filename.rfind('.');
    if (dot == std::string::npos) return getDefault();
    
    std::string ext = filename.substr(dot);
    for (const auto& [name, backend] : backends_) {
        if (backend->getFileExtension() == ext) return backend.get();
    }
    return getDefault();
}

ILanguageBackend* LanguageBackendRegistry::getDefault() const {
    auto it = backends_.find(defaultLanguage_);
    if (it != backends_.end()) return it->second.get();
    return nullptr;
}

// ==================== C 语言后端 ====================
// 基于现有的 CBackend 实现

class CBackendPlugin : public ILanguageBackend {
public:
    std::string getLanguageName() const override { return "c"; }
    std::string getFileExtension() const override { return ".c"; }
    std::string getDescription() const override {
        return "C Source Code (via GCC/Clang/MSVC)";
    }
    
    std::string generateSource(const ZCCIRModule& ir,
                                const CompileOptions& options) override;
    
    std::optional<CompilerConfig> getCompilerConfig() const override;
    std::vector<std::string> getRuntimeSourceFiles() const override;
    std::vector<std::string> getRuntimeHeaderFiles() const override;
    
private:
    // C 代码生成辅助方法 (复用现有 CBackend 逻辑)
    std::string generateCCode(const ZCCIRModule& module);
    std::string generateFunction(const ZCCIRFunction& func);
    std::string generateBlock(const ZCCIRBlock& block);
    std::string generateInstruction(const ZCCIRInstruction& instr);
    std::string generateGlobal(const ZCCIRGlobal& global);
    std::string generateExpression(const ZCCIROperand& operand);
    std::string convertType(const ZCCType& type);
    std::string indent(int level);
    std::string escapeString(const std::string& str);
    
    std::string output_;
    int indentLevel_ = 0;
};

// ---- 编译器配置 ----

std::optional<CompilerConfig> CBackendPlugin::getCompilerConfig() const {
    const auto& gcc = getGCC();
    if (!gcc.valid) {
        // 回退到硬编码路径
        return CompilerConfig{
            "C:/mingw1310/bin/gcc.exe",
            "C:/mingw1310/bin/gcc.exe {input} -o {output} "
            "-BC:/mingw1310/lib/gcc/x86_64-w64-mingw32/13.1.0/ "
            "-BC:/mingw1310/x86_64-w64-mingw32/lib/ "
            "-LC:/mingw1310/x86_64-w64-mingw32/lib/ "
            "-LC:/mingw1310/lib/gcc/x86_64-w64-mingw32/13.1.0/ "
            "-lm -I. -I runtime -Wno-implicit-function-declaration",
            {"-std=c11"},
            false
        };
    }
    
    std::string templateCmd = gcc.gccExe + " {input} -o {output} ";
    if (!gcc.libDir.empty())
        templateCmd += "-B" + gcc.libDir + " ";
    if (!gcc.sysLibDir.empty()) {
        templateCmd += "-B" + gcc.sysLibDir + " ";
        templateCmd += "-L" + gcc.sysLibDir + " ";
    }
    if (!gcc.libDir.empty())
        templateCmd += "-L" + gcc.libDir + " ";
    templateCmd += "-lm -I. -I runtime -Wno-implicit-function-declaration";
    
    return CompilerConfig{
        gcc.gccExe, templateCmd, {"-std=c11"}, false
    };
}

std::vector<std::string> CBackendPlugin::getRuntimeSourceFiles() const {
    return {"runtime/zcc_runtime.c"};
}

std::vector<std::string> CBackendPlugin::getRuntimeHeaderFiles() const {
    return {"runtime/zcc_runtime.h"};
}

// ---- generateSource 入口 ----

std::string CBackendPlugin::generateSource(const ZCCIRModule& ir,
                                            const CompileOptions& options) {
    return generateCCode(ir);
}

// ==================== C 代码生成 (从 CBackend 复用) ====================

std::string CBackendPlugin::generateCCode(const ZCCIRModule& module) {
    std::ostringstream ss;
    
    ss << "// Generated by ZCC Compiler v0.2\n";
    ss << "// Source: " << module.getMetadata().sourceFile << "\n";
    ss << "// Target: " << module.getMetadata().target.toString() << "\n\n";
    
    ss << "#include <stdio.h>\n";
    ss << "#include <stdlib.h>\n";
    ss << "#include <stdint.h>\n";
    ss << "#include <string.h>\n";
    ss << "#include <math.h>\n";
    ss << "#include \"zcc_runtime.h\"\n\n";
    
    ss << "// BigNum type (backward compat)\n";
    ss << "typedef BigNum BigNum_legacy;\n\n";
    
    for (const auto& global : module.getGlobals()) {
        ss << generateGlobal(*global) << "\n";
    }
    ss << "\n";
    
    for (const auto& func : module.getFunctions()) {
        if (func->isExternal()) {
            ss << convertType(*func->getReturnType()) << " " 
               << func->getName() << "(";
            const auto& args = func->getArguments();
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << convertType(*args[i].second) << " " << args[i].first;
            }
            ss << ");\n";
        }
    }
    ss << "\n";
    
    for (const auto& func : module.getFunctions()) {
        if (!func->isExternal()) {
            ss << generateFunction(*func) << "\n\n";
        }
    }
    
    return ss.str();
}

std::string CBackendPlugin::generateFunction(const ZCCIRFunction& func) {
    std::ostringstream ss;
    ss << convertType(*func.getReturnType()) << " " << func.getName() << "(";
    
    const auto& args = func.getArguments();
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << convertType(*args[i].second) << " " << args[i].first;
    }
    ss << ") {\n";
    
    indentLevel_ = 1;
    for (const auto& block : func.getBlocks()) {
        ss << generateBlock(*block);
    }
    
    if (!func.isExternal()) {
        ss << indent(1) << "return 0;\n";
    }
    
    ss << "}\n";
    return ss.str();
}

std::string CBackendPlugin::generateBlock(const ZCCIRBlock& block) {
    std::ostringstream ss;
    ss << indent(indentLevel_) << "// " << block.getLabel() << "\n";
    
    for (const auto& instr : block.getInstructions()) {
        ss << indent(indentLevel_) << generateInstruction(*instr) << "\n";
    }
    return ss.str();
}

// 操作数名辅助
static std::string opName(const ZCCIRInstruction& instr, size_t idx) {
    if (idx >= instr.getNumOperands()) return "???";
    auto val = instr.getOperand(idx).getValue();
    if (!val) return "???";
    std::string n = val->getName();
    if (n.empty()) n = "v" + std::to_string(val->getID());
    if (n[0] == '%') n = n.substr(1);
    return n;
}

std::string CBackendPlugin::generateInstruction(const ZCCIRInstruction& instr) {
    std::ostringstream ss;
    
    switch (instr.getOpcode()) {
    case ZCCIROpcode::NOP: ss << "// NOP"; break;
    
    // 算术
    case ZCCIROpcode::ADD: ss << opName(instr,0) << " = " << opName(instr,1) << " + " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::SUB: ss << opName(instr,0) << " = " << opName(instr,1) << " - " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::MUL: ss << opName(instr,0) << " = " << opName(instr,1) << " * " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::DIV: ss << opName(instr,0) << " = " << opName(instr,1) << " / " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::MOD: ss << opName(instr,0) << " = " << opName(instr,1) << " % " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::NEG: ss << opName(instr,0) << " = -" << opName(instr,1) << ";"; break;
    case ZCCIROpcode::INC: ss << opName(instr,0) << "++;"; break;
    case ZCCIROpcode::DEC: ss << opName(instr,0) << "--;"; break;
    case ZCCIROpcode::POW: ss << opName(instr,0) << " = pow(" << opName(instr,1) << "," << opName(instr,2) << ");"; break;
    
    // 位运算
    case ZCCIROpcode::AND: ss << opName(instr,0) << " = " << opName(instr,1) << " & " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::OR:  ss << opName(instr,0) << " = " << opName(instr,1) << " | " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::XOR: ss << opName(instr,0) << " = " << opName(instr,1) << " ^ " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::NOT: ss << opName(instr,0) << " = ~" << opName(instr,1) << ";"; break;
    case ZCCIROpcode::SHL: ss << opName(instr,0) << " = " << opName(instr,1) << " << " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::SHR: ss << opName(instr,0) << " = " << opName(instr,1) << " >> " << opName(instr,2) << ";"; break;
    
    // 比较
    case ZCCIROpcode::CMP_EQ: ss << opName(instr,0) << " = " << opName(instr,1) << " == " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::CMP_NE: ss << opName(instr,0) << " = " << opName(instr,1) << " != " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::CMP_LT: ss << opName(instr,0) << " = " << opName(instr,1) << " < " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::CMP_LE: ss << opName(instr,0) << " = " << opName(instr,1) << " <= " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::CMP_GT: ss << opName(instr,0) << " = " << opName(instr,1) << " > " << opName(instr,2) << ";"; break;
    case ZCCIROpcode::CMP_GE: ss << opName(instr,0) << " = " << opName(instr,1) << " >= " << opName(instr,2) << ";"; break;
    
    // 控制流
    case ZCCIROpcode::LOAD:  ss << opName(instr,0) << " = " << opName(instr,1) << ";"; break;
    case ZCCIROpcode::STORE: ss << opName(instr,0) << " = " << opName(instr,1) << ";"; break;
    case ZCCIROpcode::CALL: {
        ss << opName(instr,0) << " = " << opName(instr,1) << "(";
        for (size_t i = 2; i < instr.getNumOperands(); ++i) {
            if (i > 2) ss << ", "; ss << opName(instr,i);
        }
        ss << ");"; break;
    }
    case ZCCIROpcode::RET:
        if (instr.getNumOperands() > 0) ss << "return " << opName(instr,0) << ";";
        else ss << "return 0;"; break;
    case ZCCIROpcode::JMP: ss << "goto " << opName(instr,0) << ";"; break;
    case ZCCIROpcode::JMP_IF: ss << "if(" << opName(instr,0) << ") goto " << opName(instr,1) << ";"; break;
    case ZCCIROpcode::JMP_IF_NOT: ss << "if(!" << opName(instr,0) << ") goto " << opName(instr,1) << ";"; break;
    case ZCCIROpcode::PHI: ss << "// PHI"; break;
    
    // 高精度
    case ZCCIROpcode::BIG_ADD: ss << "bignum_add(&" << opName(instr,0) << ",&" << opName(instr,1) << ",&" << opName(instr,2) << ");"; break;
    case ZCCIROpcode::BIG_SUB: ss << "bignum_sub(&" << opName(instr,0) << ",&" << opName(instr,1) << ",&" << opName(instr,2) << ");"; break;
    case ZCCIROpcode::BIG_MUL: ss << "bignum_mul(&" << opName(instr,0) << ",&" << opName(instr,1) << ",&" << opName(instr,2) << ");"; break;
    
    // 数组
    case ZCCIROpcode::ARRAY_NEW: ss << "ZCCArray* " << opName(instr,0) << " = zcc_array_new();"; break;
    case ZCCIROpcode::ARRAY_GET: ss << opName(instr,0) << " = zcc_array_get(" << opName(instr,1) << "," << opName(instr,2) << ");"; break;
    case ZCCIROpcode::ARRAY_SET: ss << "zcc_array_set(" << opName(instr,0) << "," << opName(instr,1) << "," << opName(instr,2) << ");"; break;
    case ZCCIROpcode::ARRAY_LEN: ss << opName(instr,0) << " = zcc_array_len(" << opName(instr,1) << ");"; break;
    
    // 对象
    case ZCCIROpcode::OBJECT_NEW: ss << "ZCCObject* " << opName(instr,0) << " = zcc_object_new();"; break;
    case ZCCIROpcode::OBJECT_GET: ss << opName(instr,0) << " = zcc_object_get(" << opName(instr,1) << "," << opName(instr,2) << ");"; break;
    case ZCCIROpcode::OBJECT_SET: ss << "zcc_object_set(" << opName(instr,0) << "," << opName(instr,1) << "," << opName(instr,2) << ");"; break;
    
    // 异常
    case ZCCIROpcode::THROW: ss << "zcc_throw(" << opName(instr,0) << ");"; break;
    
    // 内存操作
    case ZCCIROpcode::ALLOCA: ss << "void* " << opName(instr,0) << " = alloca(" << opName(instr,1) << ");"; break;
    case ZCCIROpcode::ALLOC:  ss << "void* " << opName(instr,0) << " = malloc(" << opName(instr,1) << ");"; break;
    case ZCCIROpcode::FREE:   ss << "free(" << opName(instr,0) << ");"; break;
    
    // 类型转换
    case ZCCIROpcode::TRUNC:   ss << opName(instr,0) << " = (int64_t)" << opName(instr,1) << ";"; break;
    case ZCCIROpcode::ZEXT:    ss << opName(instr,0) << " = (uint64_t)" << opName(instr,1) << ";"; break;
    case ZCCIROpcode::SEXT:    ss << opName(instr,0) << " = (int64_t)(int32_t)" << opName(instr,1) << ";"; break;
    case ZCCIROpcode::FPTRUNC: ss << opName(instr,0) << " = (float)" << opName(instr,1) << ";"; break;
    case ZCCIROpcode::FPEXT:   ss << opName(instr,0) << " = (double)" << opName(instr,1) << ";"; break;
    case ZCCIROpcode::FPTOUI:  ss << opName(instr,0) << " = (uint64_t)" << opName(instr,1) << ";"; break;
    case ZCCIROpcode::FPTOSI:  ss << opName(instr,0) << " = (int64_t)" << opName(instr,1) << ";"; break;
    case ZCCIROpcode::UITOFP:  ss << opName(instr,0) << " = (double)(uint64_t)" << opName(instr,1) << ";"; break;
    case ZCCIROpcode::SITOFP:  ss << opName(instr,0) << " = (double)(int64_t)" << opName(instr,1) << ";"; break;
    case ZCCIROpcode::BITCAST: ss << opName(instr,0) << " = " << opName(instr,1) << "; /* bitcast */"; break;
    
    // 高精度 (续)
    case ZCCIROpcode::BIG_DIV: ss << "bignum_div(&" << opName(instr,0) << ",&" << opName(instr,1) << ",&" << opName(instr,2) << ");"; break;
    case ZCCIROpcode::BIG_MOD: ss << "bignum_mod(&" << opName(instr,0) << ",&" << opName(instr,1) << ",&" << opName(instr,2) << ");"; break;
    case ZCCIROpcode::BIG_CMP: ss << opName(instr,0) << " = bignum_cmp(&" << opName(instr,1) << ",&" << opName(instr,2) << ");"; break;
    case ZCCIROpcode::BIG_POW: ss << "bignum_pow(&" << opName(instr,0) << ",&" << opName(instr,1) << ",&" << opName(instr,2) << ");"; break;
    
    // 字符串操作
    case ZCCIROpcode::STR_CONCAT: ss << opName(instr,0) << " = zcc_str_concat(" << opName(instr,1) << "," << opName(instr,2) << ");"; break;
    case ZCCIROpcode::STR_LEN:    ss << opName(instr,0) << " = strlen(" << opName(instr,1) << ");"; break;
    case ZCCIROpcode::STR_CMP:    ss << opName(instr,0) << " = strcmp(" << opName(instr,1) << "," << opName(instr,2) << ");"; break;
    case ZCCIROpcode::STR_SLICE:  ss << opName(instr,0) << " = zcc_str_slice(" << opName(instr,1) << "," << opName(instr,2) << "," << opName(instr,3) << ");"; break;
    
    // 特殊操作
    case ZCCIROpcode::INTRINSIC: ss << "// intrinsic: " << opName(instr,0); break;
    case ZCCIROpcode::ASSEMBLY:  ss << "__asm__(" << opName(instr,0) << ");"; break;
    case ZCCIROpcode::TRAP:       ss << "abort();"; break;
    case ZCCIROpcode::UNREACHABLE: ss << "__builtin_unreachable();"; break;
    
    default: ss << "// opcode " << static_cast<int>(instr.getOpcode()); break;
    }
    return ss.str();
}

std::string CBackendPlugin::generateGlobal(const ZCCIRGlobal& global) {
    std::ostringstream ss;
    if (global.isConstant()) ss << "const ";
    ss << convertType(*global.getType()) << " " << global.getName() << ";";
    return ss.str();
}

std::string CBackendPlugin::generateExpression(const ZCCIROperand&) {
    return "/* expr */";
}

std::string CBackendPlugin::convertType(const ZCCType& type) {
    std::string name = type.getName();
    if (name == "void")   return "void";
    if (name == "bool")   return "int";
    if (name == "i8")     return "int8_t";
    if (name == "i16")    return "int16_t";
    if (name == "i32")    return "int32_t";
    if (name == "i64")    return "int64_t";
    if (name == "f32")    return "float";
    if (name == "f64")    return "double";
    if (name == "string") return "char*";
    if (name == "any")    return "ZCCValue";
    if (name == "bigint") return "BigNum";
    return "int64_t";
}

std::string CBackendPlugin::indent(int level) {
    return std::string(level * 4, ' ');
}

std::string CBackendPlugin::escapeString(const std::string& str) {
    return "\"" + str + "\"";
}

// ==================== C++ 语言后端 (Stub - 预留扩展) ====================

class CppBackendPlugin : public ILanguageBackend {
public:
    std::string getLanguageName() const override { return "cpp"; }
    std::string getFileExtension() const override { return ".cpp"; }
    std::string getDescription() const override {
        return "C++ Source Code (via G++/Clang++/MSVC) - [预留扩展]";
    }
    
    std::string generateSource(const ZCCIRModule&, const CompileOptions&) override {
        return "// ZCC C++ Backend - Not yet implemented\n// TODO: IR -> C++ code generation\n";
    }
    
    std::optional<CompilerConfig> getCompilerConfig() const override {
        return CompilerConfig{"g++", "g++ {input} -o {output} -std=c++20 -O{flags} -I.", {}, false};
    }
};

// ==================== 初始化所有内置后端 ====================

void initializeBuiltinBackends() {
    auto& reg = LanguageBackendRegistry::instance();
    reg.registerBackend(std::make_unique<CBackendPlugin>());
    reg.registerBackend(std::make_unique<CppBackendPlugin>());
}

} // namespace zcc

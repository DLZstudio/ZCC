#ifndef ZCC_BYTECODE_HPP
#define ZCC_BYTECODE_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <variant>
#include <unordered_map>
#include <stdexcept>
#include <cstring>

namespace zcc {

// ==================== 字节码格式定义 ====================

// ZCC 字节码文件魔数: "ZCCB" (ZCC Bytecode)
constexpr uint32_t BYTECODE_MAGIC = 0x5A434342;

// 字节码版本
constexpr uint8_t BYTECODE_VERSION_MAJOR = 0x01;
constexpr uint8_t BYTECODE_VERSION_MINOR = 0x00;

// 常量类型
enum class ConstantType : uint8_t {
    Null = 0,
    Undefined = 1,
    Boolean = 2,
    Number = 3,
    String = 4,
};

// 常量池项
struct Constant {
    ConstantType type;
    std::variant<std::monostate, bool, double, std::string> value;
    
    Constant() : type(ConstantType::Null), value(std::monostate{}) {}
    Constant(bool b) : type(ConstantType::Boolean), value(b) {}
    Constant(double n) : type(ConstantType::Number), value(n) {}
    Constant(std::string s) : type(ConstantType::String), value(std::move(s)) {}
    
    static Constant undefined() {
        Constant c;
        c.type = ConstantType::Undefined;
        return c;
    }
};

// ==================== 操作码定义 ====================

// 操作码枚举 (单字节)
enum class OpCode : uint8_t {
    // ===== 栈操作 =====
    NOP = 0x00,           // 无操作
    POP = 0x01,           // 弹出栈顶
    DUP = 0x02,           // 复制栈顶
    DUP2 = 0x03,          // 复制栈顶两个元素
    SWAP = 0x04,          // 交换栈顶两个元素
    
    // ===== 常量加载 =====
    LOAD_NULL = 0x10,     // 加载 null
    LOAD_UNDEFINED = 0x11,// 加载 undefined
    LOAD_TRUE = 0x12,     // 加载 true
    LOAD_FALSE = 0x13,    // 加载 false
    LOAD_CONST = 0x14,    // 加载常量 (操作数：常量索引)
    LOAD_CONST_BIG = 0x15,// 加载大索引常量 (操作数：4 字节索引)
    
    // ===== 变量加载 =====
    LOAD_LOCAL = 0x20,    // 加载局部变量 (操作数：槽位索引)
    STORE_LOCAL = 0x21,   // 存储局部变量
    LOAD_GLOBAL = 0x22,   // 加载全局变量 (操作数：名称索引)
    STORE_GLOBAL = 0x23,  // 存储全局变量
    LOAD_UPVALUE = 0x24,  // 加载闭包变量
    STORE_UPVALUE = 0x25, // 存储闭包变量
    
    // ===== 数组和对象 =====
    LOAD_PROPERTY = 0x30, // 加载对象属性 (操作数：属性名索引)
    STORE_PROPERTY = 0x31,// 存储对象属性
    LOAD_SUBSCRIPT = 0x32,// 加载数组/索引元素
    STORE_SUBSCRIPT = 0x33,// 存储数组/索引元素
    
    // ===== 算术运算 =====
    NEGATE = 0x40,        // 取反
    ADD = 0x41,           // 加法
    SUBTRACT = 0x42,      // 减法
    MULTIPLY = 0x43,      // 乘法
    DIVIDE = 0x44,        // 除法
    MODULO = 0x45,        // 取模
    POWER = 0x46,         // 幂运算
    
    // ===== 比较运算 =====
    EQUAL = 0x50,         // 相等
    NOT_EQUAL = 0x51,     // 不等
    STRICT_EQUAL = 0x52,  // 严格相等
    STRICT_NOT_EQUAL = 0x53,// 严格不等
    LESS = 0x54,          // 小于
    GREATER = 0x55,       // 大于
    LESS_EQUAL = 0x56,    // 小于等于
    GREATER_EQUAL = 0x57, // 大于等于
    
    // ===== 逻辑运算 =====
    NOT = 0x60,           // 逻辑非
    AND = 0x61,           // 逻辑与
    OR = 0x62,            // 逻辑或
    
    // ===== 位运算 =====
    BIT_NOT = 0x68,       // 位非
    BIT_AND = 0x69,       // 位与
    BIT_OR = 0x6A,        // 位或
    BIT_XOR = 0x6B,       // 位异或
    SHIFT_LEFT = 0x6C,    // 左移
    SHIFT_RIGHT = 0x6D,   // 右移
    
    // ===== 控制流 =====
    JUMP = 0x70,          // 无条件跳转 (操作数：偏移量)
    JUMP_IF_FALSE = 0x71, // 假时跳转
    JUMP_IF_TRUE = 0x72,  // 真时跳转
    JUMP_IF_NULL = 0x73,  // null 时跳转
    LOOP = 0x74,          // 循环跳转
    
    // ===== 函数调用 =====
    CALL = 0x80,          // 调用 (操作数：参数个数)
    CALL_METHOD = 0x81,   // 调用方法
    CALL_SUPER = 0x82,    // 调用父类方法
    RETURN = 0x83,        // 返回
    RETURN_VALUE = 0x84,  // 返回值
    CLOSURE = 0x85,       // 创建闭包
    
    // ===== 类和对象 =====
    NEW_INSTANCE = 0x90,  // 创建实例
    NEW_ARRAY = 0x91,     // 创建数组 (操作数：元素个数)
    NEW_OBJECT = 0x92,    // 创建对象
    NEW_OBJECT_WITH_SIZE = 0x93, // 创建对象 (操作数：属性个数)
    
    // ===== 异常处理 =====
    THROW = 0xA0,         // 抛出异常
    TRY_BEGIN = 0xA1,     // try 块开始
    TRY_END = 0xA2,       // try 块结束
    CATCH = 0xA3,         // catch 块
    
    // ===== 其他 =====
    PRINT = 0xB0,         // 打印栈顶 (调试用)
    DEBUG = 0xB1,         // 调试断点
    
    // ===== 特殊 =====
    HALT = 0xFF,          // 停止执行
};

// ==================== 字节码结构 ====================

// 函数信息
struct FunctionInfo {
    std::string name;
    uint16_t arity;           // 参数个数
    uint16_t localCount;      // 局部变量数量
    uint16_t upvalueCount;    // 闭包变量数量
    bool isAsync;
    bool isVariadic;          // 是否有剩余参数
    std::vector<uint8_t> code;// 字节码
    std::vector<Constant> constants;
    std::vector<std::string> localNames;
    std::vector<std::string> upvalueNames;
    uint16_t maxStackSlots;   // 最大栈深度
    uint16_t lineCount;       // 源代码行数
    
    // 调试信息：字节码偏移 -> 源文件行号
    std::vector<std::pair<size_t, size_t>> debugLines;
};

// 类信息
struct ClassInfo {
    std::string name;
    std::string superClass;
    std::vector<FunctionInfo> methods;
    std::vector<FunctionInfo> staticMethods;
    std::vector<std::string> fields;
};

// 全局变量信息
struct GlobalInfo {
    std::string name;
    bool isConst;
    size_t constantIndex;
};

// 导入信息
struct ImportInfo {
    std::string modulePath;
    std::string namespaceAlias;
    std::vector<std::pair<std::string, std::string>> specifiers; // (imported, local)
    std::string defaultImport;
};

// 导出信息
struct ExportInfo {
    std::string name;
    bool isDefault;
    size_t functionIndex;
    size_t classIndex;
    size_t variableIndex;
};

// 字节码模块 (编译单元)
struct BytecodeModule {
    // 文件头
    uint32_t magic = BYTECODE_MAGIC;
    uint8_t versionMajor = BYTECODE_VERSION_MAJOR;
    uint8_t versionMinor = BYTECODE_VERSION_MINOR;
    
    // 模块信息
    std::string sourceFilename;
    std::string moduleName;
    
    // 常量池
    std::vector<Constant> constants;
    
    // 函数
    std::vector<FunctionInfo> functions;
    uint16_t mainFunctionIndex = 0xFFFF;  // 主函数索引
    
    // 类
    std::vector<ClassInfo> classes;
    
    // 全局变量
    std::vector<GlobalInfo> globals;
    
    // 导入导出
    std::vector<ImportInfo> imports;
    std::vector<ExportInfo> exports;
    
    // 源代码映射
    std::string sourceMap;
    
    // ==================== 序列化 ====================
    
    // 序列化为二进制
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buffer;
        
        // 文件头
        writeUint32(buffer, magic);
        buffer.push_back(versionMajor);
        buffer.push_back(versionMinor);
        
        // 源文件名
        writeString(buffer, sourceFilename);
        writeString(buffer, moduleName);
        
        // 常量池
        writeUint16(buffer, static_cast<uint16_t>(constants.size()));
        for (const auto& c : constants) {
            writeConstant(buffer, c);
        }
        
        // 函数
        writeUint16(buffer, static_cast<uint16_t>(functions.size()));
        for (const auto& f : functions) {
            writeFunction(buffer, f);
        }
        writeUint16(buffer, mainFunctionIndex);
        
        // 类
        writeUint16(buffer, static_cast<uint16_t>(classes.size()));
        for (const auto& c : classes) {
            writeClass(buffer, c);
        }
        
        // 全局变量
        writeUint16(buffer, static_cast<uint16_t>(globals.size()));
        for (const auto& g : globals) {
            writeGlobal(buffer, g);
        }
        
        // 导入
        writeUint16(buffer, static_cast<uint16_t>(imports.size()));
        for (const auto& i : imports) {
            writeImport(buffer, i);
        }
        
        // 导出
        writeUint16(buffer, static_cast<uint16_t>(exports.size()));
        for (const auto& e : exports) {
            writeExport(buffer, e);
        }
        
        // 源代码映射
        writeString(buffer, sourceMap);
        
        return buffer;
    }
    
    // 从二进制反序列化
    static BytecodeModule deserialize(const std::vector<uint8_t>& buffer) {
        BytecodeModule module;
        size_t offset = 0;
        
        // 文件头
        module.magic = readUint32(buffer, offset);
        module.versionMajor = buffer[offset++];
        module.versionMinor = buffer[offset++];
        
        // 验证魔数
        if (module.magic != BYTECODE_MAGIC) {
            throw std::runtime_error("Invalid bytecode magic number");
        }
        
        module.sourceFilename = readString(buffer, offset);
        module.moduleName = readString(buffer, offset);
        
        // 常量池
        uint16_t constCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < constCount; ++i) {
            module.constants.push_back(readConstant(buffer, offset));
        }
        
        // 函数
        uint16_t funcCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < funcCount; ++i) {
            module.functions.push_back(readFunction(buffer, offset));
        }
        module.mainFunctionIndex = readUint16(buffer, offset);
        
        // 类
        uint16_t classCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < classCount; ++i) {
            module.classes.push_back(readClass(buffer, offset));
        }
        
        // 全局变量
        uint16_t globalCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < globalCount; ++i) {
            module.globals.push_back(readGlobal(buffer, offset));
        }
        
        // 导入
        uint16_t importCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < importCount; ++i) {
            module.imports.push_back(readImport(buffer, offset));
        }
        
        // 导出
        uint16_t exportCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < exportCount; ++i) {
            module.exports.push_back(readExport(buffer, offset));
        }
        
        // 源代码映射
        module.sourceMap = readString(buffer, offset);
        
        return module;
    }

private:
    // 写入辅助
    static void writeUint16(std::vector<uint8_t>& buffer, uint16_t value) {
        buffer.push_back(value & 0xFF);
        buffer.push_back((value >> 8) & 0xFF);
    }
    
    static void writeUint32(std::vector<uint8_t>& buffer, uint32_t value) {
        buffer.push_back(value & 0xFF);
        buffer.push_back((value >> 8) & 0xFF);
        buffer.push_back((value >> 16) & 0xFF);
        buffer.push_back((value >> 24) & 0xFF);
    }
    
    static void writeString(std::vector<uint8_t>& buffer, const std::string& str) {
        writeUint32(buffer, static_cast<uint32_t>(str.size()));
        buffer.insert(buffer.end(), str.begin(), str.end());
    }
    
    static void writeConstant(std::vector<uint8_t>& buffer, const Constant& c) {
        buffer.push_back(static_cast<uint8_t>(c.type));
        switch (c.type) {
            case ConstantType::Boolean:
                buffer.push_back(std::get<bool>(c.value) ? 1 : 0);
                break;
            case ConstantType::Number: {
                double d = std::get<double>(c.value);
                uint64_t bits;
                memcpy(&bits, &d, sizeof(double));
                for (int i = 0; i < 8; ++i) {
                    buffer.push_back((bits >> (i * 8)) & 0xFF);
                }
                break;
            }
            case ConstantType::String:
                writeString(buffer, std::get<std::string>(c.value));
                break;
            default:
                break;
        }
    }
    
    static void writeFunction(std::vector<uint8_t>& buffer, const FunctionInfo& f) {
        writeString(buffer, f.name);
        writeUint16(buffer, f.arity);
        writeUint16(buffer, f.localCount);
        writeUint16(buffer, f.upvalueCount);
        buffer.push_back(f.isAsync ? 1 : 0);
        buffer.push_back(f.isVariadic ? 1 : 0);
        
        // 字节码
        writeUint32(buffer, static_cast<uint32_t>(f.code.size()));
        buffer.insert(buffer.end(), f.code.begin(), f.code.end());
        
        // 常量
        writeUint16(buffer, static_cast<uint16_t>(f.constants.size()));
        for (const auto& c : f.constants) {
            writeConstant(buffer, c);
        }
        
        // 局部变量名
        writeUint16(buffer, static_cast<uint16_t>(f.localNames.size()));
        for (const auto& name : f.localNames) {
            writeString(buffer, name);
        }
        
        // 闭包变量名
        writeUint16(buffer, static_cast<uint16_t>(f.upvalueNames.size()));
        for (const auto& name : f.upvalueNames) {
            writeString(buffer, name);
        }
        
        writeUint16(buffer, f.maxStackSlots);
        writeUint16(buffer, f.lineCount);
        
        // 调试行信息
        writeUint16(buffer, static_cast<uint16_t>(f.debugLines.size()));
        for (const auto& [offset, line] : f.debugLines) {
            writeUint32(buffer, static_cast<uint32_t>(offset));
            writeUint32(buffer, static_cast<uint32_t>(line));
        }
    }
    
    static void writeClass(std::vector<uint8_t>& buffer, const ClassInfo& c) {
        writeString(buffer, c.name);
        writeString(buffer, c.superClass);
        
        writeUint16(buffer, static_cast<uint16_t>(c.methods.size()));
        for (const auto& m : c.methods) {
            writeFunction(buffer, m);
        }
        
        writeUint16(buffer, static_cast<uint16_t>(c.staticMethods.size()));
        for (const auto& m : c.staticMethods) {
            writeFunction(buffer, m);
        }
        
        writeUint16(buffer, static_cast<uint16_t>(c.fields.size()));
        for (const auto& f : c.fields) {
            writeString(buffer, f);
        }
    }
    
    static void writeGlobal(std::vector<uint8_t>& buffer, const GlobalInfo& g) {
        writeString(buffer, g.name);
        buffer.push_back(g.isConst ? 1 : 0);
        writeUint32(buffer, static_cast<uint32_t>(g.constantIndex));
    }
    
    static void writeImport(std::vector<uint8_t>& buffer, const ImportInfo& i) {
        writeString(buffer, i.modulePath);
        writeString(buffer, i.namespaceAlias);
        writeString(buffer, i.defaultImport);
        
        writeUint16(buffer, static_cast<uint16_t>(i.specifiers.size()));
        for (const auto& [imported, local] : i.specifiers) {
            writeString(buffer, imported);
            writeString(buffer, local);
        }
    }
    
    static void writeExport(std::vector<uint8_t>& buffer, const ExportInfo& e) {
        writeString(buffer, e.name);
        buffer.push_back(e.isDefault ? 1 : 0);
        writeUint16(buffer, static_cast<uint16_t>(e.functionIndex));
        writeUint16(buffer, static_cast<uint16_t>(e.classIndex));
        writeUint16(buffer, static_cast<uint16_t>(e.variableIndex));
    }
    
    // 读取辅助
    static uint16_t readUint16(const std::vector<uint8_t>& buffer, size_t& offset) {
        uint16_t value = buffer[offset] | (buffer[offset + 1] << 8);
        offset += 2;
        return value;
    }
    
    static uint32_t readUint32(const std::vector<uint8_t>& buffer, size_t& offset) {
        uint32_t value = buffer[offset] | (buffer[offset + 1] << 8) |
                        (buffer[offset + 2] << 16) | (buffer[offset + 3] << 24);
        offset += 4;
        return value;
    }
    
    static std::string readString(const std::vector<uint8_t>& buffer, size_t& offset) {
        uint32_t len = readUint32(buffer, offset);
        std::string str(buffer.begin() + offset, buffer.begin() + offset + len);
        offset += len;
        return str;
    }
    
    static Constant readConstant(const std::vector<uint8_t>& buffer, size_t& offset) {
        Constant c;
        c.type = static_cast<ConstantType>(buffer[offset++]);
        
        switch (c.type) {
            case ConstantType::Boolean:
                c.value = buffer[offset++] != 0;
                break;
            case ConstantType::Number: {
                uint64_t bits = 0;
                for (int i = 0; i < 8; ++i) {
                    bits |= static_cast<uint64_t>(buffer[offset++]) << (i * 8);
                }
                double d;
                memcpy(&d, &bits, sizeof(double));
                c.value = d;
                break;
            }
            case ConstantType::String:
                c.value = readString(buffer, offset);
                break;
            default:
                break;
        }
        
        return c;
    }
    
    static FunctionInfo readFunction(const std::vector<uint8_t>& buffer, size_t& offset) {
        FunctionInfo f;
        f.name = readString(buffer, offset);
        f.arity = readUint16(buffer, offset);
        f.localCount = readUint16(buffer, offset);
        f.upvalueCount = readUint16(buffer, offset);
        f.isAsync = buffer[offset++] != 0;
        f.isVariadic = buffer[offset++] != 0;
        
        uint32_t codeSize = readUint32(buffer, offset);
        f.code.assign(buffer.begin() + offset, buffer.begin() + offset + codeSize);
        offset += codeSize;
        
        uint16_t constCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < constCount; ++i) {
            f.constants.push_back(readConstant(buffer, offset));
        }
        
        uint16_t localCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < localCount; ++i) {
            f.localNames.push_back(readString(buffer, offset));
        }
        
        uint16_t upvalueCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < upvalueCount; ++i) {
            f.upvalueNames.push_back(readString(buffer, offset));
        }
        
        f.maxStackSlots = readUint16(buffer, offset);
        f.lineCount = readUint16(buffer, offset);
        
        uint16_t debugCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < debugCount; ++i) {
            uint32_t off = readUint32(buffer, offset);
            uint32_t line = readUint32(buffer, offset);
            f.debugLines.push_back({off, line});
        }
        
        return f;
    }
    
    static ClassInfo readClass(const std::vector<uint8_t>& buffer, size_t& offset) {
        ClassInfo c;
        c.name = readString(buffer, offset);
        c.superClass = readString(buffer, offset);
        
        uint16_t methodCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < methodCount; ++i) {
            c.methods.push_back(readFunction(buffer, offset));
        }
        
        uint16_t staticMethodCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < staticMethodCount; ++i) {
            c.staticMethods.push_back(readFunction(buffer, offset));
        }
        
        uint16_t fieldCount = readUint16(buffer, offset);
        for (uint16_t i = 0; i < fieldCount; ++i) {
            c.fields.push_back(readString(buffer, offset));
        }
        
        return c;
    }
    
    static GlobalInfo readGlobal(const std::vector<uint8_t>& buffer, size_t& offset) {
        GlobalInfo g;
        g.name = readString(buffer, offset);
        g.isConst = buffer[offset++] != 0;
        g.constantIndex = readUint32(buffer, offset);
        return g;
    }
    
    static ImportInfo readImport(const std::vector<uint8_t>& buffer, size_t& offset) {
        ImportInfo i;
        i.modulePath = readString(buffer, offset);
        i.namespaceAlias = readString(buffer, offset);
        i.defaultImport = readString(buffer, offset);
        
        uint16_t specCount = readUint16(buffer, offset);
        for (uint16_t j = 0; j < specCount; ++j) {
            std::string imported = readString(buffer, offset);
            std::string local = readString(buffer, offset);
            i.specifiers.push_back({imported, local});
        }
        
        return i;
    }
    
    static ExportInfo readExport(const std::vector<uint8_t>& buffer, size_t& offset) {
        ExportInfo e;
        e.name = readString(buffer, offset);
        e.isDefault = buffer[offset++] != 0;
        e.functionIndex = readUint16(buffer, offset);
        e.classIndex = readUint16(buffer, offset);
        e.variableIndex = readUint16(buffer, offset);
        return e;
    }
};

// ==================== 字节码工具 ====================

// 反汇编器
class Disassembler {
public:
    static std::string disassemble(const BytecodeModule& module) {
        std::string result;
        
        result += "=== ZCC Bytecode Module ===\n";
        result += "Source: " + module.sourceFilename + "\n";
        result += "Module: " + module.moduleName + "\n\n";
        
        // 常量池
        result += "--- Constants ---\n";
        for (size_t i = 0; i < module.constants.size(); ++i) {
            result += "  [" + std::to_string(i) + "] " + 
                      constantToString(module.constants[i]) + "\n";
        }
        result += "\n";
        
        // 函数
        result += "--- Functions ---\n";
        for (size_t i = 0; i < module.functions.size(); ++i) {
            result += disassembleFunction(module.functions[i], i);
            result += "\n";
        }
        
        // 类
        result += "--- Classes ---\n";
        for (const auto& c : module.classes) {
            result += "class " + c.name;
            if (!c.superClass.empty()) {
                result += " extends " + c.superClass;
            }
            result += "\n";
        }
        
        return result;
    }
    
    static std::string disassembleFunction(const FunctionInfo& func, size_t index) {
        std::string result;
        
        result += "Function [" + std::to_string(index) + "]: " + func.name + "\n";
        result += "  Arity: " + std::to_string(func.arity) + "\n";
        result += "  Locals: " + std::to_string(func.localCount) + "\n";
        result += "  Upvalues: " + std::to_string(func.upvalueCount) + "\n";
        result += "  Async: " + std::string(func.isAsync ? "yes" : "no") + "\n";
        result += "  Max Stack: " + std::to_string(func.maxStackSlots) + "\n\n";
        
        result += "  Code:\n";
        result += disassembleCode(func.code, func.constants, func.debugLines);
        
        return result;
    }
    
    static std::string disassembleCode(const std::vector<uint8_t>& code,
                                       const std::vector<Constant>& constants,
                                       const std::vector<std::pair<size_t, size_t>>& debugLines) {
        std::string result;
        
        for (size_t i = 0; i < code.size();) {
            // 打印行号
            for (const auto& [offset, line] : debugLines) {
                if (offset == i) {
                    result += "    ; Line " + std::to_string(line) + "\n";
                }
            }
            
            size_t offset = i;
            uint8_t byte = code[i++];
            OpCode op = static_cast<OpCode>(byte);
            
            result += "    " + formatOffset(offset) + "  " + 
                      opCodeToString(op);
            
            switch (op) {
                case OpCode::LOAD_CONST:
                case OpCode::LOAD_CONST_BIG:
                case OpCode::LOAD_GLOBAL:
                case OpCode::STORE_GLOBAL:
                case OpCode::LOAD_LOCAL:
                case OpCode::STORE_LOCAL: {
                    uint16_t index = code[i++];
                    if (op == OpCode::LOAD_CONST_BIG) {
                        index = (index << 8) | code[i++];
                    }
                    result += " " + formatIndex(index);
                    if (index < constants.size()) {
                        result += " (" + constantToString(constants[index]) + ")";
                    }
                    break;
                }
                
                case OpCode::CALL:
                case OpCode::NEW_ARRAY:
                case OpCode::NEW_OBJECT_WITH_SIZE: {
                    uint8_t count = code[i++];
                    result += " " + std::to_string(count);
                    break;
                }
                
                case OpCode::JUMP:
                case OpCode::JUMP_IF_FALSE:
                case OpCode::JUMP_IF_TRUE:
                case OpCode::LOOP: {
                    int16_t offset = (code[i++] << 8) | code[i++];
                    result += " " + std::to_string(offset) + " -> " + 
                              formatOffset(i + offset);
                    break;
                }
                
                default:
                    break;
            }
            
            result += "\n";
        }
        
        return result;
    }

private:
    static std::string opCodeToString(OpCode op) {
        switch (op) {
            case OpCode::NOP: return "NOP";
            case OpCode::POP: return "POP";
            case OpCode::DUP: return "DUP";
            case OpCode::DUP2: return "DUP2";
            case OpCode::SWAP: return "SWAP";
            case OpCode::LOAD_NULL: return "LOAD_NULL";
            case OpCode::LOAD_UNDEFINED: return "LOAD_UNDEFINED";
            case OpCode::LOAD_TRUE: return "LOAD_TRUE";
            case OpCode::LOAD_FALSE: return "LOAD_FALSE";
            case OpCode::LOAD_CONST: return "LOAD_CONST";
            case OpCode::LOAD_CONST_BIG: return "LOAD_CONST_BIG";
            case OpCode::LOAD_LOCAL: return "LOAD_LOCAL";
            case OpCode::STORE_LOCAL: return "STORE_LOCAL";
            case OpCode::LOAD_GLOBAL: return "LOAD_GLOBAL";
            case OpCode::STORE_GLOBAL: return "STORE_GLOBAL";
            case OpCode::LOAD_UPVALUE: return "LOAD_UPVALUE";
            case OpCode::STORE_UPVALUE: return "STORE_UPVALUE";
            case OpCode::LOAD_PROPERTY: return "LOAD_PROPERTY";
            case OpCode::STORE_PROPERTY: return "STORE_PROPERTY";
            case OpCode::LOAD_SUBSCRIPT: return "LOAD_SUBSCRIPT";
            case OpCode::STORE_SUBSCRIPT: return "STORE_SUBSCRIPT";
            case OpCode::NEGATE: return "NEGATE";
            case OpCode::ADD: return "ADD";
            case OpCode::SUBTRACT: return "SUBTRACT";
            case OpCode::MULTIPLY: return "MULTIPLY";
            case OpCode::DIVIDE: return "DIVIDE";
            case OpCode::MODULO: return "MODULO";
            case OpCode::POWER: return "POWER";
            case OpCode::EQUAL: return "EQUAL";
            case OpCode::NOT_EQUAL: return "NOT_EQUAL";
            case OpCode::STRICT_EQUAL: return "STRICT_EQUAL";
            case OpCode::STRICT_NOT_EQUAL: return "STRICT_NOT_EQUAL";
            case OpCode::LESS: return "LESS";
            case OpCode::GREATER: return "GREATER";
            case OpCode::LESS_EQUAL: return "LESS_EQUAL";
            case OpCode::GREATER_EQUAL: return "GREATER_EQUAL";
            case OpCode::NOT: return "NOT";
            case OpCode::AND: return "AND";
            case OpCode::OR: return "OR";
            case OpCode::BIT_NOT: return "BIT_NOT";
            case OpCode::BIT_AND: return "BIT_AND";
            case OpCode::BIT_OR: return "BIT_OR";
            case OpCode::BIT_XOR: return "BIT_XOR";
            case OpCode::SHIFT_LEFT: return "SHIFT_LEFT";
            case OpCode::SHIFT_RIGHT: return "SHIFT_RIGHT";
            case OpCode::JUMP: return "JUMP";
            case OpCode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
            case OpCode::JUMP_IF_TRUE: return "JUMP_IF_TRUE";
            case OpCode::JUMP_IF_NULL: return "JUMP_IF_NULL";
            case OpCode::LOOP: return "LOOP";
            case OpCode::CALL: return "CALL";
            case OpCode::CALL_METHOD: return "CALL_METHOD";
            case OpCode::CALL_SUPER: return "CALL_SUPER";
            case OpCode::RETURN: return "RETURN";
            case OpCode::RETURN_VALUE: return "RETURN_VALUE";
            case OpCode::CLOSURE: return "CLOSURE";
            case OpCode::NEW_INSTANCE: return "NEW_INSTANCE";
            case OpCode::NEW_ARRAY: return "NEW_ARRAY";
            case OpCode::NEW_OBJECT: return "NEW_OBJECT";
            case OpCode::NEW_OBJECT_WITH_SIZE: return "NEW_OBJECT_WITH_SIZE";
            case OpCode::THROW: return "THROW";
            case OpCode::TRY_BEGIN: return "TRY_BEGIN";
            case OpCode::TRY_END: return "TRY_END";
            case OpCode::CATCH: return "CATCH";
            case OpCode::PRINT: return "PRINT";
            case OpCode::DEBUG: return "DEBUG";
            case OpCode::HALT: return "HALT";
            default: return "UNKNOWN";
        }
    }
    
    static std::string constantToString(const Constant& c) {
        switch (c.type) {
            case ConstantType::Null: return "null";
            case ConstantType::Undefined: return "undefined";
            case ConstantType::Boolean:
                return std::get<bool>(c.value) ? "true" : "false";
            case ConstantType::Number:
                return std::to_string(std::get<double>(c.value));
            case ConstantType::String:
                return "\"" + std::get<std::string>(c.value) + "\"";
            default:
                return "?";
        }
    }
    
    static std::string formatOffset(size_t offset) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%04zu", offset);
        return buf;
    }
    
    static std::string formatIndex(size_t index) {
        return "[" + std::to_string(index) + "]";
    }
};

} // namespace zcc

#endif // ZCC_BYTECODE_HPP

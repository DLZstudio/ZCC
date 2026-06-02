#ifndef ZCC_NATIVE_BACKEND_HPP
#define ZCC_NATIVE_BACKEND_HPP

#include "Backend.hpp"
#include <functional>
#include <unordered_map>
#include <stack>
#include <cmath>
#include <iostream>

namespace zcc {

// ==================== ZCC 运行时解释器 ====================

// 运行时值
struct RuntimeValue {
    using Value = std::variant<
        std::monostate,  // null
        bool,            // 布尔
        int64_t,         // 整数
        double,          // 浮点
        std::string,     // 字符串
        std::vector<RuntimeValue>,  // 数组
        std::unordered_map<std::string, RuntimeValue>  // 对象
    >;
    
    Value data;
    
    RuntimeValue() : data(std::monostate{}) {}
    RuntimeValue(std::monostate) : data(std::monostate{}) {}
    RuntimeValue(bool b) : data(b) {}
    RuntimeValue(int64_t i) : data(i) {}
    RuntimeValue(double d) : data(d) {}
    RuntimeValue(const std::string& s) : data(s) {}
    RuntimeValue(std::string&& s) : data(std::move(s)) {}
    RuntimeValue(const std::vector<RuntimeValue>& a) : data(a) {}
    RuntimeValue(std::unordered_map<std::string, RuntimeValue> o) : data(std::move(o)) {}
    
    bool isNull() const { return std::holds_alternative<std::monostate>(data); }
    bool isBool() const { return std::holds_alternative<bool>(data); }
    bool isNumber() const { return std::holds_alternative<int64_t>(data) || 
                                  std::holds_alternative<double>(data); }
    bool isString() const { return std::holds_alternative<std::string>(data); }
    bool isArray() const { return std::holds_alternative<std::vector<RuntimeValue>>(data); }
    bool isObject() const { return std::holds_alternative<std::unordered_map<std::string, RuntimeValue>>(data); }
    
    template<typename T>
    T& get() { return std::get<T>(data); }
    
    template<typename T>
    const T& get() const { return std::get<T>(data); }
    
    double asNumber() const {
        if (isNumber()) {
            if (std::holds_alternative<int64_t>(data)) {
                return static_cast<double>(std::get<int64_t>(data));
            }
            return std::get<double>(data);
        }
        return 0.0;
    }
    
    bool asBool() const {
        if (isBool()) return std::get<bool>(data);
        if (isNull()) return false;
        if (isNumber()) return asNumber() != 0.0;
        if (isString()) return !std::get<std::string>(data).empty();
        return true;
    }
    
    std::string toString() const {
        if (isNull()) return "null";
        if (isBool()) return std::get<bool>(data) ? "true" : "false";
        if (std::holds_alternative<int64_t>(data)) {
            return std::to_string(std::get<int64_t>(data));
        }
        if (std::holds_alternative<double>(data)) {
            return std::to_string(std::get<double>(data));
        }
        if (isString()) return std::get<std::string>(data);
        if (isArray()) return "[Array]";
        if (isObject()) return "[Object]";
        return "unknown";
    }
};

// 解释器状态
class InterpreterState {
public:
    // 内置函数
    using BuiltinFunction = std::function<RuntimeValue(const std::vector<RuntimeValue>&)>;
    
    InterpreterState() {
        // 注册内置函数
        registerBuiltin("println", [](const std::vector<RuntimeValue>& args) -> RuntimeValue {
            for (const auto& arg : args) {
                std::cout << arg.toString() << std::endl;
            }
            return RuntimeValue();
        });
        
        registerBuiltin("print", [](const std::vector<RuntimeValue>& args) -> RuntimeValue {
            for (const auto& arg : args) {
                std::cout << arg.toString();
            }
            return RuntimeValue();
        });
        
        registerBuiltin("readline", [](const std::vector<RuntimeValue>& args) -> RuntimeValue {
            if (!args.empty() && args[0].isString()) {
                std::cout << args[0].get<std::string>();
            }
            std::string line;
            std::getline(std::cin, line);
            return RuntimeValue(line);
        });
        
        registerBuiltin("input", [](const std::vector<RuntimeValue>& args) -> RuntimeValue {
            if (!args.empty() && args[0].isString()) {
                std::cout << args[0].get<std::string>();
            }
            std::string line;
            std::getline(std::cin, line);
            return RuntimeValue(line);
        });
        
        registerBuiltin("typeof", [](const std::vector<RuntimeValue>& args) -> RuntimeValue {
            if (args.empty()) return RuntimeValue("null");
            const auto& val = args[0];
            if (val.isNull()) return RuntimeValue("null");
            if (val.isBool()) return RuntimeValue("boolean");
            if (val.isNumber()) return RuntimeValue("number");
            if (val.isString()) return RuntimeValue("string");
            if (val.isArray()) return RuntimeValue("array");
            if (val.isObject()) return RuntimeValue("object");
            return RuntimeValue("unknown");
        });
        
        registerBuiltin("len", [](const std::vector<RuntimeValue>& args) -> RuntimeValue {
            if (args.empty()) return RuntimeValue(int64_t(0));
            const auto& val = args[0];
            if (val.isString()) {
                return RuntimeValue(static_cast<int64_t>(val.get<std::string>().size()));
            }
            if (val.isArray()) {
                return RuntimeValue(static_cast<int64_t>(val.get<std::vector<RuntimeValue>>().size()));
            }
            return RuntimeValue(int64_t(0));
        });
        
        registerBuiltin("str", [](const std::vector<RuntimeValue>& args) -> RuntimeValue {
            if (args.empty()) return RuntimeValue("");
            return RuntimeValue(args[0].toString());
        });
        
        registerBuiltin("num", [](const std::vector<RuntimeValue>& args) -> RuntimeValue {
            if (args.empty()) return RuntimeValue(0.0);
            const auto& val = args[0];
            if (val.isNumber()) return val;
            if (val.isString()) {
                try {
                    return RuntimeValue(std::stod(val.get<std::string>()));
                } catch (...) {
                    return RuntimeValue(0.0);
                }
            }
            return RuntimeValue(0.0);
        });
        
        registerBuiltin("exit", [](const std::vector<RuntimeValue>& args) -> RuntimeValue {
            int code = 0;
            if (!args.empty() && args[0].isNumber()) {
                code = static_cast<int>(args[0].asNumber());
            }
            std::exit(code);
            return RuntimeValue();
        });
    }
    
    // 注册内置函数
    void registerBuiltin(const std::string& name, BuiltinFunction func) {
        builtins_[name] = std::move(func);
    }
    
    // 查找内置函数
    BuiltinFunction* findBuiltin(const std::string& name) {
        auto it = builtins_.find(name);
        if (it != builtins_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    // 变量作用域
    void pushScope() {
        scopes_.push_back(std::unordered_map<std::string, RuntimeValue>());
    }
    
    void popScope() {
        if (!scopes_.empty()) {
            scopes_.pop_back();
        }
    }
    
    void setVariable(const std::string& name, const RuntimeValue& value) {
        if (scopes_.empty()) {
            scopes_.push_back(std::unordered_map<std::string, RuntimeValue>());
        }
        scopes_.back()[name] = value;
    }
    
    RuntimeValue* getVariable(const std::string& name) {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto varIt = it->find(name);
            if (varIt != it->end()) {
                return &varIt->second;
            }
        }
        return nullptr;
    }
    
    // 全局变量
    void setGlobal(const std::string& name, const RuntimeValue& value) {
        globals_[name] = value;
    }
    
    RuntimeValue* getGlobal(const std::string& name) {
        auto it = globals_.find(name);
        if (it != globals_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    // 函数定义
    void setFunction(const std::string& name, const ZCCIRFunction* func) {
        functions_[name] = func;
    }
    
    const ZCCIRFunction* getFunction(const std::string& name) const {
        auto it = functions_.find(name);
        if (it != functions_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // 控制流
    void returnValue(const RuntimeValue& value) {
        returnValue_ = value;
        hasReturnValue_ = true;
    }
    
    bool hasReturn() const { return hasReturnValue_; }
    
    RuntimeValue getReturn() const { return returnValue_; }
    
    void clearReturn() {
        hasReturnValue_ = false;
        returnValue_ = RuntimeValue();
    }
    
private:
    std::vector<std::unordered_map<std::string, RuntimeValue>> scopes_;
    std::unordered_map<std::string, RuntimeValue> globals_;
    std::unordered_map<std::string, BuiltinFunction> builtins_;
    std::unordered_map<std::string, const ZCCIRFunction*> functions_;
    
    RuntimeValue returnValue_;
    bool hasReturnValue_ = false;
};

// ==================== 原生解释器后端 ====================

// 原生后端 - 包含解释器，生成自包含的可执行文件
class NativeBackend : public Backend {
public:
    NativeBackend() = default;
    
    // 解释执行 (测试可访问)
#ifdef ZCC_TEST_ACCESS
public:
#else
private:
#endif
    class Interpreter {
    public:
        Interpreter(InterpreterState& state) : state_(state) {}
        
        // 块执行结果 (CFG 后继)
        struct BlockResult {
            ZCCIRBlock* nextBlock = nullptr;
            RuntimeValue value;
        };
        
        RuntimeValue execute();
        BlockResult executeBlock(const ZCCIRBlock& block);
        RuntimeValue executeInstruction(const ZCCIRInstruction& instr);
        RuntimeValue executeCall(const ZCCIRInstruction& instr);
        
        // 操作数解析
        RuntimeValue resolveOperand(const ZCCIRInstruction& instr, size_t idx);
        RuntimeValue constantToRuntime(const ZCCIRConstant::ConstantValue& cv);
        
    // 测试可访问 (仅当 ZCC_TEST_ACCESS 定义时)
    #ifdef ZCC_TEST_ACCESS
    public:
        std::unordered_map<uint32_t, RuntimeValue>& testReg() { return reg_; }
        std::unordered_map<int64_t, std::vector<uint8_t>>& testAllocTable() { return allocTable_; }
    #endif

    private:
        InterpreterState& state_;
        std::unordered_map<uint32_t, RuntimeValue> reg_;
        std::unordered_map<int64_t, std::vector<uint8_t>> allocTable_;
        int64_t nextHandle_ = 1;
    };
    
public:  // 恢复 public 访问级别 (被 Interpreter 的条件 private: 影响)
    bool initialize() override { return true; }
    
    CompileResult compile(const ZCCIRModule& module,
                         const CompileOptions& options) override;
    
    std::string getName() const override { return "native"; }
    bool isSupported() const override { return true; }
    std::string getFileExtension() const override { return ".exe"; }
    std::string getDescription() const override { return "Native Executable with Embedded Interpreter"; }
    
    static RuntimeValue runModule(const ZCCIRModule& module);
    
    bool saveIRFile(const ZCCIRModule& module, const std::string& path);
    
private:
    std::vector<uint8_t> generateExecutable(const ZCCIRModule& module,
                                           const CompileOptions& options);
};

// ==================== IR 文件保存/加载 ====================

// 保存 IR 到文本文件
bool saveIRToFile(const ZCCIRModule& module, const std::string& filename);

// 从文本文件加载 IR
std::unique_ptr<ZCCIRModule> loadIRFromFile(const std::string& filename);

} // namespace zcc

#endif // ZCC_NATIVE_BACKEND_HPP

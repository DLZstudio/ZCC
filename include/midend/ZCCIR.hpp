#ifndef ZCC_ZCCIR_HPP
#define ZCC_ZCCIR_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include "Types.hpp"

namespace zcc {

// ==================== ZCC-IR 操作码 ====================

enum class ZCCIROpcode {
    // 控制流
    NOP,        // 无操作
    JMP,        // 无条件跳转
    JMP_IF,     // 条件跳转
    JMP_IF_NOT, // 条件不满足跳转
    CALL,       // 函数调用
    RET,        // 返回
    PHI,        // Phi 节点 (SSA)
    
    // 算术运算
    ADD,        // 加法
    SUB,        // 减法
    MUL,        // 乘法
    DIV,        // 除法
    MOD,        // 取模
    NEG,        // 取负
    INC,        // 自增
    DEC,        // 自减
    POW,        // 幂运算
    
    // 位运算
    AND,        // 与
    OR,         // 或
    XOR,        // 异或
    NOT,        // 非
    SHL,        // 左移
    SHR,        // 右移
    
    // 比较运算
    CMP_EQ,     // 等于
    CMP_NE,     // 不等于
    CMP_LT,     // 小于
    CMP_LE,     // 小于等于
    CMP_GT,     // 大于
    CMP_GE,     // 大于等于
    
    // 内存操作
    LOAD,       // 加载
    STORE,      // 存储
    ALLOCA,     // 栈分配
    ALLOC,      // 堆分配
    FREE,       // 释放
    
    // 类型转换
    TRUNC,      // 截断
    ZEXT,       // 零扩展
    SEXT,       // 符号扩展
    FPTRUNC,    // 浮点截断
    FPEXT,      // 浮点扩展
    FPTOUI,     // 浮点到无符号整数
    FPTOSI,     // 浮点到有符号整数
    UITOFP,     // 无符号整数到浮点
    SITOFP,     // 有符号整数到浮点
    BITCAST,    // 位转换
    
    // 高精度运算
    BIG_ADD,    // 大数加法
    BIG_SUB,    // 大数减法
    BIG_MUL,    // 大数乘法
    BIG_DIV,    // 大数除法
    BIG_MOD,    // 大数取模
    BIG_CMP,    // 大数比较
    BIG_POW,    // 大数幂运算
    
    // 字符串操作
    STR_CONCAT, // 字符串连接
    STR_LEN,    // 字符串长度
    STR_CMP,    // 字符串比较
    STR_SLICE,  // 字符串切片
    
    // 数组操作
    ARRAY_NEW,  // 创建数组
    ARRAY_GET,  // 获取元素
    ARRAY_SET,  // 设置元素
    ARRAY_LEN,  // 数组长度
    
    // 对象操作
    OBJECT_NEW, // 创建对象
    OBJECT_GET, // 获取属性
    OBJECT_SET, // 设置属性
    
    // 异常处理
    THROW,      // 抛出异常
    
    // 特殊操作
    INTRINSIC,  // 内部函数
    ASSEMBLY,   // 内联汇编
    TRAP,       // 陷阱
    UNREACHABLE // 不可达
};

// ==================== ZCC-IR 值 ====================

// 值类型
enum class ZCCIRValueKind {
    CONSTANT,   // 常量
    ARGUMENT,   // 参数
    LOCAL,      // 局部变量
    GLOBAL,     // 全局变量
    TEMPORARY,  // 临时变量
    INSTRUCTION // 指令结果
};

// 前向声明
class ZCCIRInstruction;
class ZCCIRBlock;
class ZCCIRFunction;
class ZCCIRModule;

// ZCC-IR 值基类
class ZCCIRValue {
public:
    using ID = uint32_t;

    ZCCIRValue(ZCCIRValueKind kind, ZCCTypePtr type, ID id)
        : kind_(kind), type_(std::move(type)), id_(id) {}

    virtual ~ZCCIRValue() = default;

    ZCCIRValueKind getKind() const { return kind_; }
    const ZCCTypePtr& getType() const { return type_; }
    ID getID() const { return id_; }

    std::string getName() const;
    void setName(const std::string& name);

private:
    ZCCIRValueKind kind_;
    ZCCTypePtr type_;
    ID id_;
    std::string name_;
};

// 常量值
class ZCCIRConstant : public ZCCIRValue {
public:
    using ConstantValue = std::variant<
        std::monostate,  // null
        bool,            // 布尔
        int64_t,         // 整数
        double,          // 浮点
        std::string,     // 字符串
        std::vector<uint8_t> // 二进制
    >;

    ZCCIRConstant(ConstantValue value, ZCCTypePtr type, ID id)
        : ZCCIRValue(ZCCIRValueKind::CONSTANT, std::move(type), id),
          value_(std::move(value)) {}

    const ConstantValue& getValue() const { return value_; }

    std::string toString() const;

private:
    ConstantValue value_;
};

// ==================== ZCC-IR 指令 ====================

// 指令操作数
class ZCCIROperand {
public:
    using ValuePtr = std::shared_ptr<ZCCIRValue>;
    
    ZCCIROperand() = default;
    explicit ZCCIROperand(ValuePtr value) : value_(std::move(value)) {}
    
    ValuePtr getValue() const { return value_; }
    void setValue(ValuePtr value) { value_ = std::move(value); }
    
    bool isNull() const { return value_ == nullptr; }
    
private:
    ValuePtr value_;
};

// ZCC-IR 指令
class ZCCIRInstruction {
public:
    using OperandList = std::vector<ZCCIROperand>;
    using ID = uint32_t;

    ZCCIRInstruction(ZCCIROpcode opcode, ZCCTypePtr type, ID id)
        : opcode_(opcode), type_(std::move(type)), id_(id) {}

    ZCCIROpcode getOpcode() const { return opcode_; }
    const ZCCTypePtr& getType() const { return type_; }
    ID getID() const { return id_; }

    // 操作数
    size_t getNumOperands() const { return operands_.size(); }
    ZCCIROperand& getOperand(size_t idx) { return operands_[idx]; }
    const ZCCIROperand& getOperand(size_t idx) const { return operands_[idx]; }
    void setOperand(size_t idx, ZCCIROperand operand);
    void addOperand(ZCCIROperand operand);

    // 块引用
    ZCCIRBlock* getParent() const { return parent_; }
    void setParent(ZCCIRBlock* block) { parent_ = block; }

    // 调试信息
    void setDebugLocation(std::string file, size_t line, size_t column);
    const std::string& getDebugFile() const { return debugFile_; }
    size_t getDebugLine() const { return debugLine_; }
    size_t getDebugColumn() const { return debugColumn_; }

    std::string toString() const;

private:
    ZCCIROpcode opcode_;
    ZCCTypePtr type_;
    ID id_;
    OperandList operands_;
    ZCCIRBlock* parent_ = nullptr;

    // 调试信息
    std::string debugFile_;
    size_t debugLine_ = 0;
    size_t debugColumn_ = 0;
};

// ==================== ZCC-IR 基本块 ====================

// ZCC-IR 基本块
class ZCCIRBlock {
public:
    using InstructionList = std::vector<std::unique_ptr<ZCCIRInstruction>>;
    using BlockList = std::vector<ZCCIRBlock*>;
    
    ZCCIRBlock(const std::string& label) : label_(label) {}
    
    const std::string& getLabel() const { return label_; }
    
    // 指令
    ZCCIRInstruction* addInstruction(std::unique_ptr<ZCCIRInstruction> instr);
    const InstructionList& getInstructions() const { return instructions_; }
    
    // 前驱和后继
    const BlockList& getPredecessors() const { return predecessors_; }
    const BlockList& getSuccessors() const { return successors_; }
    void addPredecessor(ZCCIRBlock* block);
    void addSuccessor(ZCCIRBlock* block);
    
    // 函数引用
    ZCCIRFunction* getParent() const { return parent_; }
    void setParent(ZCCIRFunction* func) { parent_ = func; }
    
    // 终止指令
    bool isTerminated() const;
    ZCCIRInstruction* getTerminator() const;
    
private:
    std::string label_;
    InstructionList instructions_;
    BlockList predecessors_;
    BlockList successors_;
    ZCCIRFunction* parent_ = nullptr;
};

// ==================== ZCC-IR 函数 ====================

// ZCC-IR 函数
class ZCCIRFunction {
public:
    using BlockList = std::vector<std::unique_ptr<ZCCIRBlock>>;
    using ArgumentList = std::vector<std::pair<std::string, ZCCTypePtr>>;

    ZCCIRFunction(std::string name, ZCCTypePtr returnType, ArgumentList args)
        : name_(std::move(name)), returnType_(std::move(returnType)),
          arguments_(std::move(args)) {}

    const std::string& getName() const { return name_; }
    const ZCCTypePtr& getReturnType() const { return returnType_; }
    const ArgumentList& getArguments() const { return arguments_; }

    // 基本块
    ZCCIRBlock* createBlock(const std::string& label);
    const BlockList& getBlocks() const { return blocks_; }
    ZCCIRBlock* getEntryBlock() const { return blocks_.front().get(); }

    // 属性
    bool isExternal() const { return isExternal_; }
    void setExternal(bool ext) { isExternal_ = ext; }

    bool isInline() const { return isInline_; }
    void setInline(bool inl) { isInline_ = inl; }

    // 局部变量
    size_t addLocal(const std::string& name, const ZCCTypePtr& type);

private:
    std::string name_;
    ZCCTypePtr returnType_;
    ArgumentList arguments_;
    BlockList blocks_;
    bool isExternal_ = false;
    bool isInline_ = false;
    size_t localCount_ = 0;
};

// ==================== ZCC-IR 全局变量 ====================

// ZCC-IR 全局变量
class ZCCIRGlobal {
public:
    ZCCIRGlobal(std::string name, ZCCTypePtr type, std::shared_ptr<ZCCIRConstant> init)
        : name_(std::move(name)), type_(std::move(type)), initializer_(std::move(init)) {}

    const std::string& getName() const { return name_; }
    const ZCCTypePtr& getType() const { return type_; }
    const std::shared_ptr<ZCCIRConstant>& getInitializer() const { return initializer_; }

    bool isConstant() const { return isConstant_; }
    void setConstant(bool c) { isConstant_ = c; }

private:
    std::string name_;
    ZCCTypePtr type_;
    std::shared_ptr<ZCCIRConstant> initializer_;
    bool isConstant_ = false;
};

// ==================== ZCC-IR 模块 ====================

// ZCC-IR 模块元数据
struct ZCCIRMetadata {
    std::string version = "1.0";
    TargetPlatform target;
    std::string sourceFile;
    std::string moduleName;
    uint64_t compileTime = 0;
    std::string compilerVersion;
    std::unordered_map<std::string, std::string> customData;
};

// ZCC-IR 模块
class ZCCIRModule {
public:
    using FunctionList = std::vector<std::unique_ptr<ZCCIRFunction>>;
    using GlobalList = std::vector<std::unique_ptr<ZCCIRGlobal>>;
    using ConstantList = std::vector<std::shared_ptr<ZCCIRConstant>>;

    ZCCIRModule(std::string name) : name_(std::move(name)) {}

    const std::string& getName() const { return name_; }

    // 函数
    ZCCIRFunction* createFunction(const std::string& name, ZCCTypePtr returnType,
                                  ZCCIRFunction::ArgumentList args);
    const FunctionList& getFunctions() const { return functions_; }
    ZCCIRFunction* getFunction(const std::string& name);

    // 全局变量
    ZCCIRGlobal* createGlobal(const std::string& name, ZCCTypePtr type,
                              std::shared_ptr<ZCCIRConstant> init = nullptr);
    const GlobalList& getGlobals() const { return globals_; }

    // 常量
    std::shared_ptr<ZCCIRConstant> createConstant(ZCCIRConstant::ConstantValue value, ZCCTypePtr type);
    const ConstantList& getConstants() const { return constants_; }
    void addConstant(std::shared_ptr<ZCCIRConstant> c) { constants_.push_back(std::move(c)); }
    ZCCIRValue::ID getNextValueID() { return nextValueID_++; }

    // 元数据
    ZCCIRMetadata& getMetadata() { return metadata_; }
    const ZCCIRMetadata& getMetadata() const { return metadata_; }

    // 序列化
    std::string serialize() const;
    static std::unique_ptr<ZCCIRModule> deserialize(const std::string& ir);

private:
    std::string name_;
    FunctionList functions_;
    GlobalList globals_;
    ConstantList constants_;
    ZCCIRMetadata metadata_;
    
    ZCCIRValue::ID nextValueID_ = 0;
    ZCCIRInstruction::ID nextInstructionID_ = 0;
};

// ==================== ZCC-IR 访问者 ====================

// ZCC-IR 访问者基类
class ZCCIRVisitor {
public:
    virtual ~ZCCIRVisitor() = default;
    
    virtual void visitModule(ZCCIRModule& module) = 0;
    virtual void visitFunction(ZCCIRFunction& func) = 0;
    virtual void visitBlock(ZCCIRBlock& block) = 0;
    virtual void visitInstruction(ZCCIRInstruction& instr) = 0;
    virtual void visitValue(ZCCIRValue& value) = 0;
};

// ZCC-IR 常量访问者
class ZCCIRConstVisitor {
public:
    virtual ~ZCCIRConstVisitor() = default;
    
    virtual void visitModule(const ZCCIRModule& module) = 0;
    virtual void visitFunction(const ZCCIRFunction& func) = 0;
    virtual void visitBlock(const ZCCIRBlock& block) = 0;
    virtual void visitInstruction(const ZCCIRInstruction& instr) = 0;
    virtual void visitValue(const ZCCIRValue& value) = 0;
};

} // namespace zcc

#endif // ZCC_ZCCIR_HPP

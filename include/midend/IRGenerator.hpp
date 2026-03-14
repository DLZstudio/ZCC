#ifndef ZCC_IRGENERATOR_HPP
#define ZCC_IRGENERATOR_HPP

#include "ZCCIR.hpp"
#include "AST.hpp"
#include "Types.hpp"
#include "Diagnostics.hpp"
#include <unordered_map>
#include <stack>

namespace zcc {

// IR 生成器配置
struct IRGeneratorConfig {
    bool emitDebugInfo = true;
    bool optimize = false;
    std::string moduleName;
    TargetPlatform target;
};

// 作用域信息
struct ScopeInfo {
    std::unordered_map<std::string, ZCCIRValue::ID> variables;
    ScopeInfo* parent = nullptr;
    bool isFunction = false;
};

// 控制流信息
struct ControlFlowInfo {
    ZCCIRBlock* loopStart = nullptr;
    ZCCIRBlock* loopEnd = nullptr;
    ZCCIRBlock* functionEnd = nullptr;
};

// IR 生成器类
class IRGenerator : public ASTVisitor {
public:
    IRGenerator(IRGeneratorConfig config = IRGeneratorConfig());
    
    // 生成 IR
    std::unique_ptr<ZCCIRModule> generate(ProgramPtr program);
    
    // 获取错误
    const std::vector<Diagnostic>& getErrors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }
    
    // AST 访问
    void visitProgram(Program* program) override;
    void visitFunctionDecl(FunctionDecl* decl) override;
    void visitVariableDecl(VariableDecl* decl) override;
    void visitClassDecl(ClassDecl* decl) override;
    void visitBlockStmt(BlockStmt* stmt) override;
    void visitExpressionStmt(ExpressionStmt* stmt) override;
    void visitIfStmt(IfStmt* stmt) override;
    void visitForStmt(ForStmt* stmt) override;
    void visitForEachStmt(ForEachStmt* stmt) override;
    void visitWhileStmt(WhileStmt* stmt) override;
    void visitReturnStmt(ReturnStmt* stmt) override;
    void visitBreakStmt(BreakStmt* stmt) override;
    void visitContinueStmt(ContinueStmt* stmt) override;
    void visitThrowStmt(ThrowStmt* stmt) override;
    void visitTryStmt(TryStmt* stmt) override;
    void visitSwitchStmt(SwitchStmt* stmt) override;
    void visitLiteralExpr(LiteralExpr* expr) override;
    void visitIdentifierExpr(IdentifierExpr* expr) override;
    void visitBinaryExpr(BinaryExpr* expr) override;
    void visitUnaryExpr(UnaryExpr* expr) override;
    void visitAssignExpr(AssignExpr* expr) override;
    void visitCallExpr(CallExpr* expr) override;
    void visitMemberExpr(MemberExpr* expr) override;
    void visitIndexExpr(IndexExpr* expr) override;
    void visitArrayExpr(ArrayExpr* expr) override;
    void visitObjectExpr(ObjectExpr* expr) override;
    void visitFunctionExpr(FunctionExpr* expr) override;
    void visitArrowFunctionExpr(ArrowFunctionExpr* expr) override;
    void visitTernaryExpr(TernaryExpr* expr) override;
    
private:
    // 工具方法
    void enterScope(bool isFunction = false);
    void leaveScope();
    void declareVariable(const std::string& name, ZCCIRValue::ID id);
    ZCCIRValue::ID lookupVariable(const std::string& name);
    bool isVariableDeclared(const std::string& name);
    
    // 类型转换
    ZCCTypePtr convertType(const std::string& typeName);
    ZCCTypePtr inferType(ZCCIRValue::ID value);
    
    // 指令生成
    ZCCIRValue::ID emitInstruction(ZCCIROpcode opcode, 
                                   std::vector<ZCCIRValue::ID> operands,
                                   ZCCTypePtr type = nullptr);
    void emitTerminator(ZCCIROpcode opcode, std::vector<ZCCIRValue::ID> operands);
    void emitJump(ZCCIRBlock* target);
    void emitCondJump(ZCCIRValue::ID condition, ZCCIRBlock* ifTrue, ZCCIRBlock* ifFalse);
    
    // 常量生成
    ZCCIRValue::ID emitConstant(ZCCIRConstant::ConstantValue value, ZCCTypePtr type);
    
    // 表达式生成
    ZCCIRValue::ID generateExpression(ExprPtr expr);
    void generateStatement(StmtPtr stmt);
    
    // 错误处理
    void error(const SourceLocation& loc, const std::string& message);
    
    // 成员
    IRGeneratorConfig config_;
    std::unique_ptr<ZCCIRModule> module_;
    ZCCIRFunction* currentFunction_ = nullptr;
    ZCCIRBlock* currentBlock_ = nullptr;
    ScopeInfo* currentScope_ = nullptr;
    ControlFlowInfo controlFlow_;
    
    std::vector<Diagnostic> errors_;
    ZCCIRValue::ID nextValueID_ = 0;
    ZCCIRInstruction::ID nextInstructionID_ = 0;
    
    // 函数和类型缓存
    std::unordered_map<std::string, ZCCIRFunction*> functionCache_;
    std::unordered_map<std::string, ZCCTypePtr> typeCache_;
};

// ==================== 实现 ====================

inline IRGenerator::IRGenerator(IRGeneratorConfig config)
    : config_(std::move(config)) {}

inline void IRGenerator::enterScope(bool isFunction) {
    auto* newScope = new ScopeInfo();
    newScope->parent = currentScope_;
    newScope->isFunction = isFunction;
    currentScope_ = newScope;
}

inline void IRGenerator::leaveScope() {
    if (currentScope_) {
        ScopeInfo* parent = currentScope_->parent;
        delete currentScope_;
        currentScope_ = parent;
    }
}

inline void IRGenerator::declareVariable(const std::string& name, ZCCIRValue::ID id) {
    if (currentScope_) {
        currentScope_->variables[name] = id;
    }
}

inline ZCCIRValue::ID IRGenerator::lookupVariable(const std::string& name) {
    ScopeInfo* scope = currentScope_;
    while (scope) {
        auto it = scope->variables.find(name);
        if (it != scope->variables.end()) {
            return it->second;
        }
        scope = scope->parent;
    }
    return 0; // 未找到
}

inline bool IRGenerator::isVariableDeclared(const std::string& name) {
    return lookupVariable(name) != 0;
}

inline void IRGenerator::error(const SourceLocation& loc, const std::string& message) {
    errors_.push_back(Diagnostic::error(message, loc));
}

inline ZCCIRValue::ID IRGenerator::emitInstruction(
    ZCCIROpcode opcode,
    std::vector<ZCCIRValue::ID> operands,
    ZCCTypePtr type) {
    
    if (!currentBlock_) return 0;
    
    auto instr = std::make_unique<ZCCIRInstruction>(
        opcode, type ? type : getVoid(), nextInstructionID_++);
    
    // 添加操作数
    for (auto id : operands) {
        // TODO: 从值 ID 创建操作数
        instr->addOperand(ZCCIROperand());
    }
    
    ZCCIRValue::ID resultID = 0;
    if (type && type->getName() != "void") {
        resultID = nextValueID_++;
        // TODO: 创建临时值
    }
    
    currentBlock_->addInstruction(std::move(instr));
    return resultID;
}

inline void IRGenerator::emitTerminator(ZCCIROpcode opcode, 
                                        std::vector<ZCCIRValue::ID> operands) {
    if (!currentBlock_) return;
    
    auto instr = std::make_unique<ZCCIRInstruction>(
        opcode, getVoid(), nextInstructionID_++);
    
    for (auto id : operands) {
        instr->addOperand(ZCCIROperand());
    }
    
    currentBlock_->addInstruction(std::move(instr));
}

inline void IRGenerator::emitJump(ZCCIRBlock* target) {
    if (!currentBlock_ || !target) return;
    
    emitTerminator(ZCCIROpcode::JMP, {});
    currentBlock_->addSuccessor(target);
    target->addPredecessor(currentBlock_);
}

inline void IRGenerator::emitCondJump(ZCCIRValue::ID condition,
                                      ZCCIRBlock* ifTrue,
                                      ZCCIRBlock* ifFalse) {
    if (!currentBlock_ || !ifTrue || !ifFalse) return;
    
    emitTerminator(ZCCIROpcode::JMP_IF, {condition});
    currentBlock_->addSuccessor(ifTrue);
    currentBlock_->addSuccessor(ifFalse);
    ifTrue->addPredecessor(currentBlock_);
    ifFalse->addPredecessor(currentBlock_);
}

inline ZCCIRValue::ID IRGenerator::emitConstant(
    ZCCIRConstant::ConstantValue value,
    ZCCTypePtr type) {
    
    if (!module_) return 0;
    
    auto constant = std::make_shared<ZCCIRConstant>(
        std::move(value), type ? type : getAny(), nextValueID_++);
    
    module_->constants_.push_back(constant);
    return constant->getID();
}

inline ZCCIRValue::ID IRGenerator::generateExpression(ExprPtr expr) {
    if (!expr) return 0;
    expr->accept(*this);
    // TODO: 返回生成的值 ID
    return 0;
}

inline void IRGenerator::generateStatement(StmtPtr stmt) {
    if (!stmt) return;
    stmt->accept(*this);
}

// AST 访问实现
inline void IRGenerator::visitProgram(Program* program) {
    module_ = std::make_unique<ZCCIRModule>(config_.moduleName);
    module_->metadata_.sourceFile = program->sourceFilename;
    module_->metadata_.target = config_.target;
    
    // 首先处理所有声明
    for (auto& decl : program->declarations) {
        decl->accept(*this);
    }
}

inline void IRGenerator::visitFunctionDecl(FunctionDecl* decl) {
    // 创建函数类型
    std::vector<std::pair<std::string, ZCCTypePtr>> params;
    for (const auto& param : decl->parameters) {
        params.push_back({param, getAny()}); // TODO: 推断类型
    }
    
    auto funcType = std::make_shared<FunctionType>(getAny(), std::move(params));
    auto func = module_->createFunction(decl->name, getAny(), std::move(params));
    
    functionCache_[decl->name] = func;
    currentFunction_ = func;
    
    // 创建入口块
    enterScope(true);
    
    // 声明参数
    for (size_t i = 0; i < decl->parameters.size(); ++i) {
        const auto& param = decl->parameters[i];
        // TODO: 创建参数值
        declareVariable(param, i);
    }
    
    // 生成函数体
    if (decl->body) {
        currentBlock_ = func->createBlock("entry");
        generateStatement(decl->body);
        
        // 如果没有显式返回，添加隐式返回
        if (!currentBlock_->isTerminated()) {
            emitTerminator(ZCCIROpcode::RET, {});
        }
    }
    
    leaveScope();
    currentFunction_ = nullptr;
    currentBlock_ = nullptr;
}

inline void IRGenerator::visitVariableDecl(VariableDecl* decl) {
    // 生成初始化表达式
    ZCCIRValue::ID initValue = 0;
    if (decl->initializer) {
        initValue = generateExpression(decl->initializer);
    } else {
        // 默认初始化为 null
        initValue = emitConstant(std::monostate{}, getAny());
    }
    
    // 创建变量
    ZCCIRValue::ID varID = nextValueID_++;
    declareVariable(decl->name, varID);
    
    // 存储初始值
    if (initValue) {
        emitInstruction(ZCCIROpcode::STORE, {varID, initValue});
    }
}

inline void IRGenerator::visitBlockStmt(BlockStmt* stmt) {
    enterScope();
    
    for (auto& s : stmt->statements) {
        generateStatement(s);
    }
    
    leaveScope();
}

inline void IRGenerator::visitExpressionStmt(ExpressionStmt* stmt) {
    generateExpression(stmt->expression);
}

inline void IRGenerator::visitIfStmt(IfStmt* stmt) {
    // 生成条件
    ZCCIRValue::ID condition = generateExpression(stmt->condition);
    
    // 创建块
    auto* thenBlock = currentFunction_->createBlock("if.then");
    auto* elseBlock = stmt->elseBranch ? 
        currentFunction_->createBlock("if.else") : nullptr;
    auto* endBlock = currentFunction_->createBlock("if.end");
    
    // 条件跳转
    emitCondJump(condition, thenBlock, elseBlock ? elseBlock : endBlock);
    
    // Then 分支
    currentBlock_ = thenBlock;
    generateStatement(stmt->thenBranch);
    if (!currentBlock_->isTerminated()) {
        emitJump(endBlock);
    }
    
    // Else 分支
    if (elseBlock) {
        currentBlock_ = elseBlock;
        generateStatement(stmt->elseBranch);
        if (!currentBlock_->isTerminated()) {
            emitJump(endBlock);
        }
    }
    
    // 结束块
    currentBlock_ = endBlock;
}

inline void IRGenerator::visitForStmt(ForStmt* stmt) {
    auto* initBlock = currentFunction_->createBlock("for.init");
    auto* condBlock = currentFunction_->createBlock("for.cond");
    auto* bodyBlock = currentFunction_->createBlock("for.body");
    auto* incrBlock = currentFunction_->createBlock("for.incr");
    auto* endBlock = currentFunction_->createBlock("for.end");
    
    // 初始化
    emitJump(initBlock);
    currentBlock_ = initBlock;
    if (stmt->initializer) {
        generateStatement(stmt->initializer);
    }
    emitJump(condBlock);
    
    // 条件
    currentBlock_ = condBlock;
    ZCCIRValue::ID condition = 0;
    if (stmt->condition) {
        condition = generateExpression(stmt->condition);
    } else {
        condition = emitConstant(true, getBool());
    }
    emitCondJump(condition, bodyBlock, endBlock);
    
    // 循环体
    controlFlow_.loopStart = condBlock;
    controlFlow_.loopEnd = endBlock;
    currentBlock_ = bodyBlock;
    generateStatement(stmt->body);
    if (!currentBlock_->isTerminated()) {
        emitJump(incrBlock);
    }
    
    // 增量
    currentBlock_ = incrBlock;
    if (stmt->increment) {
        generateExpression(stmt->increment);
    }
    emitJump(condBlock);
    
    // 结束
    currentBlock_ = endBlock;
    controlFlow_.loopStart = nullptr;
    controlFlow_.loopEnd = nullptr;
}

inline void IRGenerator::visitWhileStmt(WhileStmt* stmt) {
    auto* condBlock = currentFunction_->createBlock("while.cond");
    auto* bodyBlock = currentFunction_->createBlock("while.body");
    auto* endBlock = currentFunction_->createBlock("while.end");
    
    // 条件
    emitJump(condBlock);
    currentBlock_ = condBlock;
    ZCCIRValue::ID condition = generateExpression(stmt->condition);
    emitCondJump(condition, bodyBlock, endBlock);
    
    // 循环体
    controlFlow_.loopStart = condBlock;
    controlFlow_.loopEnd = endBlock;
    currentBlock_ = bodyBlock;
    generateStatement(stmt->body);
    if (!currentBlock_->isTerminated()) {
        emitJump(condBlock);
    }
    
    // 结束
    currentBlock_ = endBlock;
    controlFlow_.loopStart = nullptr;
    controlFlow_.loopEnd = nullptr;
}

inline void IRGenerator::visitReturnStmt(ReturnStmt* stmt) {
    ZCCIRValue::ID returnValue = 0;
    if (stmt->value) {
        returnValue = generateExpression(stmt->value);
    }
    
    emitTerminator(ZCCIROpcode::RET, returnValue ? std::vector{returnValue} : std::vector<ZCCIRValue::ID>{});
}

inline void IRGenerator::visitBreakStmt(BreakStmt* stmt) {
    if (!controlFlow_.loopEnd) {
        error(stmt->location, "break statement outside of loop");
        return;
    }
    emitJump(controlFlow_.loopEnd);
}

inline void IRGenerator::visitContinueStmt(ContinueStmt* stmt) {
    if (!controlFlow_.loopStart) {
        error(stmt->location, "continue statement outside of loop");
        return;
    }
    emitJump(controlFlow_.loopStart);
}

inline void IRGenerator::visitLiteralExpr(LiteralExpr* expr) {
    ZCCTypePtr type = nullptr;
    ZCCIRConstant::ConstantValue value;
    
    switch (expr->literalType) {
        case TokenType::NUMBER:
            if (auto num = std::get_if<double>(&expr->value)) {
                value = *num;
                type = getF64();
            }
            break;
        case TokenType::STRING:
            if (auto str = std::get_if<std::string>(&expr->value)) {
                value = *str;
                type = getString();
            }
            break;
        case TokenType::TRUE:
        case TokenType::FALSE:
            value = expr->literalType == TokenType::TRUE;
            type = getBool();
            break;
        case TokenType::NULL_:
            value = std::monostate{};
            type = getAny();
            break;
        default:
            value = std::monostate{};
            type = getAny();
            break;
    }
    
    emitConstant(value, type);
}

inline void IRGenerator::visitBinaryExpr(BinaryExpr* expr) {
    ZCCIRValue::ID left = generateExpression(expr->left);
    ZCCIRValue::ID right = generateExpression(expr->right);
    
    ZCCIROpcode opcode = ZCCIROpcode::NOP;
    
    switch (expr->op) {
        case TokenType::PLUS: opcode = ZCCIROpcode::ADD; break;
        case TokenType::MINUS: opcode = ZCCIROpcode::SUB; break;
        case TokenType::STAR: opcode = ZCCIROpcode::MUL; break;
        case TokenType::SLASH: opcode = ZCCIROpcode::DIV; break;
        case TokenType::PERCENT: opcode = ZCCIROpcode::MOD; break;
        case TokenType::STAR_STAR: opcode = ZCCIROpcode::POW; break;
        case TokenType::EQUAL_EQUAL:
        case TokenType::EQUAL_EQUAL_EQUAL:
            opcode = ZCCIROpcode::CMP_EQ; break;
        case TokenType::BANG_EQUAL:
        case TokenType::BANG_EQUAL_EQUAL:
            opcode = ZCCIROpcode::CMP_NE; break;
        case TokenType::LESS: opcode = ZCCIROpcode::CMP_LT; break;
        case TokenType::GREATER: opcode = ZCCIROpcode::CMP_GT; break;
        case TokenType::LESS_EQUAL: opcode = ZCCIROpcode::CMP_LE; break;
        case TokenType::GREATER_EQUAL: opcode = ZCCIROpcode::CMP_GE; break;
        case TokenType::AMPERSAND_AMPERSAND: opcode = ZCCIROpcode::AND; break;
        case TokenType::PIPE_PIPE: opcode = ZCCIROpcode::OR; break;
        default:
            error(expr->location, "Unknown binary operator");
            return;
    }
    
    emitInstruction(opcode, {left, right});
}

inline void IRGenerator::visitUnaryExpr(UnaryExpr* expr) {
    ZCCIRValue::ID operand = generateExpression(expr->operand);
    
    ZCCIROpcode opcode = ZCCIROpcode::NOP;
    
    switch (expr->op) {
        case TokenType::MINUS: opcode = ZCCIROpcode::NEG; break;
        case TokenType::BANG: opcode = ZCCIROpcode::NOT; break;
        case TokenType::TILDE: opcode = ZCCIROpcode::NOT; break;
        default:
            error(expr->location, "Unknown unary operator");
            return;
    }
    
    emitInstruction(opcode, {operand});
}

inline void IRGenerator::visitAssignExpr(AssignExpr* expr) {
    ZCCIRValue::ID varID = lookupVariable(expr->target);
    if (varID == 0) {
        error(expr->location, "Undefined variable: " + expr->target);
        return;
    }
    
    ZCCIRValue::ID value = generateExpression(expr->value);
    emitInstruction(ZCCIROpcode::STORE, {varID, value});
}

inline void IRGenerator::visitCallExpr(CallExpr* expr) {
    // 生成参数
    std::vector<ZCCIRValue::ID> args;
    for (auto& arg : expr->arguments) {
        args.push_back(generateExpression(arg));
    }
    
    // 生成调用
    ZCCIRValue::ID callee = generateExpression(expr->callee);
    
    std::vector<ZCCIRValue::ID> operands = {callee};
    operands.insert(operands.end(), args.begin(), args.end());
    
    emitInstruction(ZCCIROpcode::CALL, operands);
}

// 其他访问方法可以类似实现...
inline void IRGenerator::visitClassDecl(ClassDecl* decl) {
    // TODO: 实现类声明的 IR 生成
}

inline void IRGenerator::visitForEachStmt(ForEachStmt* stmt) {
    // TODO: 实现 for-each 循环
}

inline void IRGenerator::visitThrowStmt(ThrowStmt* stmt) {
    // TODO: 实现异常
}

inline void IRGenerator::visitTryStmt(TryStmt* stmt) {
    // TODO: 实现异常处理
}

inline void IRGenerator::visitSwitchStmt(SwitchStmt* stmt) {
    // TODO: 实现 switch 语句
}

inline void IRGenerator::visitMemberExpr(MemberExpr* expr) {
    // TODO: 实现成员访问
}

inline void IRGenerator::visitIndexExpr(IndexExpr* expr) {
    // TODO: 实现索引访问
}

inline void IRGenerator::visitArrayExpr(ArrayExpr* expr) {
    // TODO: 实现数组字面量
}

inline void IRGenerator::visitObjectExpr(ObjectExpr* expr) {
    // TODO: 实现对象字面量
}

inline void IRGenerator::visitFunctionExpr(FunctionExpr* expr) {
    // TODO: 实现函数字面量
}

inline void IRGenerator::visitArrowFunctionExpr(ArrowFunctionExpr* expr) {
    // TODO: 实现箭头函数
}

inline void IRGenerator::visitTernaryExpr(TernaryExpr* expr) {
    // TODO: 实现三元运算符
}

inline void IRGenerator::visitIdentifierExpr(IdentifierExpr* expr) {
    ZCCIRValue::ID varID = lookupVariable(expr->name);
    if (varID == 0) {
        // 可能是全局变量或函数
        // TODO: 查找全局符号
        error(expr->location, "Undefined variable: " + expr->name);
        return;
    }
    
    // TODO: 加载变量值
    emitInstruction(ZCCIROpcode::LOAD, {varID});
}

} // namespace zcc

#endif // ZCC_IRGENERATOR_HPP

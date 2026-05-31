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
    void visitModuleDecl(ModuleDecl* decl) override;
    void visitImportDecl(ImportDecl* decl) override;
    void visitExportDecl(ExportDecl* decl) override;
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
    
    // 当前表达式结果（用于 generateExpression 返回值）
    ZCCIRValue::ID currentExprResult_ = 0;
    
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

inline std::unique_ptr<ZCCIRModule> IRGenerator::generate(ProgramPtr program) {
    if (!program) return nullptr;
    visitProgram(program.get());
    return std::move(module_);
}

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
    
    module_->addConstant(constant);
    return constant->getID();
}

inline ZCCIRValue::ID IRGenerator::generateExpression(ExprPtr expr) {
    if (!expr) return 0;
    currentExprResult_ = 0;
    
    // Manual dispatch - AST nodes don't have accept()
    if (auto* e = dynamic_cast<LiteralExpr*>(expr.get()))       visitLiteralExpr(e);
    else if (auto* e = dynamic_cast<IdentifierExpr*>(expr.get())) visitIdentifierExpr(e);
    else if (auto* e = dynamic_cast<BinaryExpr*>(expr.get()))    visitBinaryExpr(e);
    else if (auto* e = dynamic_cast<UnaryExpr*>(expr.get()))     visitUnaryExpr(e);
    else if (auto* e = dynamic_cast<AssignExpr*>(expr.get()))    visitAssignExpr(e);
    else if (auto* e = dynamic_cast<CallExpr*>(expr.get()))      visitCallExpr(e);
    else if (auto* e = dynamic_cast<MemberExpr*>(expr.get()))    visitMemberExpr(e);
    else if (auto* e = dynamic_cast<IndexExpr*>(expr.get()))     visitIndexExpr(e);
    else if (auto* e = dynamic_cast<ArrayExpr*>(expr.get()))     visitArrayExpr(e);
    else if (auto* e = dynamic_cast<ObjectExpr*>(expr.get()))    visitObjectExpr(e);
    else if (auto* e = dynamic_cast<FunctionExpr*>(expr.get()))  visitFunctionExpr(e);
    else if (auto* e = dynamic_cast<ArrowFunctionExpr*>(expr.get())) visitArrowFunctionExpr(e);
    else if (auto* e = dynamic_cast<TernaryExpr*>(expr.get()))   visitTernaryExpr(e);
    
    return currentExprResult_;
}

inline void IRGenerator::generateStatement(StmtPtr stmt) {
    if (!stmt) return;
    
    if (auto* s = dynamic_cast<ExpressionStmt*>(stmt.get()))     visitExpressionStmt(s);
    else if (auto* s = dynamic_cast<BlockStmt*>(stmt.get()))     visitBlockStmt(s);
    else if (auto* s = dynamic_cast<IfStmt*>(stmt.get()))        visitIfStmt(s);
    else if (auto* s = dynamic_cast<ForStmt*>(stmt.get()))       visitForStmt(s);
    else if (auto* s = dynamic_cast<WhileStmt*>(stmt.get()))     visitWhileStmt(s);
    else if (auto* s = dynamic_cast<ForEachStmt*>(stmt.get()))   visitForEachStmt(s);
    else if (auto* s = dynamic_cast<ReturnStmt*>(stmt.get()))    visitReturnStmt(s);
    else if (auto* s = dynamic_cast<BreakStmt*>(stmt.get()))     visitBreakStmt(s);
    else if (auto* s = dynamic_cast<ContinueStmt*>(stmt.get()))  visitContinueStmt(s);
    else if (auto* s = dynamic_cast<ThrowStmt*>(stmt.get()))     visitThrowStmt(s);
    else if (auto* s = dynamic_cast<TryStmt*>(stmt.get()))       visitTryStmt(s);
    else if (auto* s = dynamic_cast<SwitchStmt*>(stmt.get()))    visitSwitchStmt(s);
}

// AST 访问实现
inline void IRGenerator::visitProgram(Program* program) {
    module_ = std::make_unique<ZCCIRModule>(config_.moduleName);
    module_->getMetadata().sourceFile = program->sourceFilename;
    module_->getMetadata().target = config_.target;
    
    // 检查是否有顶层函数声明
    bool hasExplicitMain = false;
    for (auto& decl : program->declarations) {
        if (auto* fd = dynamic_cast<FunctionDecl*>(decl.get())) {
            if (fd->name == "main") hasExplicitMain = true;
        }
    }
    
    // 如果没有显式 main，为顶层变量创建 main 函数包装
    if (!hasExplicitMain) {
        std::vector<std::pair<std::string, ZCCTypePtr>> mainParams;
        auto* mainFunc = module_->createFunction("main", getI64(), std::move(mainParams));
        currentFunction_ = mainFunc;
        enterScope(true);
        currentBlock_ = mainFunc->createBlock("entry");
        
        for (auto& decl : program->declarations) {
            // 跳过函数声明 — 它们会在显式 main 分支中处理
            if (dynamic_cast<FunctionDecl*>(decl.get())) continue;
            if (auto* vd = dynamic_cast<VariableDecl*>(decl.get())) {
                visitVariableDecl(vd);
            } else if (auto* cd = dynamic_cast<ClassDecl*>(decl.get())) {
                visitClassDecl(cd);
            }
        }
        
        // 隐式 return 0
        emitTerminator(ZCCIROpcode::RET, {});
        
        leaveScope();
        currentFunction_ = nullptr;
        currentBlock_ = nullptr;
    } else {
        // 已有 main 函数，正常处理
        for (auto& decl : program->declarations) {
            if (auto* fd = dynamic_cast<FunctionDecl*>(decl.get()))
                visitFunctionDecl(fd);
            else if (auto* vd = dynamic_cast<VariableDecl*>(decl.get()))
                visitVariableDecl(vd);
            else if (auto* cd = dynamic_cast<ClassDecl*>(decl.get()))
                visitClassDecl(cd);
        }
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
        auto bodyStmt = std::make_shared<BlockStmt>(*decl->body);
        generateStatement(bodyStmt);
        
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

// 模块声明 - 不需要生成代码
inline void IRGenerator::visitModuleDecl(ModuleDecl* decl) {
    // 模块声明不影响 IR 生成
}

// 导入声明
inline void IRGenerator::visitImportDecl(ImportDecl* decl) {
    // 导入在编译时处理，不影响 IR
}

// 导出声明
inline void IRGenerator::visitExportDecl(ExportDecl* decl) {
    // 导出通过模块系统处理
}

// 类声明 - 生成类定义和构造函数的 IR
inline void IRGenerator::visitClassDecl(ClassDecl* decl) {
    // 为类创建一个新的函数（构造函数）
    auto* classFunc = module_->createFunction(decl->name + "_ctor", getVoid(), {});
    functionCache_[decl->name] = classFunc;
    
    // 生成类的 IR 表示
    ZCCIRValue::ID classObj = emitInstruction(ZCCIROpcode::OBJECT_NEW, {});
    declareVariable(decl->name, classObj);
    
    // 处理父类
    if (!decl->superClass.empty()) {
        auto superIt = functionCache_.find(decl->superClass);
        if (superIt != functionCache_.end()) {
            emitInstruction(ZCCIROpcode::CALL, {});
        }
    }
    
    // 处理方法
    for (auto& member : decl->members) {
        if (auto* funcDecl = dynamic_cast<FunctionDecl*>(member.get())) {
            visitFunctionDecl(funcDecl);
        } else if (auto* varDecl = dynamic_cast<VariableDecl*>(member.get())) {
            visitVariableDecl(varDecl);
        }
    }
}

// for-each 循环
inline void IRGenerator::visitForEachStmt(ForEachStmt* stmt) {
    // 生成可迭代对象
    ZCCIRValue::ID iterable = generateExpression(stmt->iterable);
    ZCCIRValue::ID length = emitInstruction(ZCCIROpcode::ARRAY_LEN, {iterable});
    
    // 创建块
    auto* initBlock = currentFunction_->createBlock("foreach.init");
    auto* condBlock = currentFunction_->createBlock("foreach.cond");
    auto* bodyBlock = currentFunction_->createBlock("foreach.body");
    auto* incrBlock = currentFunction_->createBlock("foreach.incr");
    auto* endBlock = currentFunction_->createBlock("foreach.end");
    
    // 初始化索引
    emitJump(initBlock);
    currentBlock_ = initBlock;
    ZCCIRValue::ID indexVar = nextValueID_++;
    emitConstant(static_cast<int64_t>(0), getI64());
    emitInstruction(ZCCIROpcode::STORE, {indexVar, 0UL});
    emitJump(condBlock);
    
    // 条件检查
    currentBlock_ = condBlock;
    ZCCIRValue::ID idxLoaded = emitInstruction(ZCCIROpcode::LOAD, {indexVar});
    ZCCIRValue::ID cond = emitInstruction(ZCCIROpcode::CMP_LT, {idxLoaded, length});
    emitCondJump(cond, bodyBlock, endBlock);
    
    // 循环体
    controlFlow_.loopStart = condBlock;
    controlFlow_.loopEnd = endBlock;
    currentBlock_ = bodyBlock;
    
    // 获取当前元素
    ZCCIRValue::ID element = emitInstruction(ZCCIROpcode::ARRAY_GET, {iterable, idxLoaded});
    declareVariable(stmt->variable, element);
    
    if (stmt->body) {
        generateStatement(std::make_shared<BlockStmt>(*stmt->body));
    }
    
    if (!currentBlock_->isTerminated()) {
        emitJump(incrBlock);
    }
    
    // 增量
    currentBlock_ = incrBlock;
    emitInstruction(ZCCIROpcode::INC, {indexVar});
    emitJump(condBlock);
    
    currentBlock_ = endBlock;
    controlFlow_.loopStart = nullptr;
    controlFlow_.loopEnd = nullptr;
}

// throw 语句
inline void IRGenerator::visitThrowStmt(ThrowStmt* stmt) {
    ZCCIRValue::ID exception = generateExpression(stmt->exception);
    emitTerminator(ZCCIROpcode::THROW, {exception});
}

// try-catch-finally
inline void IRGenerator::visitTryStmt(TryStmt* stmt) {
    auto* tryBlock = currentFunction_->createBlock("try.body");
    auto* catchBlock = stmt->catchBlock ? currentFunction_->createBlock("try.catch") : nullptr;
    auto* finallyBlock = stmt->finallyBlock ? currentFunction_->createBlock("try.finally") : nullptr;
    auto* endBlock = currentFunction_->createBlock("try.end");
    
    // Try 块
    emitJump(tryBlock);
    currentBlock_ = tryBlock;
    generateStatement(std::make_shared<BlockStmt>(*stmt->tryBlock));
    if (!currentBlock_->isTerminated()) {
        if (finallyBlock) emitJump(finallyBlock);
        else emitJump(endBlock);
    }
    
    // Catch 块
    if (catchBlock && stmt->catchBlock) {
        currentBlock_ = catchBlock;
        if (!stmt->catchVariable.empty()) {
            ZCCIRValue::ID exVar = nextValueID_++;
            declareVariable(stmt->catchVariable, exVar);
        }
        generateStatement(std::make_shared<BlockStmt>(*stmt->catchBlock));
        if (!currentBlock_->isTerminated()) {
            if (finallyBlock) emitJump(finallyBlock);
            else emitJump(endBlock);
        }
    }
    
    // Finally 块
    if (finallyBlock && stmt->finallyBlock) {
        currentBlock_ = finallyBlock;
        generateStatement(std::make_shared<BlockStmt>(*stmt->finallyBlock));
        if (!currentBlock_->isTerminated()) {
            emitJump(endBlock);
        }
    }
    
    currentBlock_ = endBlock;
}

// switch-case
inline void IRGenerator::visitSwitchStmt(SwitchStmt* stmt) {
    ZCCIRValue::ID discriminant = generateExpression(stmt->discriminant);
    auto* endBlock = currentFunction_->createBlock("switch.end");
    ZCCIRBlock* defaultBlock = endBlock;
    
    // 为每个 case 创建块并收集
    std::vector<ZCCIRBlock*> caseBlocks;
    std::vector<ZCCIRValue::ID> caseValues;
    for (size_t i = 0; i < stmt->cases.size(); ++i) {
        auto& cs = stmt->cases[i];
        if (cs.isDefault) {
            defaultBlock = currentFunction_->createBlock("switch.default");
            caseBlocks.push_back(defaultBlock);
            caseValues.push_back(0);
        } else {
            auto* block = currentFunction_->createBlock("switch.case" + std::to_string(i));
            caseBlocks.push_back(block);
            caseValues.push_back(generateExpression(cs.value));
        }
    }
    
    // 发射比较和条件跳转
    for (size_t i = 0; i < stmt->cases.size(); ++i) {
        if (stmt->cases[i].isDefault) continue;
        
        ZCCIRValue::ID eq = emitInstruction(ZCCIROpcode::CMP_EQ, {discriminant, caseValues[i]});
        auto* nextBlock = (i + 1 < stmt->cases.size()) ? 
            currentFunction_->createBlock("switch.test" + std::to_string(i + 1)) : 
            defaultBlock;
        emitCondJump(eq, caseBlocks[i], nextBlock);
        currentBlock_ = nextBlock;
    }
    
    if (defaultBlock != endBlock) {
        emitJump(defaultBlock);
    }
    
    // 生成 case 体
    for (size_t i = 0; i < stmt->cases.size(); ++i) {
        currentBlock_ = caseBlocks[i];
        for (auto& s : stmt->cases[i].statements) {
            generateStatement(s);
        }
        if (!currentBlock_->isTerminated()) {
            emitJump(endBlock);
        }
    }
    
    currentBlock_ = endBlock;
}

// 成员访问 obj.prop
inline void IRGenerator::visitMemberExpr(MemberExpr* expr) {
    ZCCIRValue::ID obj = generateExpression(expr->object);
    ZCCIRValue::ID propKey = emitConstant(expr->property, getString());
    emitInstruction(ZCCIROpcode::OBJECT_GET, {obj, propKey});
}

// 索引访问 obj[index]
inline void IRGenerator::visitIndexExpr(IndexExpr* expr) {
    ZCCIRValue::ID obj = generateExpression(expr->object);
    ZCCIRValue::ID idx = generateExpression(expr->index);
    emitInstruction(ZCCIROpcode::ARRAY_GET, {obj, idx});
}

// 数组字面量 [elem1, elem2, ...]
inline void IRGenerator::visitArrayExpr(ArrayExpr* expr) {
    std::vector<ZCCIRValue::ID> elements;
    for (auto& elem : expr->elements) {
        elements.push_back(generateExpression(elem));
    }
    
    // 创建数组
    ZCCIRValue::ID arr = emitInstruction(ZCCIROpcode::ARRAY_NEW, {});
    for (size_t i = 0; i < elements.size(); ++i) {
        ZCCIRValue::ID idx = emitConstant(static_cast<int64_t>(i), getI64());
        emitInstruction(ZCCIROpcode::ARRAY_SET, {arr, idx, elements[i]});
    }
}

// 对象字面量 { key: value, ... }
inline void IRGenerator::visitObjectExpr(ObjectExpr* expr) {
    ZCCIRValue::ID obj = emitInstruction(ZCCIROpcode::OBJECT_NEW, {});
    
    for (auto& prop : expr->properties) {
        ZCCIRValue::ID key = emitConstant(prop.key, getString());
        ZCCIRValue::ID value = generateExpression(prop.value);
        emitInstruction(ZCCIROpcode::OBJECT_SET, {obj, key, value});
    }
}

// 函数表达式 function name(params) { body }
inline void IRGenerator::visitFunctionExpr(FunctionExpr* expr) {
    // 创建内部函数
    std::vector<std::pair<std::string, ZCCTypePtr>> params;
    for (const auto& p : expr->parameters) {
        params.push_back({p, getAny()});
    }
    
    auto* innerFunc = module_->createFunction(
        expr->name.empty() ? "__lambda" : expr->name, getAny(), std::move(params));
    
    functionCache_[expr->name.empty() ? "__lambda" : expr->name] = innerFunc;
    
    // 保存当前函数上下文
    auto* savedFunc = currentFunction_;
    auto* savedBlock = currentBlock_;
    
    currentFunction_ = innerFunc;
    enterScope(true);
    
    // 声明参数
    for (size_t i = 0; i < expr->parameters.size(); ++i) {
        declareVariable(expr->parameters[i], i);
    }
    
    // 生成函数体
    currentBlock_ = innerFunc->createBlock("entry");
    for (auto& stmt : expr->body) {
        generateStatement(stmt);
    }
    if (!currentBlock_->isTerminated()) {
        emitTerminator(ZCCIROpcode::RET, {});
    }
    
    leaveScope();
    
    // 恢复上下文
    currentFunction_ = savedFunc;
    currentBlock_ = savedBlock;
}

// 箭头函数 (params) => expr 或 (params) => { body }
inline void IRGenerator::visitArrowFunctionExpr(ArrowFunctionExpr* expr) {
    // 创建内部函数
    std::vector<std::pair<std::string, ZCCTypePtr>> params;
    for (const auto& p : expr->parameters) {
        params.push_back({p, getAny()});
    }
    
    auto* innerFunc = module_->createFunction(
        "__arrow", getAny(), std::move(params));
    
    // 保存上下文
    auto* savedFunc = currentFunction_;
    auto* savedBlock = currentBlock_;
    
    currentFunction_ = innerFunc;
    enterScope(true);
    
    for (size_t i = 0; i < expr->parameters.size(); ++i) {
        declareVariable(expr->parameters[i], i);
    }
    
    currentBlock_ = innerFunc->createBlock("entry");
    
    if (expr->expressionBody) {
        // 表达式体: (x) => x + 1
        ZCCIRValue::ID result = generateExpression(expr->expressionBody);
        emitTerminator(ZCCIROpcode::RET, {result});
    } else {
        // 块体: (x) => { return x + 1; }
        for (auto& stmt : expr->blockBody) {
            generateStatement(stmt);
        }
        if (!currentBlock_->isTerminated()) {
            emitTerminator(ZCCIROpcode::RET, {});
        }
    }
    
    leaveScope();
    
    currentFunction_ = savedFunc;
    currentBlock_ = savedBlock;
}

// 三元运算符 condition ? then : else
inline void IRGenerator::visitTernaryExpr(TernaryExpr* expr) {
    ZCCIRValue::ID condition = generateExpression(expr->condition);
    
    auto* thenBlock = currentFunction_->createBlock("ternary.then");
    auto* elseBlock = currentFunction_->createBlock("ternary.else");
    auto* endBlock = currentFunction_->createBlock("ternary.end");
    
    emitCondJump(condition, thenBlock, elseBlock);
    
    // Then 分支
    ZCCIRValue::ID thenVal = 0;
    currentBlock_ = thenBlock;
    thenVal = generateExpression(expr->thenBranch);
    if (!currentBlock_->isTerminated()) emitJump(endBlock);
    
    // Else 分支
    ZCCIRValue::ID elseVal = 0;
    currentBlock_ = elseBlock;
    elseVal = generateExpression(expr->elseBranch);
    if (!currentBlock_->isTerminated()) emitJump(endBlock);
    
    currentBlock_ = endBlock;
    // Phi 节点合并两个分支的值
    ZCCIRValue::ID phiVal = emitInstruction(ZCCIROpcode::PHI, {thenVal, elseVal});
}

inline void IRGenerator::visitIdentifierExpr(IdentifierExpr* expr) {
    ZCCIRValue::ID varID = lookupVariable(expr->name);
    if (varID == 0) {
        // 可能是全局变量或函数
        error(expr->location, "Undefined variable: " + expr->name);
        return;
    }
    
    currentExprResult_ = emitInstruction(ZCCIROpcode::LOAD, {varID});
}

} // namespace zcc

#endif // ZCC_IRGENERATOR_HPP

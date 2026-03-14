#include "CodeGenerator.hpp"
#include <algorithm>
#include <cstring>

namespace zcc {

CodeGenerator::CodeGenerator()
    : currentFunction_(nullptr), scopeDepth_(0), localSlotCount_(0) {}

BytecodeModule CodeGenerator::generate(ProgramPtr program) {
    module_ = BytecodeModule();
    module_.sourceFilename = program->sourceFilename;
    module_.moduleName = "main";
    
    // 首先处理导入
    for (const auto& decl : program->declarations) {
        if (auto importDecl = std::dynamic_pointer_cast<ImportDecl>(decl)) {
            generateImportDecl(importDecl.get());
        }
    }
    
    // 处理所有声明
    for (const auto& decl : program->declarations) {
        generateDeclaration(decl);
    }
    
    // 查找主函数
    for (size_t i = 0; i < module_.functions.size(); ++i) {
        if (module_.functions[i].name == "main" || 
            module_.functions[i].name == "主函数") {
            module_.mainFunctionIndex = static_cast<uint16_t>(i);
            break;
        }
    }
    
    return module_;
}

int CodeGenerator::addConstant(Constant constant) {
    if (module_.constants.size() >= 0xFFFF) {
        error(SourceLocation(), "Too many constants");
        return 0;
    }
    module_.constants.push_back(std::move(constant));
    return static_cast<int>(module_.constants.size() - 1);
}

void CodeGenerator::emitByte(uint8_t byte) {
    if (currentFunction_) {
        currentFunction_->code.push_back(byte);
    } else {
        // 顶层代码，创建一个隐式函数
        if (module_.functions.empty()) {
            FunctionInfo func;
            func.name = "<top-level>";
            func.arity = 0;
            func.localCount = 0;
            func.upvalueCount = 0;
            func.isAsync = false;
            func.isVariadic = false;
            func.maxStackSlots = 1;
            func.lineCount = 0;
            module_.functions.push_back(func);
        }
        module_.functions[0].code.push_back(byte);
    }
}

void CodeGenerator::emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

void CodeGenerator::emitWord(uint16_t word) {
    emitByte(word & 0xFF);
    emitByte((word >> 8) & 0xFF);
}

void CodeGenerator::emitConstant(int index) {
    if (index < 0x100) {
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_CONST), static_cast<uint8_t>(index));
    } else {
        emitByte(static_cast<uint8_t>(OpCode::LOAD_CONST_BIG));
        emitWord(static_cast<uint16_t>(index));
    }
}

void CodeGenerator::emitOp(OpCode op) {
    emitByte(static_cast<uint8_t>(op));
}

void CodeGenerator::patchJump(size_t offset) {
    // 计算相对偏移
    size_t current = currentOffset();
    int16_t jumpOffset = static_cast<int16_t>(current - offset - 2);
    
    if (currentFunction_) {
        currentFunction_->code[offset] = jumpOffset & 0xFF;
        currentFunction_->code[offset + 1] = (jumpOffset >> 8) & 0xFF;
    } else if (!module_.functions.empty()) {
        module_.functions[0].code[offset] = jumpOffset & 0xFF;
        module_.functions[0].code[offset + 1] = (jumpOffset >> 8) & 0xFF;
    }
}

size_t CodeGenerator::currentOffset() const {
    if (currentFunction_) {
        return currentFunction_->code.size();
    } else if (!module_.functions.empty()) {
        return module_.functions[0].code.size();
    }
    return 0;
}

void CodeGenerator::beginScope() {
    scopes_.push_back(std::unordered_map<std::string, int>());
    scopeDepth_++;
}

void CodeGenerator::endScope() {
    if (scopes_.empty()) return;

    // 弹出当前作用域的变量
    scopes_.pop_back();
    scopeDepth_--;
}

void CodeGenerator::declareVariable(const std::string& name) {
    if (scopes_.empty()) return;

    scopes_.back()[name] = localSlotCount_++;
    if (currentFunction_) {
        currentFunction_->localNames.push_back(name);
    }
}

int CodeGenerator::resolveLocal(const std::string& name) {
    // 从内到外查找作用域 (使用 vector 的反向迭代)
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto fit = it->find(name);
        if (fit != it->end()) {
            return fit->second;
        }
    }
    return -1; // 未找到，可能是全局变量
}

void CodeGenerator::generateDeclaration(DeclPtr decl) {
    if (auto varDecl = std::dynamic_pointer_cast<VariableDecl>(decl)) {
        generateVariableDecl(varDecl.get());
    } else if (auto funcDecl = std::dynamic_pointer_cast<FunctionDecl>(decl)) {
        generateFunctionDecl(funcDecl.get());
    } else if (auto classDecl = std::dynamic_pointer_cast<ClassDecl>(decl)) {
        generateClassDecl(classDecl.get());
    } else if (auto moduleDecl = std::dynamic_pointer_cast<ModuleDecl>(decl)) {
        generateModuleDecl(moduleDecl.get());
    } else if (auto importDecl = std::dynamic_pointer_cast<ImportDecl>(decl)) {
        // 导入已经在前面处理了
    } else if (auto exportDecl = std::dynamic_pointer_cast<ExportDecl>(decl)) {
        generateExportDecl(exportDecl.get());
    } else {
        // 其他声明作为语句处理
        generateStatement(std::dynamic_pointer_cast<Statement>(decl));
    }
}

void CodeGenerator::generateStatement(StmtPtr stmt) {
    if (auto block = std::dynamic_pointer_cast<BlockStmt>(stmt)) {
        generateBlock(block.get());
    } else if (auto exprStmt = std::dynamic_pointer_cast<ExpressionStmt>(stmt)) {
        generateExpressionStmt(exprStmt.get());
    } else if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt)) {
        generateIfStmt(ifStmt.get());
    } else if (auto forStmt = std::dynamic_pointer_cast<ForStmt>(stmt)) {
        generateForStmt(forStmt.get());
    } else if (auto forEachStmt = std::dynamic_pointer_cast<ForEachStmt>(stmt)) {
        generateForEachStmt(forEachStmt.get());
    } else if (auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt)) {
        generateWhileStmt(whileStmt.get());
    } else if (auto returnStmt = std::dynamic_pointer_cast<ReturnStmt>(stmt)) {
        generateReturnStmt(returnStmt.get());
    } else if (auto breakStmt = std::dynamic_pointer_cast<BreakStmt>(stmt)) {
        generateBreakStmt(breakStmt.get());
    } else if (auto continueStmt = std::dynamic_pointer_cast<ContinueStmt>(stmt)) {
        generateContinueStmt(continueStmt.get());
    } else if (auto throwStmt = std::dynamic_pointer_cast<ThrowStmt>(stmt)) {
        generateThrowStmt(throwStmt.get());
    } else if (auto tryStmt = std::dynamic_pointer_cast<TryStmt>(stmt)) {
        generateTryStmt(tryStmt.get());
    } else if (auto switchStmt = std::dynamic_pointer_cast<SwitchStmt>(stmt)) {
        generateSwitchStmt(switchStmt.get());
    } else if (auto varDecl = std::dynamic_pointer_cast<VariableDecl>(stmt)) {
        generateVariableDecl(varDecl.get());
    }
}

void CodeGenerator::generateExpression(ExprPtr expr) {
    if (auto literal = std::dynamic_pointer_cast<LiteralExpr>(expr)) {
        generateLiteral(literal.get());
    } else if (auto ident = std::dynamic_pointer_cast<IdentifierExpr>(expr)) {
        generateIdentifier(ident.get());
    } else if (auto binary = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
        generateBinary(binary.get());
    } else if (auto unary = std::dynamic_pointer_cast<UnaryExpr>(expr)) {
        generateUnary(unary.get());
    } else if (auto assign = std::dynamic_pointer_cast<AssignExpr>(expr)) {
        generateAssign(assign.get());
    } else if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
        generateCall(call.get());
    } else if (auto member = std::dynamic_pointer_cast<MemberExpr>(expr)) {
        generateMember(member.get());
    } else if (auto index = std::dynamic_pointer_cast<IndexExpr>(expr)) {
        generateIndex(index.get());
    } else if (auto array = std::dynamic_pointer_cast<ArrayExpr>(expr)) {
        generateArray(array.get());
    } else if (auto object = std::dynamic_pointer_cast<ObjectExpr>(expr)) {
        generateObject(object.get());
    } else if (auto func = std::dynamic_pointer_cast<FunctionExpr>(expr)) {
        generateFunction(func.get());
    } else if (auto arrow = std::dynamic_pointer_cast<ArrowFunctionExpr>(expr)) {
        generateArrowFunction(arrow.get());
    } else if (auto ternary = std::dynamic_pointer_cast<TernaryExpr>(expr)) {
        generateTernary(ternary.get());
    }
}

void CodeGenerator::generateVariableDecl(VariableDecl* decl) {
    // 声明变量
    declareVariable(decl->name);
    
    // 生成初始化表达式
    if (decl->initializer) {
        generateExpression(decl->initializer);
    } else {
        // 无初始化，推入 undefined
        emitOp(OpCode::LOAD_UNDEFINED);
    }
    
    // 存储到局部变量
    int slot = resolveLocal(decl->name);
    if (slot >= 0) {
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), static_cast<uint8_t>(slot));
    } else {
        // 全局变量
        int constIndex = addConstant(Constant(decl->name));
        emitBytes(static_cast<uint8_t>(OpCode::STORE_GLOBAL), static_cast<uint8_t>(constIndex));
    }
}

void CodeGenerator::generateFunctionDecl(FunctionDecl* decl) {
    // 创建函数信息
    FunctionInfo func;
    func.name = decl->name;
    func.arity = static_cast<uint16_t>(decl->parameters.size());
    func.isAsync = decl->isAsync;
    
    // 检查剩余参数
    for (const auto& param : decl->parameters) {
        if (param.find("...") == 0) {
            func.isVariadic = true;
            break;
        }
    }
    
    // 保存当前状态
    FunctionInfo* savedFunction = currentFunction_;
    int savedSlotCount = localSlotCount_;
    auto savedScopes = scopes_;

    // 切换到新函数
    currentFunction_ = &func;
    localSlotCount_ = 0;
    scopes_.clear();

    beginScope();
    
    // 声明参数
    for (const auto& param : decl->parameters) {
        std::string paramName = param;
        if (paramName.find("...") == 0) {
            paramName = paramName.substr(3);
        }
        declareVariable(paramName);
    }
    
    // 生成函数体
    if (decl->body) {
        for (const auto& stmt : decl->body->statements) {
            generateStatement(stmt);
        }
    }
    
    // 如果没有显式返回，添加隐式返回
    if (decl->body && decl->body->statements.empty()) {
        emitOp(OpCode::RETURN);
    }
    
    endScope();
    
    // 恢复状态
    func.localCount = static_cast<uint16_t>(localSlotCount_);
    func.maxStackSlots = 256; // TODO: 计算实际最大栈深度
    currentFunction_ = savedFunction;
    localSlotCount_ = savedSlotCount;
    scopes_ = savedScopes;
    
    // 添加函数到模块
    module_.functions.push_back(func);
    
    // 创建闭包或加载函数
    int funcIndex = static_cast<int>(module_.functions.size() - 1);
    emitBytes(static_cast<uint8_t>(OpCode::CLOSURE), static_cast<uint8_t>(funcIndex));
    
    // 存储到变量
    int slot = resolveLocal(decl->name);
    if (slot >= 0) {
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), static_cast<uint8_t>(slot));
    } else {
        int constIndex = addConstant(Constant(decl->name));
        emitBytes(static_cast<uint8_t>(OpCode::STORE_GLOBAL), static_cast<uint8_t>(constIndex));
    }
}

void CodeGenerator::generateClassDecl(ClassDecl* decl) {
    // 加载父类（如果有）
    if (!decl->superClass.empty()) {
        int constIndex = addConstant(Constant(decl->superClass));
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_GLOBAL), static_cast<uint8_t>(constIndex));
    }
    
    // 创建类
    emitOp(OpCode::NEW_INSTANCE);
    
    // 存储类到变量
    int constIndex = addConstant(Constant(decl->name));
    emitBytes(static_cast<uint8_t>(OpCode::STORE_GLOBAL), static_cast<uint8_t>(constIndex));
    
    // TODO: 生成方法
}

void CodeGenerator::generateModuleDecl(ModuleDecl* decl) {
    // 模块声明 - 创建新的作用域
    beginScope();
    
    if (decl->body) {
        for (const auto& stmt : decl->body->statements) {
            generateStatement(stmt);
        }
    }
    
    endScope();
}

void CodeGenerator::generateImportDecl(ImportDecl* decl) {
    ImportInfo info;
    info.modulePath = decl->modulePath;
    info.namespaceAlias = decl->namespaceAlias;
    info.defaultImport = decl->defaultImport;
    
    for (const auto& spec : decl->specifiers) {
        info.specifiers.push_back({spec.importedName, spec.localName});
    }
    
    module_.imports.push_back(info);
}

void CodeGenerator::generateExportDecl(ExportDecl* decl) {
    ExportInfo info;
    info.name = decl->name;
    info.isDefault = decl->isDefault;
    
    module_.exports.push_back(info);
}

void CodeGenerator::generateBlock(BlockStmt* block) {
    beginScope();
    
    for (const auto& stmt : block->statements) {
        generateStatement(stmt);
    }
    
    endScope();
}

void CodeGenerator::generateExpressionStmt(ExpressionStmt* stmt) {
    generateExpression(stmt->expression);
    emitOp(OpCode::POP);
}

void CodeGenerator::generateIfStmt(IfStmt* stmt) {
    // 生成条件
    generateExpression(stmt->condition);
    
    // 如果为假，跳转到 else 分支
    emitOp(OpCode::JUMP_IF_FALSE);
    size_t elseJumpOffset = currentOffset();
    emitWord(0); // 占位符
    
    // 生成 then 分支
    generateStatement(stmt->thenBranch);
    
    if (stmt->elseBranch) {
        // 跳转到 if 结束
        emitOp(OpCode::JUMP);
        size_t endJumpOffset = currentOffset();
        emitWord(0); // 占位符
        
        // 修复 else 跳转
        patchJump(elseJumpOffset);
        
        // 生成 else 分支
        generateStatement(stmt->elseBranch);
        
        // 修复结束跳转
        patchJump(endJumpOffset);
    } else {
        // 没有 else 分支
        patchJump(elseJumpOffset);
    }
}

void CodeGenerator::generateForStmt(ForStmt* stmt) {
    // 循环开始标记
    size_t loopStart = currentOffset();
    loopStarts_.push_back(loopStart);

    beginScope();

    // 初始化
    if (stmt->initializer) {
        generateStatement(stmt->initializer);
    }

    // 条件检查
    size_t conditionOffset = currentOffset();
    size_t exitJumpOffset = 0;  // 初始化
    bool hasCondition = stmt->condition != nullptr;
    
    if (hasCondition) {
        generateExpression(stmt->condition);
        emitOp(OpCode::JUMP_IF_FALSE);
        exitJumpOffset = currentOffset();
        emitWord(0);
    }

    // 循环体
    generateStatement(stmt->body);

    // 增量
    size_t incrementOffset = currentOffset();
    if (stmt->increment) {
        generateExpression(stmt->increment);
        emitOp(OpCode::POP);
    }

    // 跳回条件检查
    emitOp(OpCode::JUMP);
    size_t loopJumpOffset = currentOffset();
    int16_t jumpOffset = static_cast<int16_t>(conditionOffset - currentOffset() - 2);
    emitWord(jumpOffset);

    // 循环结束
    size_t loopEnd = currentOffset();
    loopEnds_.push_back(loopEnd);

    // 修复跳转
    if (hasCondition) {
        patchJump(exitJumpOffset);
    }

    // 弹出循环变量
    endScope();

    loopStarts_.pop_back();
    loopEnds_.pop_back();
}

void CodeGenerator::generateForEachStmt(ForEachStmt* stmt) {
    // 简化实现：遍历数组
    beginScope();
    
    // 加载可迭代对象
    generateExpression(stmt->iterable);
    
    // 获取长度
    emitOp(OpCode::DUP);
    emitOp(OpCode::LOAD_PROPERTY); // 假设有一个 length 属性
    int lengthConst = addConstant(Constant("length"));
    emitByte(static_cast<uint8_t>(lengthConst));
    
    // 索引变量
    declareVariable("__index");
    emitOp(OpCode::LOAD_CONST);
    emitByte(0);
    emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), 0);
    
    size_t loopStart = currentOffset();
    loopStarts_.push_back(loopStart);
    
    // 检查索引 < length
    emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), 0);
    emitOp(OpCode::LOAD_PROPERTY);
    emitOp(OpCode::LESS);
    
    emitOp(OpCode::JUMP_IF_FALSE);
    size_t exitJumpOffset = currentOffset();
    emitWord(0);
    
    // 获取当前元素
    emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), 0);
    emitOp(OpCode::LOAD_SUBSCRIPT);
    
    // 存储到循环变量
    declareVariable(stmt->variable);
    int slot = resolveLocal(stmt->variable);
    emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), static_cast<uint8_t>(slot));

    // 循环体
    generateBlock(stmt->body);
    
    // 索引++
    emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), 0);
    emitOp(OpCode::LOAD_CONST);
    emitByte(1);
    emitOp(OpCode::ADD);
    emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), 0);
    
    // 跳回循环开始
    emitOp(OpCode::JUMP);
    size_t loopJumpOffset = currentOffset();
    int16_t jumpOffset = static_cast<int16_t>(loopStart - currentOffset() - 2);
    emitWord(jumpOffset);
    
    // 循环结束
    size_t loopEnd = currentOffset();
    loopEnds_.push_back(loopEnd);
    
    patchJump(exitJumpOffset);
    
    endScope();
    loopStarts_.pop_back();
    loopEnds_.pop_back();
}

void CodeGenerator::generateWhileStmt(WhileStmt* stmt) {
    size_t loopStart = currentOffset();
    loopStarts_.push_back(loopStart);
    
    // 条件
    generateExpression(stmt->condition);
    emitOp(OpCode::JUMP_IF_FALSE);
    size_t exitJumpOffset = currentOffset();
    emitWord(0);
    
    // 循环体
    generateStatement(stmt->body);
    
    // 跳回循环开始
    emitOp(OpCode::JUMP);
    size_t loopJumpOffset = currentOffset();
    int16_t jumpOffset = static_cast<int16_t>(loopStart - currentOffset() - 2);
    emitWord(jumpOffset);
    
    // 循环结束
    size_t loopEnd = currentOffset();
    loopEnds_.push_back(loopEnd);
    
    patchJump(exitJumpOffset);
    
    loopStarts_.pop_back();
    loopEnds_.pop_back();
}

void CodeGenerator::generateReturnStmt(ReturnStmt* stmt) {
    if (stmt->value) {
        generateExpression(stmt->value);
        emitOp(OpCode::RETURN_VALUE);
    } else {
        emitOp(OpCode::RETURN);
    }
}

void CodeGenerator::generateBreakStmt(BreakStmt* stmt) {
    if (!loopEnds_.empty()) {
        emitOp(OpCode::JUMP);
        size_t jumpOffset = currentOffset();
        emitWord(0);
        // 这里需要记录跳转位置以便后续修复
    } else {
        error(stmt->location, "break statement outside of loop");
    }
}

void CodeGenerator::generateContinueStmt(ContinueStmt* stmt) {
    if (!loopStarts_.empty()) {
        emitOp(OpCode::JUMP);
        size_t jumpOffset = currentOffset();
        emitWord(0);
        // 这里需要记录跳转位置以便后续修复
    } else {
        error(stmt->location, "continue statement outside of loop");
    }
}

void CodeGenerator::generateThrowStmt(ThrowStmt* stmt) {
    generateExpression(stmt->exception);
    emitOp(OpCode::THROW);
}

void CodeGenerator::generateTryStmt(TryStmt* stmt) {
    // try 块开始
    emitOp(OpCode::TRY_BEGIN);
    size_t tryStart = currentOffset();
    
    if (stmt->tryBlock) {
        generateBlock(stmt->tryBlock);
    }
    
    // try 块结束
    emitOp(OpCode::TRY_END);
    size_t tryEnd = currentOffset();
    
    // catch 块
    if (stmt->catchBlock) {
        emitOp(OpCode::CATCH);
        
        // 声明 catch 变量
        beginScope();
        if (!stmt->catchVariable.empty()) {
            declareVariable(stmt->catchVariable);
        }
        
        generateBlock(stmt->catchBlock);
        
        endScope();
    }
    
    // finally 块
    if (stmt->finallyBlock) {
        generateBlock(stmt->finallyBlock);
    }
}

void CodeGenerator::generateSwitchStmt(SwitchStmt* stmt) {
    // 生成判别表达式
    generateExpression(stmt->discriminant);
    
    std::vector<size_t> caseJumps;
    
    // 生成所有 case
    for (const auto& case_ : stmt->cases) {
        if (!case_.isDefault) {
            // 比较值
            emitOp(OpCode::DUP);
            generateExpression(case_.value);
            emitOp(OpCode::EQUAL);
            
            // 如果不匹配，跳过
            emitOp(OpCode::JUMP_IF_FALSE);
            size_t jumpOffset = currentOffset();
            emitWord(0);
            caseJumps.push_back(jumpOffset);
        }
        
        // 生成 case 体
        for (const auto& s : case_.statements) {
            generateStatement(s);
        }
    }
    
    // 修复所有跳转
    for (size_t offset : caseJumps) {
        patchJump(offset);
    }
    
    // 弹出判别值
    emitOp(OpCode::POP);
}

void CodeGenerator::generateLiteral(LiteralExpr* expr) {
    if (expr->literalType == TokenType::NULL_) {
        emitOp(OpCode::LOAD_NULL);
    } else if (expr->literalType == TokenType::UNDEFINED) {
        emitOp(OpCode::LOAD_UNDEFINED);
    } else if (expr->literalType == TokenType::TRUE) {
        emitOp(OpCode::LOAD_TRUE);
    } else if (expr->literalType == TokenType::FALSE) {
        emitOp(OpCode::LOAD_FALSE);
    } else if (expr->literalType == TokenType::NUMBER) {
        if (auto num = std::get_if<double>(&expr->value)) {
            int constIndex = addConstant(Constant(*num));
            emitConstant(constIndex);
        }
    } else if (expr->literalType == TokenType::STRING) {
        if (auto str = std::get_if<std::string>(&expr->value)) {
            int constIndex = addConstant(Constant(*str));
            emitConstant(constIndex);
        }
    }
}

void CodeGenerator::generateIdentifier(IdentifierExpr* expr) {
    int slot = resolveLocal(expr->name);
    if (slot >= 0) {
        // 局部变量
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), static_cast<uint8_t>(slot));
    } else {
        // 全局变量
        int constIndex = addConstant(Constant(expr->name));
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_GLOBAL), static_cast<uint8_t>(constIndex));
    }
}

void CodeGenerator::generateBinary(BinaryExpr* expr) {
    // 生成左操作数
    generateExpression(expr->left);
    
    // 生成右操作数
    generateExpression(expr->right);
    
    // 生成操作码
    switch (expr->op) {
        case TokenType::PLUS:
            emitOp(OpCode::ADD);
            break;
        case TokenType::MINUS:
            emitOp(OpCode::SUBTRACT);
            break;
        case TokenType::STAR:
            emitOp(OpCode::MULTIPLY);
            break;
        case TokenType::SLASH:
            emitOp(OpCode::DIVIDE);
            break;
        case TokenType::PERCENT:
            emitOp(OpCode::MODULO);
            break;
        case TokenType::STAR_STAR:
            emitOp(OpCode::POWER);
            break;
        case TokenType::EQUAL_EQUAL:
        case TokenType::EQUAL_EQUAL_EQUAL:
            emitOp(OpCode::EQUAL);
            break;
        case TokenType::BANG_EQUAL:
        case TokenType::BANG_EQUAL_EQUAL:
            emitOp(OpCode::NOT_EQUAL);
            break;
        case TokenType::LESS:
            emitOp(OpCode::LESS);
            break;
        case TokenType::GREATER:
            emitOp(OpCode::GREATER);
            break;
        case TokenType::LESS_EQUAL:
            emitOp(OpCode::LESS_EQUAL);
            break;
        case TokenType::GREATER_EQUAL:
            emitOp(OpCode::GREATER_EQUAL);
            break;
        case TokenType::AMPERSAND_AMPERSAND:
            emitOp(OpCode::AND);
            break;
        case TokenType::PIPE_PIPE:
            emitOp(OpCode::OR);
            break;
        case TokenType::AMPERSAND:
            emitOp(OpCode::BIT_AND);
            break;
        case TokenType::PIPE:
            emitOp(OpCode::BIT_OR);
            break;
        case TokenType::CARET:
            emitOp(OpCode::BIT_XOR);
            break;
        case TokenType::LESS_LESS:
            emitOp(OpCode::SHIFT_LEFT);
            break;
        case TokenType::GREATER_GREATER:
            emitOp(OpCode::SHIFT_RIGHT);
            break;
        default:
            error(expr->location, "Unknown binary operator");
            break;
    }
}

void CodeGenerator::generateUnary(UnaryExpr* expr) {
    generateExpression(expr->operand);
    
    switch (expr->op) {
        case TokenType::MINUS:
            emitOp(OpCode::NEGATE);
            break;
        case TokenType::BANG:
            emitOp(OpCode::NOT);
            break;
        case TokenType::TILDE:
            emitOp(OpCode::BIT_NOT);
            break;
        case TokenType::PLUS_PLUS:
            // ++x: x = x + 1
            emitOp(OpCode::LOAD_CONST);
            emitByte(1);
            emitOp(OpCode::ADD);
            break;
        case TokenType::MINUS_MINUS:
            // --x: x = x - 1
            emitOp(OpCode::LOAD_CONST);
            emitByte(1);
            emitOp(OpCode::SUBTRACT);
            break;
        default:
            error(expr->location, "Unknown unary operator");
            break;
    }
}

void CodeGenerator::generateAssign(AssignExpr* expr) {
    // 生成值
    generateExpression(expr->value);
    
    // 复制一份用于存储
    emitOp(OpCode::DUP);
    
    // 存储到变量
    int slot = resolveLocal(expr->target);
    if (slot >= 0) {
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), static_cast<uint8_t>(slot));
    } else {
        int constIndex = addConstant(Constant(expr->target));
        emitBytes(static_cast<uint8_t>(OpCode::STORE_GLOBAL), static_cast<uint8_t>(constIndex));
    }
}

void CodeGenerator::generateCall(CallExpr* expr) {
    // 生成参数
    for (const auto& arg : expr->arguments) {
        generateExpression(arg);
    }
    
    // 生成被调用对象
    generateExpression(expr->callee);
    
    // 调用
    emitBytes(static_cast<uint8_t>(OpCode::CALL), static_cast<uint8_t>(expr->arguments.size()));
}

void CodeGenerator::generateMember(MemberExpr* expr) {
    // 生成对象
    generateExpression(expr->object);
    
    // 加载属性
    int constIndex = addConstant(Constant(expr->property));
    emitBytes(static_cast<uint8_t>(OpCode::LOAD_PROPERTY), static_cast<uint8_t>(constIndex));
}

void CodeGenerator::generateIndex(IndexExpr* expr) {
    // 生成对象
    generateExpression(expr->object);
    
    // 生成索引
    generateExpression(expr->index);
    
    // 加载元素
    emitOp(OpCode::LOAD_SUBSCRIPT);
}

void CodeGenerator::generateArray(ArrayExpr* expr) {
    // 生成所有元素
    for (const auto& elem : expr->elements) {
        generateExpression(elem);
    }
    
    // 创建数组
    emitBytes(static_cast<uint8_t>(OpCode::NEW_ARRAY), static_cast<uint8_t>(expr->elements.size()));
}

void CodeGenerator::generateObject(ObjectExpr* expr) {
    // 创建空对象
    emitOp(OpCode::NEW_OBJECT);
    
    // 设置所有属性
    for (const auto& prop : expr->properties) {
        emitOp(OpCode::DUP);
        
        // 属性名
        int nameConst = addConstant(Constant(prop.key));
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_CONST), static_cast<uint8_t>(nameConst));
        
        // 属性值
        generateExpression(prop.value);
        
        // 存储属性
        emitOp(OpCode::STORE_PROPERTY);
    }
}

void CodeGenerator::generateFunction(FunctionExpr* expr) {
    // 创建函数信息
    FunctionInfo func;
    func.name = expr->name;
    func.arity = static_cast<uint16_t>(expr->parameters.size());
    func.isAsync = expr->isAsync;
    
    // 保存当前状态
    FunctionInfo* savedFunction = currentFunction_;
    int savedSlotCount = localSlotCount_;
    auto savedScopes = scopes_;

    // 切换到新函数
    currentFunction_ = &func;
    localSlotCount_ = 0;
    scopes_.clear();

    beginScope();
    
    // 声明参数
    for (const auto& param : expr->parameters) {
        declareVariable(param);
    }
    
    // 生成函数体
    for (const auto& stmt : expr->body) {
        generateStatement(stmt);
    }
    
    // 如果没有显式返回，添加隐式返回
    if (expr->body.empty() || 
        !std::dynamic_pointer_cast<ReturnStmt>(expr->body.back())) {
        emitOp(OpCode::RETURN);
    }
    
    endScope();
    
    // 恢复状态
    func.localCount = static_cast<uint16_t>(localSlotCount_);
    func.maxStackSlots = 256;
    currentFunction_ = savedFunction;
    localSlotCount_ = savedSlotCount;
    scopes_ = savedScopes;
    
    // 添加函数到模块
    module_.functions.push_back(func);
    
    // 创建闭包
    int funcIndex = static_cast<int>(module_.functions.size() - 1);
    emitBytes(static_cast<uint8_t>(OpCode::CLOSURE), static_cast<uint8_t>(funcIndex));
}

void CodeGenerator::generateArrowFunction(ArrowFunctionExpr* expr) {
    // 类似普通函数，但更简化
    FunctionInfo func;
    func.name = "<arrow>";
    func.arity = static_cast<uint16_t>(expr->parameters.size());
    func.isAsync = expr->isAsync;

    FunctionInfo* savedFunction = currentFunction_;
    int savedSlotCount = localSlotCount_;
    auto savedScopes = scopes_;

    currentFunction_ = &func;
    localSlotCount_ = 0;
    scopes_.clear();

    beginScope();
    
    for (const auto& param : expr->parameters) {
        declareVariable(param);
    }
    
    if (expr->expressionBody) {
        // 表达式体箭头函数
        generateExpression(expr->expressionBody);
        emitOp(OpCode::RETURN_VALUE);
    } else {
        // 块体箭头函数
        for (const auto& stmt : expr->blockBody) {
            generateStatement(stmt);
        }
    }
    
    endScope();
    
    func.localCount = static_cast<uint16_t>(localSlotCount_);
    func.maxStackSlots = 256;
    currentFunction_ = savedFunction;
    localSlotCount_ = savedSlotCount;
    scopes_ = savedScopes;
    
    module_.functions.push_back(func);
    
    int funcIndex = static_cast<int>(module_.functions.size() - 1);
    emitBytes(static_cast<uint8_t>(OpCode::CLOSURE), static_cast<uint8_t>(funcIndex));
}

void CodeGenerator::generateTernary(TernaryExpr* expr) {
    // 生成条件
    generateExpression(expr->condition);
    
    // 如果为假，跳转到 else
    emitOp(OpCode::JUMP_IF_FALSE);
    size_t elseJumpOffset = currentOffset();
    emitWord(0);
    
    // 生成 then 分支
    generateExpression(expr->thenBranch);
    
    emitOp(OpCode::JUMP);
    size_t endJumpOffset = currentOffset();
    emitWord(0);
    
    // 修复 else 跳转
    patchJump(elseJumpOffset);
    
    // 生成 else 分支
    generateExpression(expr->elseBranch);
    
    // 修复结束跳转
    patchJump(endJumpOffset);
}

void CodeGenerator::error(const SourceLocation& loc, const std::string& message) {
    errors_.push_back(Diagnostic::error(message, loc));
}

void CodeGenerator::recordDebugLine(size_t line) {
    if (currentFunction_) {
        currentFunction_->debugLines.push_back({currentOffset(), line});
    }
}

} // namespace zcc

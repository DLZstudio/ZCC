#ifndef ZCC_CODEGENERATOR_HPP
#define ZCC_CODEGENERATOR_HPP

#include "AST.hpp"
#include "Bytecode.hpp"
#include "Diagnostics.hpp"
#include <unordered_map>
#include <stack>

namespace zcc {

// 代码生成器类
class CodeGenerator {
public:
    CodeGenerator();
    
    // 生成字节码
    BytecodeModule generate(ProgramPtr program);
    
    // 获取错误列表
    const std::vector<Diagnostic>& getErrors() const { return errors_; }
    
    // 是否有错误
    bool hasErrors() const { return !errors_.empty(); }

private:
    BytecodeModule module_;
    std::vector<Diagnostic> errors_;
    
    // 当前函数信息
    FunctionInfo* currentFunction_;
    
    // 作用域栈 (使用 vector 代替 stack 以支持迭代)
    std::vector<std::unordered_map<std::string, int>> scopes_;
    int scopeDepth_;
    
    // 局部变量槽位
    int localSlotCount_;
    
    // 闭包变量
    std::unordered_map<std::string, int> upvalues_;
    
    // 循环控制
    std::vector<size_t> loopStarts_;
    std::vector<size_t> loopEnds_;
    
    // 常量池索引
    int addConstant(Constant constant);
    
    // 字节码发射
    void emitByte(uint8_t byte);
    void emitBytes(uint8_t byte1, uint8_t byte2);
    void emitWord(uint16_t word);
    void emitConstant(int index);
    void emitOp(OpCode op);
    
    // 修复跳转
    void patchJump(size_t offset);
    size_t currentOffset() const;
    
    // 作用域管理
    void beginScope();
    void endScope();
    void declareVariable(const std::string& name);
    int resolveLocal(const std::string& name);
    
    // AST 节点访问
    void generateDeclaration(DeclPtr decl);
    void generateStatement(StmtPtr stmt);
    void generateExpression(ExprPtr expr);
    
    // 具体节点生成
    void generateVariableDecl(VariableDecl* decl);
    void generateFunctionDecl(FunctionDecl* decl);
    void generateClassDecl(ClassDecl* decl);
    void generateModuleDecl(ModuleDecl* decl);
    void generateImportDecl(ImportDecl* decl);
    void generateExportDecl(ExportDecl* decl);
    
    void generateBlock(BlockStmt* block);
    void generateExpressionStmt(ExpressionStmt* stmt);
    void generateIfStmt(IfStmt* stmt);
    void generateForStmt(ForStmt* stmt);
    void generateForEachStmt(ForEachStmt* stmt);
    void generateWhileStmt(WhileStmt* stmt);
    void generateReturnStmt(ReturnStmt* stmt);
    void generateBreakStmt(BreakStmt* stmt);
    void generateContinueStmt(ContinueStmt* stmt);
    void generateThrowStmt(ThrowStmt* stmt);
    void generateTryStmt(TryStmt* stmt);
    void generateSwitchStmt(SwitchStmt* stmt);
    
    void generateLiteral(LiteralExpr* expr);
    void generateIdentifier(IdentifierExpr* expr);
    void generateBinary(BinaryExpr* expr);
    void generateUnary(UnaryExpr* expr);
    void generateAssign(AssignExpr* expr);
    void generateCall(CallExpr* expr);
    void generateMember(MemberExpr* expr);
    void generateIndex(IndexExpr* expr);
    void generateArray(ArrayExpr* expr);
    void generateObject(ObjectExpr* expr);
    void generateFunction(FunctionExpr* expr);
    void generateArrowFunction(ArrowFunctionExpr* expr);
    void generateTernary(TernaryExpr* expr);
    
    // 辅助函数
    void error(const SourceLocation& loc, const std::string& message);
    void recordDebugLine(size_t line);
};

} // namespace zcc

#endif // ZCC_CODEGENERATOR_HPP

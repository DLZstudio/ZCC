#ifndef ZCC_AST_HPP
#define ZCC_AST_HPP

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include "Token.hpp"
#include "Diagnostics.hpp"

namespace zcc {

// 前向声明
struct Expression;
struct Statement;
struct Declaration;
struct Program;

// 使用智能指针
using ExprPtr = std::shared_ptr<struct Expression>;
using StmtPtr = std::shared_ptr<struct Statement>;
using DeclPtr = std::shared_ptr<struct Declaration>;
using ProgramPtr = std::shared_ptr<struct Program>;

// AST 节点基类
struct ASTNode {
    virtual ~ASTNode() = default;
    virtual std::string toString() const = 0;
    SourceLocation location;
    
protected:
    ASTNode() : location("<unknown>", 0, 0, 0) {}
    ASTNode(const SourceLocation& loc) : location(loc) {}
};

// ==================== 表达式 ====================

// 表达式基类
struct Expression : ASTNode {
    virtual ExprPtr clone() const = 0;
};

// 字面量表达式
struct LiteralExpr : Expression {
    TokenValue value;
    TokenType literalType;
    
    LiteralExpr(TokenValue v, TokenType t, const SourceLocation& loc)
        : value(std::move(v)), literalType(t) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<LiteralExpr>(*this);
    }
    
    std::string toString() const override {
        if (auto str = std::get_if<std::string>(&value)) {
            return "\"" + *str + "\"";
        } else if (auto num = std::get_if<double>(&value)) {
            return std::to_string(*num);
        } else if (auto b = std::get_if<bool>(&value)) {
            return *b ? "true" : "false";
        } else if (literalType == TokenType::NULL_) {
            return "null";
        } else if (literalType == TokenType::UNDEFINED) {
            return "undefined";
        }
        return "literal";
    }
};

// 标识符表达式
struct IdentifierExpr : Expression {
    std::string name;
    
    IdentifierExpr(std::string n, const SourceLocation& loc)
        : name(std::move(n)) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<IdentifierExpr>(*this);
    }
    
    std::string toString() const override {
        return name;
    }
};

// 二元运算表达式
struct BinaryExpr : Expression {
    ExprPtr left;
    TokenType op;
    ExprPtr right;
    
    BinaryExpr(ExprPtr l, TokenType o, ExprPtr r, const SourceLocation& loc)
        : left(std::move(l)), op(o), right(std::move(r)) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<BinaryExpr>(*this);
    }
    
    std::string toString() const override {
        return "(" + left->toString() + " " + 
               Token(TokenType::EOF_, std::monostate{}, "", 0, 0).typeName() + " " +
               right->toString() + ")";
    }
};

// 一元运算表达式
struct UnaryExpr : Expression {
    TokenType op;
    ExprPtr operand;
    
    UnaryExpr(TokenType o, ExprPtr operand, const SourceLocation& loc)
        : op(o), operand(std::move(operand)) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<UnaryExpr>(*this);
    }
    
    std::string toString() const override {
        return "(" + std::to_string(static_cast<int>(op)) + " " + operand->toString() + ")";
    }
};

// 赋值表达式
struct AssignExpr : Expression {
    std::string target;
    ExprPtr value;
    
    AssignExpr(std::string t, ExprPtr v, const SourceLocation& loc)
        : target(std::move(t)), value(std::move(v)) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<AssignExpr>(*this);
    }
    
    std::string toString() const override {
        return target + " = " + value->toString();
    }
};

// 调用表达式
struct CallExpr : Expression {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;
    
    CallExpr(ExprPtr c, std::vector<ExprPtr> args, const SourceLocation& loc)
        : callee(std::move(c)), arguments(std::move(args)) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<CallExpr>(*this);
    }
    
    std::string toString() const override {
        std::string args;
        for (size_t i = 0; i < arguments.size(); ++i) {
            if (i > 0) args += ", ";
            args += arguments[i]->toString();
        }
        return callee->toString() + "(" + args + ")";
    }
};

// 成员访问表达式
struct MemberExpr : Expression {
    ExprPtr object;
    std::string property;
    
    MemberExpr(ExprPtr obj, std::string prop, const SourceLocation& loc)
        : object(std::move(obj)), property(std::move(prop)) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<MemberExpr>(*this);
    }
    
    std::string toString() const override {
        return object->toString() + "." + property;
    }
};

// 索引表达式
struct IndexExpr : Expression {
    ExprPtr object;
    ExprPtr index;
    
    IndexExpr(ExprPtr obj, ExprPtr idx, const SourceLocation& loc)
        : object(std::move(obj)), index(std::move(idx)) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<IndexExpr>(*this);
    }
    
    std::string toString() const override {
        return object->toString() + "[" + index->toString() + "]";
    }
};

// 数组字面量
struct ArrayExpr : Expression {
    std::vector<ExprPtr> elements;
    
    ArrayExpr(std::vector<ExprPtr> elems, const SourceLocation& loc)
        : elements(std::move(elems)) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<ArrayExpr>(*this);
    }
    
    std::string toString() const override {
        std::string result = "[";
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0) result += ", ";
            result += elements[i]->toString();
        }
        result += "]";
        return result;
    }
};

// 对象字面量
struct ObjectExpr : Expression {
    struct Property {
        std::string key;
        ExprPtr value;
        bool isMethod;
        bool isShorthand;
    };
    
    std::vector<Property> properties;
    
    ObjectExpr(std::vector<Property> props, const SourceLocation& loc)
        : properties(std::move(props)) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<ObjectExpr>(*this);
    }
    
    std::string toString() const override {
        std::string result = "{";
        for (size_t i = 0; i < properties.size(); ++i) {
            if (i > 0) result += ", ";
            result += properties[i].key + ": " + properties[i].value->toString();
        }
        result += "}";
        return result;
    }
};

// 函数表达式
struct FunctionExpr : Expression {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<StmtPtr> body;
    bool isAsync;
    
    FunctionExpr(std::string n, std::vector<std::string> params, 
                 std::vector<StmtPtr> b, bool async, const SourceLocation& loc)
        : name(std::move(n)), parameters(std::move(params)), 
          body(std::move(b)), isAsync(async) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<FunctionExpr>(*this);
    }
    
    std::string toString() const override {
        return "function " + name + "() {...}";
    }
};

// 箭头函数表达式
struct ArrowFunctionExpr : Expression {
    std::vector<std::string> parameters;
    ExprPtr expressionBody;
    std::vector<StmtPtr> blockBody;
    bool isAsync;
    
    ArrowFunctionExpr(std::vector<std::string> params, ExprPtr exprBody,
                      std::vector<StmtPtr> blk, bool async, const SourceLocation& loc)
        : parameters(std::move(params)), expressionBody(std::move(exprBody)),
          blockBody(std::move(blk)), isAsync(async) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<ArrowFunctionExpr>(*this);
    }
    
    std::string toString() const override {
        return "() => {...}";
    }
};

// 三元表达式
struct TernaryExpr : Expression {
    ExprPtr condition;
    ExprPtr thenBranch;
    ExprPtr elseBranch;
    
    TernaryExpr(ExprPtr cond, ExprPtr thenB, ExprPtr elseB, const SourceLocation& loc)
        : condition(std::move(cond)), thenBranch(std::move(thenB)), 
          elseBranch(std::move(elseB)) {
        location = loc;
    }
    
    ExprPtr clone() const override {
        return std::make_shared<TernaryExpr>(*this);
    }
    
    std::string toString() const override {
        return condition->toString() + " ? " + thenBranch->toString() + 
               " : " + elseBranch->toString();
    }
};

// ==================== 语句 ====================

// 语句基类
struct Statement : ASTNode {
    virtual StmtPtr clone() const = 0;
};

// 表达式语句
struct ExpressionStmt : Statement {
    ExprPtr expression;
    
    ExpressionStmt(ExprPtr expr, const SourceLocation& loc)
        : expression(std::move(expr)) {
        location = loc;
    }
    
    StmtPtr clone() const override {
        return std::make_shared<ExpressionStmt>(*this);
    }
    
    std::string toString() const override {
        return expression->toString() + ";";
    }
};

// 块语句
struct BlockStmt : Statement {
    std::vector<StmtPtr> statements;
    
    BlockStmt(std::vector<StmtPtr> stmts, const SourceLocation& loc)
        : statements(std::move(stmts)) {
        location = loc;
    }
    
    StmtPtr clone() const override {
        return std::make_shared<BlockStmt>(*this);
    }
    
    std::string toString() const override {
        std::string result = "{\n";
        for (const auto& stmt : statements) {
            result += "  " + stmt->toString() + "\n";
        }
        result += "}";
        return result;
    }
};

// if 语句
struct IfStmt : Statement {
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;
    
    IfStmt(ExprPtr cond, StmtPtr thenB, StmtPtr elseB, const SourceLocation& loc)
        : condition(std::move(cond)), thenBranch(std::move(thenB)), 
          elseBranch(std::move(elseB)) {
        location = loc;
    }
    
    StmtPtr clone() const override {
        return std::make_shared<IfStmt>(*this);
    }
    
    std::string toString() const override {
        std::string result = "if (" + condition->toString() + ") " + thenBranch->toString();
        if (elseBranch) {
            result += " else " + elseBranch->toString();
        }
        return result;
    }
};

// for 循环语句
struct ForStmt : Statement {
    StmtPtr initializer;
    ExprPtr condition;
    ExprPtr increment;
    StmtPtr body;
    
    ForStmt(StmtPtr init, ExprPtr cond, ExprPtr inc, StmtPtr b, const SourceLocation& loc)
        : initializer(std::move(init)), condition(std::move(cond)), 
          increment(std::move(inc)), body(std::move(b)) {
        location = loc;
    }
    
    StmtPtr clone() const override {
        return std::make_shared<ForStmt>(*this);
    }
    
    std::string toString() const override {
        return "for (...) {...}";
    }
};

// while 循环语句
struct WhileStmt : Statement {
    ExprPtr condition;
    StmtPtr body;
    
    WhileStmt(ExprPtr cond, StmtPtr b, const SourceLocation& loc)
        : condition(std::move(cond)), body(std::move(b)) {
        location = loc;
    }
    
    StmtPtr clone() const override {
        return std::make_shared<WhileStmt>(*this);
    }
    
    std::string toString() const override {
        return "while (" + condition->toString() + ") {...}";
    }
};

// for-each 循环语句
struct ForEachStmt : Statement {
    std::string variable;
    ExprPtr iterable;
    StmtPtr body;
    
    ForEachStmt(std::string var, ExprPtr iter, StmtPtr b, const SourceLocation& loc)
        : variable(std::move(var)), iterable(std::move(iter)), body(std::move(b)) {
        location = loc;
    }
    
    StmtPtr clone() const override {
        return std::make_shared<ForEachStmt>(*this);
    }
    
    std::string toString() const override {
        return "for each in (...) {...}";
    }
};

// return 语句
struct ReturnStmt : Statement {
    ExprPtr value;
    
    ReturnStmt(ExprPtr v, const SourceLocation& loc)
        : value(std::move(v)) {
        location = loc;
    }
    
    StmtPtr clone() const override {
        return std::make_shared<ReturnStmt>(*this);
    }
    
    std::string toString() const override {
        return value ? "return " + value->toString() : "return";
    }
};

// break 语句
struct BreakStmt : Statement {
    BreakStmt(const SourceLocation& loc) {
        location = loc;
    }
    
    StmtPtr clone() const override {
        return std::make_shared<BreakStmt>(*this);
    }
    
    std::string toString() const override {
        return "break";
    }
};

// continue 语句
struct ContinueStmt : Statement {
    ContinueStmt(const SourceLocation& loc) {
        location = loc;
    }
    
    StmtPtr clone() const override {
        return std::make_shared<ContinueStmt>(*this);
    }
    
    std::string toString() const override {
        return "continue";
    }
};

// throw 语句
struct ThrowStmt : Statement {
    ExprPtr exception;
    
    ThrowStmt(ExprPtr exc, const SourceLocation& loc)
        : exception(std::move(exc)) {
        location = loc;
    }
    
    StmtPtr clone() const override {
        return std::make_shared<ThrowStmt>(*this);
    }
    
    std::string toString() const override {
        return "throw " + exception->toString();
    }
};

// try-catch-finally 语句
struct TryStmt : Statement {
    BlockStmt* tryBlock;
    std::string catchVariable;
    BlockStmt* catchBlock;
    BlockStmt* finallyBlock;
    
    TryStmt(BlockStmt* tryB, std::string catchVar, BlockStmt* catchB, 
            BlockStmt* finallyB, const SourceLocation& loc)
        : tryBlock(tryB), catchVariable(std::move(catchVar)), 
          catchBlock(catchB), finallyBlock(finallyB) {
        location = loc;
    }
    
    StmtPtr clone() const override {
        return std::make_shared<TryStmt>(*this);
    }
    
    std::string toString() const override {
        return "try {...} catch {...}";
    }
};

// switch 语句
struct SwitchStmt : Statement {
    ExprPtr discriminant;
    struct Case {
        ExprPtr value;
        std::vector<StmtPtr> statements;
        bool isDefault;
    };
    std::vector<Case> cases;
    
    SwitchStmt(ExprPtr disc, std::vector<Case> c, const SourceLocation& loc)
        : discriminant(std::move(disc)), cases(std::move(c)) {
        location = loc;
    }
    
    StmtPtr clone() const override {
        return std::make_shared<SwitchStmt>(*this);
    }
    
    std::string toString() const override {
        return "switch (...) {...}";
    }
};

// ==================== 声明 ====================

// 声明基类
struct Declaration : Statement {
    // 重写基类的 clone() 方法
    StmtPtr clone() const override {
        return cloneImpl();
    }
    
    // 返回 DeclPtr 的版本
    DeclPtr cloneDecl() const {
        return std::static_pointer_cast<Declaration>(cloneImpl());
    }

protected:
    virtual StmtPtr cloneImpl() const = 0;
    
public:
    // 添加 toString 实现
    std::string toString() const override {
        return "Declaration";
    }
};

// 变量声明
struct VariableDecl : Declaration {
    std::string name;
    ExprPtr initializer;
    bool isConst;
    
    VariableDecl(std::string n, ExprPtr init, bool isConst, const SourceLocation& loc)
        : name(std::move(n)), initializer(std::move(init)), isConst(isConst) {
        location = loc;
    }
    
    StmtPtr cloneImpl() const override {
        return std::make_shared<VariableDecl>(*this);
    }
    
    std::string toString() const override {
        return (isConst ? "const " : "auto ") + name + 
               (initializer ? " = " + initializer->toString() : "");
    }
};

// 函数声明
struct FunctionDecl : Declaration {
    std::string name;
    std::vector<std::string> parameters;
    BlockStmt* body;
    bool isAsync;
    bool isStatic;
    
    FunctionDecl(std::string n, std::vector<std::string> params, 
                 BlockStmt* b, bool async, bool isStatic, const SourceLocation& loc)
        : name(std::move(n)), parameters(std::move(params)), 
          body(b), isAsync(async), isStatic(isStatic) {
        location = loc;
    }
    
    StmtPtr cloneImpl() const override {
        return std::make_shared<FunctionDecl>(*this);
    }
    
    std::string toString() const override {
        return "function " + name + "() {...}";
    }
};

// 类声明
struct ClassDecl : Declaration {
    std::string name;
    std::string superClass;
    std::vector<DeclPtr> members;

    ClassDecl(std::string n, std::string super,
              std::vector<DeclPtr> m, const SourceLocation& loc)
        : name(std::move(n)), superClass(std::move(super)),
          members(std::move(m)) {
        location = loc;
    }

    StmtPtr cloneImpl() const override {
        return std::make_shared<ClassDecl>(*this);
    }

    std::string toString() const override {
        return "class " + name + " {...}";
    }
};

// 模块声明
struct ModuleDecl : Declaration {
    std::string name;
    BlockStmt* body;
    
    ModuleDecl(std::string n, BlockStmt* b, const SourceLocation& loc)
        : name(std::move(n)), body(b) {
        location = loc;
    }
    
    StmtPtr cloneImpl() const override {
        return std::make_shared<ModuleDecl>(*this);
    }
    
    std::string toString() const override {
        return "module " + name + " {...}";
    }
};

// 导入声明
struct ImportDecl : Declaration {
    struct ImportSpecifier {
        std::string importedName;
        std::string localName;
    };
    
    std::string modulePath;
    std::string namespaceAlias;
    std::vector<ImportSpecifier> specifiers;
    std::string defaultImport;
    
    ImportDecl(std::string path, std::string nsAlias,
               std::vector<ImportSpecifier> specs, std::string defaultImp,
               const SourceLocation& loc)
        : modulePath(std::move(path)), namespaceAlias(std::move(nsAlias)),
          specifiers(std::move(specs)), defaultImport(std::move(defaultImp)) {
        location = loc;
    }
    
    StmtPtr cloneImpl() const override {
        return std::make_shared<ImportDecl>(*this);
    }
    
    std::string toString() const override {
        return "import ... from \"" + modulePath + "\"";
    }
};

// 导出声明
struct ExportDecl : Declaration {
    DeclPtr declaration;
    std::string name;
    bool isDefault;
    
    ExportDecl(DeclPtr decl, std::string n, bool isDefault, const SourceLocation& loc)
        : declaration(std::move(decl)), name(std::move(n)), isDefault(isDefault) {
        location = loc;
    }
    
    StmtPtr cloneImpl() const override {
        return std::make_shared<ExportDecl>(*this);
    }
    
    std::string toString() const override {
        return "export ...";
    }
};

// ==================== 程序 ====================

// 程序根节点
struct Program : ASTNode {
    std::vector<DeclPtr> declarations;
    std::string sourceFilename;
    
    Program(std::string filename = "<input>")
        : sourceFilename(std::move(filename)) {
        location = SourceLocation(sourceFilename, 0, 0, 0);
    }
    
    std::string toString() const override {
        std::string result;
        for (const auto& decl : declarations) {
            result += decl->toString() + "\n";
        }
        return result;
    }
};

} // namespace zcc

#endif // ZCC_AST_HPP

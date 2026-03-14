#ifndef ZCC_PARSER_HPP
#define ZCC_PARSER_HPP

#include <vector>
#include <memory>
#include "AST.hpp"
#include "Token.hpp"
#include "Lexer.hpp"
#include "Diagnostics.hpp"

namespace zcc {

// 语法分析器类
class Parser {
public:
    explicit Parser(std::vector<Token> tokens, std::string filename = "<input>");
    
    // 解析程序
    ProgramPtr parse();
    
    // 获取错误列表
    const std::vector<Diagnostic>& getErrors() const { return errors_; }
    
    // 是否有错误
    bool hasErrors() const { return !errors_.empty(); }

private:
    std::vector<Token> tokens_;
    std::string filename_;
    size_t current_;
    std::vector<Diagnostic> errors_;
    
    // 基础解析函数
    bool isAtEnd() const;
    Token peek() const;
    Token previous() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool match(std::initializer_list<TokenType> types);
    Token expect(TokenType type, const std::string& message);
    void synchronize();
    
    // 错误处理
    void error(const Token& token, const std::string& message);
    void error(size_t line, size_t column, const std::string& message);
    
    // 解析声明
    DeclPtr parseDeclaration();
    DeclPtr parseVariableDeclaration(bool isConst);
    DeclPtr parseFunctionDeclaration();
    DeclPtr parseClassDeclaration();
    DeclPtr parseModuleDeclaration();
    DeclPtr parseImportDeclaration();
    DeclPtr parseExportDeclaration();
    
    // 解析语句
    StmtPtr parseStatement();
    BlockStmt* parseBlock();
    StmtPtr parseExpressionStatement();
    StmtPtr parseIfStatement();
    StmtPtr parseForStatement();
    StmtPtr parseWhileStatement();
    StmtPtr parseForEachStatement();
    StmtPtr parseReturnStatement();
    StmtPtr parseBreakStatement();
    StmtPtr parseContinueStatement();
    StmtPtr parseThrowStatement();
    StmtPtr parseTryStatement();
    StmtPtr parseSwitchStatement();
    
    // 解析表达式
    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseTernary();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    ExprPtr parseBitwiseOr();
    ExprPtr parseBitwiseXor();
    ExprPtr parseBitwiseAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseShift();
    ExprPtr parseAdditive();
    ExprPtr parseMultiplicative();
    ExprPtr parseExponentiation();
    ExprPtr parseUnary();
    ExprPtr parseCall();
    ExprPtr parsePrimary();
    
    // 辅助解析函数
    ExprPtr parseFunctionExpression(bool isAsync);
    ExprPtr parseArrowFunction(std::vector<std::string> params, bool isAsync);
    ExprPtr parseArrayLiteral();
    ExprPtr parseObjectLiteral();
    std::vector<ExprPtr> parseArguments();
    
    // 源位置辅助
    SourceLocation getLocation(const Token& token) const;
    SourceLocation getCurrentLocation() const;
};

} // namespace zcc

#endif // ZCC_PARSER_HPP

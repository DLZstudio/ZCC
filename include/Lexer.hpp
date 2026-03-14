#ifndef ZCC_LEXER_HPP
#define ZCC_LEXER_HPP

#include <string>
#include <vector>
#include <optional>
#include "Token.hpp"
#include "Keywords.hpp"
#include "Diagnostics.hpp"

namespace zcc {

// 词法分析器类
class Lexer {
public:
    explicit Lexer(std::string source, std::string filename = "<input>");
    
    // 生成所有 Token
    std::vector<Token> tokenize();
    
    // 获取下一个 Token (用于递归下降解析)
    Token nextToken();
    
    // 查看下一个 Token 但不消耗
    Token peekToken() const;
    
    // 查看后 N 个 Token
    Token peekAt(size_t offset) const;
    
    // 检查是否到达末尾
    bool isAtEnd() const;
    
    // 获取当前行号
    size_t getLine() const { return line_; }
    
    // 获取当前列号
    size_t getColumn() const { return column_; }
    
    // 获取错误列表
    const std::vector<Diagnostic>& getErrors() const { return errors_; }
    
    // 是否有错误
    bool hasErrors() const { return !errors_.empty(); }

private:
    std::string source_;
    std::string filename_;
    size_t start_;
    size_t current_;
    size_t line_;
    size_t column_;
    std::vector<Diagnostic> errors_;
    
    Token scanToken();
    
    bool isAtEndInternal() const { return current_ >= source_.length(); }
    char advance();
    char peekChar() const;
    char peekNext() const;
    bool match(char expected);
    
    std::optional<Token> scanWhitespaceOrComment();
    std::optional<Token> scanIdentifier();
    std::optional<Token> scanNumber();
    std::optional<Token> scanString();
    std::optional<Token> scanOperator();
    
    void addError(const std::string& message, size_t line, size_t column);
    Token makeToken(TokenType type, TokenValue value = std::monostate{});
    Token errorToken(const std::string& message);
    
    bool isIdentifierStart(char c) const;
    bool isIdentifierChar(char c) const;
    bool isDigit(char c) const;
    void skipWhitespace();
};

} // namespace zcc

#endif // ZCC_LEXER_HPP

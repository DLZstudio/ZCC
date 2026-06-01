#ifndef ZCC_TOKEN_HPP
#define ZCC_TOKEN_HPP

#include <string>
#include <variant>
#include <memory>
#include "Keywords.hpp"

// 前向声明
namespace zcc { struct SourceLocation; }

namespace zcc {

// Token 类型枚举
enum class TokenType {
    // 特殊
    EOF_,
    ERROR,
    
    // 字面量
    IDENTIFIER,
    NUMBER,
    STRING,
    BOOLEAN,
    NULL_,
    UNDEFINED,
    
    // 关键词 (由 Keywords.hpp 映射)
    AUTO, CONST, FUNCTION, MAIN, RETURN,
    IF, ELSE, ELSEIF, FOR, WHILE, BREAK, CONTINUE,
    IMPORT, EXPORT, MODULE, FROM, AS, DEFAULT_,
    TRY, CATCH, FINALLY, THROW,
    TRUE, FALSE,
    NEW, CLASS, EXTENDS, CONSTRUCTOR, THIS, SUPER, STATIC_, GET, SET,
    EACH, IN, ZHONG,
    ASYNC, AWAIT, PROMISE, RESOLVE, REJECT,
    THREAD, START, SLEEP, WAIT_ALL, CLOSE,
    SWITCH, CASE,
    ENUM, NAMESPACE,

    FUNC,
    
    // 运算符
    // 算术运算符
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /
    PERCENT,        // %
    STAR_STAR,      // ** (幂运算)
    PLUS_PLUS,      // ++
    MINUS_MINUS,    // --
    
    // 赋值运算符
    EQUAL,          // =
    PLUS_EQUAL,     // +=
    MINUS_EQUAL,    // -=
    STAR_EQUAL,     // *=
    SLASH_EQUAL,    // /=
    PERCENT_EQUAL,  // %=
    
    // 比较运算符
    EQUAL_EQUAL,    // ==
    BANG_EQUAL,     // !=
    LESS,           // <
    GREATER,        // >
    LESS_EQUAL,     // <=
    GREATER_EQUAL,  // >=
    EQUAL_EQUAL_EQUAL,  // ===
    BANG_EQUAL_EQUAL,   // !==
    
    // 逻辑运算符
    BANG,           // !
    AMPERSAND_AMPERSAND,  // &&
    PIPE_PIPE,      // ||
    
    // 位运算符
    AMPERSAND,      // &
    PIPE,           // |
    CARET,          // ^
    TILDE,          // ~
    LESS_LESS,      // <<
    GREATER_GREATER,// >>
    
    // 分隔符
    LEFT_PAREN,     // (
    RIGHT_PAREN,    // )
    LEFT_BRACE,     // {
    RIGHT_BRACE,    // }
    LEFT_BRACKET,   // [
    RIGHT_BRACKET,  // ]
    AT_EXTERNAL,    // @external / @外部
    
    // 其他符号
    COMMA,          // ,
    DOT,            // .
    SEMICOLON,      // ;
    COLON,          // :
    QUESTION,       // ?
    ARROW,          // =>
    DOT_DOT_DOT,    // ... (剩余参数)
    
    // 注释
    COMMENT_SINGLE,   // //
    COMMENT_MULTI,    // /* */
};

// Token 值类型
using TokenValue = std::variant<
    std::monostate,     // 无值
    std::string,        // 标识符、字符串
    double,             // 数字
    bool                // 布尔值
>;

// Token 类
class Token {
public:
    TokenType type;
    TokenValue value;
    std::string lexeme;     // 原始词素
    size_t line;            // 行号
    size_t column;          // 列号
    size_t length;          // 词素长度

    Token(TokenType type, TokenValue value, std::string lexeme, 
          size_t line, size_t column, size_t length = 0)
        : type(type), value(value), lexeme(std::move(lexeme)),
          line(line), column(column), length(length) {}

    // 获取字符串值
    std::string getString() const {
        if (auto str = std::get_if<std::string>(&value)) {
            return *str;
        }
        return lexeme;
    }

    // 获取数字值
    double getNumber() const {
        if (auto num = std::get_if<double>(&value)) {
            return *num;
        }
        return 0.0;
    }

    // 获取布尔值
    bool getBoolean() const {
        if (auto b = std::get_if<bool>(&value)) {
            return *b;
        }
        return false;
    }

    // 判断是否为标识符
    bool isIdentifier() const {
        return type == TokenType::IDENTIFIER;
    }

    // 判断是否为字面量
    bool isLiteral() const {
        return type == TokenType::NUMBER ||
               type == TokenType::STRING ||
               type == TokenType::BOOLEAN ||
               type == TokenType::NULL_ ||
               type == TokenType::UNDEFINED;
    }

    // 默认构造函数
    Token() : type(TokenType::EOF_), value(std::monostate{}), lexeme(""),
              line(0), column(0), length(0) {}

    // Token 类型名称
    std::string typeName() const {
        return tokenTypeToString(type);
    }

    // 转换为字符串表示
    std::string toString() const {
        std::string result = typeName() + " " + lexeme;
        if (type == TokenType::STRING) {
            if (auto str = std::get_if<std::string>(&value)) {
                result += " \"" + *str + "\"";
            }
        } else if (type == TokenType::NUMBER) {
            if (auto num = std::get_if<double>(&value)) {
                result += " (" + std::to_string(*num) + ")";
            }
        }
        return result;
    }

private:
    static std::string tokenTypeToString(TokenType type) {
        switch (type) {
            case TokenType::EOF_: return "EOF";
            case TokenType::ERROR: return "ERROR";
            case TokenType::IDENTIFIER: return "IDENTIFIER";
            case TokenType::NUMBER: return "NUMBER";
            case TokenType::STRING: return "STRING";
            case TokenType::BOOLEAN: return "BOOLEAN";
            case TokenType::NULL_: return "NULL";
            case TokenType::UNDEFINED: return "UNDEFINED";
            
            // 关键词
            case TokenType::AUTO: return "AUTO";
            case TokenType::CONST: return "CONST";
            case TokenType::FUNCTION: return "FUNCTION";
            case TokenType::MAIN: return "MAIN";
            case TokenType::RETURN: return "RETURN";
            case TokenType::IF: return "IF";
            case TokenType::ELSE: return "ELSE";
            case TokenType::ELSEIF: return "ELSEIF";
            case TokenType::FOR: return "FOR";
            case TokenType::WHILE: return "WHILE";
            case TokenType::BREAK: return "BREAK";
            case TokenType::CONTINUE: return "CONTINUE";
            case TokenType::IMPORT: return "IMPORT";
            case TokenType::EXPORT: return "EXPORT";
            case TokenType::MODULE: return "MODULE";
            case TokenType::FROM: return "FROM";
            case TokenType::AS: return "AS";
            case TokenType::DEFAULT_: return "DEFAULT";
            case TokenType::TRY: return "TRY";
            case TokenType::CATCH: return "CATCH";
            case TokenType::FINALLY: return "FINALLY";
            case TokenType::THROW: return "THROW";
            case TokenType::TRUE: return "TRUE";
            case TokenType::FALSE: return "FALSE";
            case TokenType::NEW: return "NEW";
            case TokenType::CLASS: return "CLASS";
            case TokenType::EXTENDS: return "EXTENDS";
            case TokenType::CONSTRUCTOR: return "CONSTRUCTOR";
            case TokenType::THIS: return "THIS";
            case TokenType::SUPER: return "SUPER";
            case TokenType::STATIC_: return "STATIC";
            case TokenType::GET: return "GET";
            case TokenType::SET: return "SET";
            case TokenType::EACH: return "EACH";
            case TokenType::IN: return "IN";
            case TokenType::ZHONG: return "ZHONG";
            case TokenType::ASYNC: return "ASYNC";
            case TokenType::AWAIT: return "AWAIT";
            case TokenType::PROMISE: return "PROMISE";
            case TokenType::RESOLVE: return "RESOLVE";
            case TokenType::REJECT: return "REJECT";
            case TokenType::THREAD: return "THREAD";
            case TokenType::START: return "START";
            case TokenType::SLEEP: return "SLEEP";
            case TokenType::WAIT_ALL: return "WAIT_ALL";
            case TokenType::CLOSE: return "CLOSE";
            case TokenType::SWITCH: return "SWITCH";
            case TokenType::CASE: return "CASE";
            


            case TokenType::ENUM: return "ENUM";
            case TokenType::NAMESPACE: return "NAMESPACE";
            case TokenType::FUNC: return "FUNC";
            
            // 运算符
            case TokenType::PLUS: return "PLUS";
            case TokenType::MINUS: return "MINUS";
            case TokenType::STAR: return "STAR";
            case TokenType::SLASH: return "SLASH";
            case TokenType::PERCENT: return "PERCENT";
            case TokenType::STAR_STAR: return "STAR_STAR";
            case TokenType::PLUS_PLUS: return "PLUS_PLUS";
            case TokenType::MINUS_MINUS: return "MINUS_MINUS";
            case TokenType::EQUAL: return "EQUAL";
            case TokenType::PLUS_EQUAL: return "PLUS_EQUAL";
            case TokenType::MINUS_EQUAL: return "MINUS_EQUAL";
            case TokenType::STAR_EQUAL: return "STAR_EQUAL";
            case TokenType::SLASH_EQUAL: return "SLASH_EQUAL";
            case TokenType::PERCENT_EQUAL: return "PERCENT_EQUAL";
            case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
            case TokenType::BANG_EQUAL: return "BANG_EQUAL";
            case TokenType::LESS: return "LESS";
            case TokenType::GREATER: return "GREATER";
            case TokenType::LESS_EQUAL: return "LESS_EQUAL";
            case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
            case TokenType::EQUAL_EQUAL_EQUAL: return "EQUAL_EQUAL_EQUAL";
            case TokenType::BANG_EQUAL_EQUAL: return "BANG_EQUAL_EQUAL";
            case TokenType::BANG: return "BANG";
            case TokenType::AMPERSAND_AMPERSAND: return "AMPERSAND_AMPERSAND";
            case TokenType::PIPE_PIPE: return "PIPE_PIPE";
            case TokenType::AMPERSAND: return "AMPERSAND";
            case TokenType::PIPE: return "PIPE";
            case TokenType::CARET: return "CARET";
            case TokenType::TILDE: return "TILDE";
            case TokenType::LESS_LESS: return "LESS_LESS";
            case TokenType::GREATER_GREATER: return "GREATER_GREATER";
            
            // 分隔符
            case TokenType::LEFT_PAREN: return "LEFT_PAREN";
            case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
            case TokenType::LEFT_BRACE: return "LEFT_BRACE";
            case TokenType::RIGHT_BRACE: return "RIGHT_BRACE";
            case TokenType::LEFT_BRACKET: return "LEFT_BRACKET";
            case TokenType::RIGHT_BRACKET: return "RIGHT_BRACKET";
            case TokenType::AT_EXTERNAL: return "AT_EXTERNAL";
            case TokenType::COMMA: return "COMMA";
            case TokenType::DOT: return "DOT";
            case TokenType::SEMICOLON: return "SEMICOLON";
            case TokenType::COLON: return "COLON";
            case TokenType::QUESTION: return "QUESTION";
            case TokenType::ARROW: return "ARROW";
            case TokenType::DOT_DOT_DOT: return "DOT_DOT_DOT";
            
            // 注释
            case TokenType::COMMENT_SINGLE: return "COMMENT_SINGLE";
            case TokenType::COMMENT_MULTI: return "COMMENT_MULTI";
            
            default: return "UNKNOWN";
        }
    }
};

} // namespace zcc

#endif // ZCC_TOKEN_HPP

#include "Lexer.hpp"
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace zcc {

Lexer::Lexer(std::string source, std::string filename)
    : source_(std::move(source)), filename_(std::move(filename)),
      start_(0), current_(0), line_(1), column_(1) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (!isAtEndInternal()) {
        start_ = current_;
        Token token = scanToken();
        if (token.type != TokenType::COMMENT_SINGLE && 
            token.type != TokenType::COMMENT_MULTI) {
            tokens.push_back(std::move(token));
        }
    }
    
    tokens.push_back(Token(TokenType::EOF_, std::monostate{}, "", line_, column_, 0));
    return tokens;
}

Token Lexer::nextToken() {
    skipWhitespace();
    start_ = current_;
    return scanToken();
}

Token Lexer::peekToken() const {
    // 简化的 peek 实现
    return Token(TokenType::EOF_, std::monostate{}, "", line_, column_, 0);
}

Token Lexer::peekAt(size_t offset) const {
    return peekToken();
}

bool Lexer::isAtEnd() const {
    return isAtEndInternal();
}

char Lexer::advance() {
    if (isAtEndInternal()) return '\0';
    char c = source_[current_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

char Lexer::peekChar() const {
    if (isAtEndInternal()) return '\0';
    return source_[current_];
}

char Lexer::peekNext() const {
    if (current_ + 1 >= source_.length()) return '\0';
    return source_[current_ + 1];
}

bool Lexer::match(char expected) {
    if (isAtEndInternal()) return false;
    if (source_[current_] != expected) return false;
    advance();
    return true;
}

Token Lexer::scanToken() {
    // 检查空白和注释
    if (auto comment = scanWhitespaceOrComment()) {
        return *comment;
    }
    
    // 标识符和关键词
    if (auto ident = scanIdentifier()) {
        return *ident;
    }
    
    // 数字
    if (auto num = scanNumber()) {
        return *num;
    }
    
    // 字符串
    if (peekChar() == '"' || peekChar() == '\'' || peekChar() == '`') {
        if (auto str = scanString()) {
            return *str;
        }
    }
    
    // 运算符和标点符号
    if (auto op = scanOperator()) {
        return *op;
    }
    
    // 未知字符
    return errorToken("Unexpected character: " + std::string(1, peekChar()));
}

std::optional<Token> Lexer::scanWhitespaceOrComment() {
    skipWhitespace();
    
    // 检查注释
    if (peekChar() == '/' && !isAtEndInternal()) {
        if (peekNext() == '/') {
            // 单行注释
            start_ = current_;
            advance(); // /
            advance(); // /
            while (peekChar() != '\n' && !isAtEndInternal()) {
                advance();
            }
            size_t len = current_ - start_;
            return makeToken(TokenType::COMMENT_SINGLE, 
                           source_.substr(start_, len));
        } else if (peekNext() == '*') {
            // 多行注释
            start_ = current_;
            advance(); // /
            advance(); // *
            size_t startLine = line_;
            size_t startCol = column_;
            
            while (!isAtEndInternal() && !(peekChar() == '*' && peekNext() == '/')) {
                advance();
            }
            
            if (isAtEndInternal()) {
                addError("Unterminated multi-line comment", startLine, startCol);
            } else {
                advance(); // *
                advance(); // /
            }
            
            size_t len = current_ - start_;
            return makeToken(TokenType::COMMENT_MULTI,
                           source_.substr(start_, len));
        }
    }
    
    return std::nullopt;
}

void Lexer::skipWhitespace() {
    while (!isAtEndInternal()) {
        char c = peekChar();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peekNext() == '/') {
            // 单行注释
            advance();
            advance();
            while (peekChar() != '\n' && !isAtEndInternal()) {
                advance();
            }
        } else if (c == '/' && peekNext() == '*') {
            // 多行注释
            advance();
            advance();
            while (!isAtEndInternal() && !(peekChar() == '*' && peekNext() == '/')) {
                advance();
            }
            if (!isAtEndInternal()) {
                advance();
                advance();
            }
        } else {
            break;
        }
    }
}

std::optional<Token> Lexer::scanIdentifier() {
    if (!isIdentifierStart(peekChar())) {
        return std::nullopt;
    }
    
    start_ = current_;
    size_t startLine = line_;
    size_t startCol = column_;
    
    while (!isAtEndInternal() && isIdentifierChar(peekChar())) {
        advance();
    }
    
    std::string text = source_.substr(start_, current_ - start_);

    // 检查是否为关键词
    KeywordMap::KeywordType keywordType = getKeywordMap().lookup(text);
    if (keywordType != KeywordMap::KeywordType::NOT_KEYWORD) {
        TokenType tokenType = TokenType::IDENTIFIER;
        TokenValue value = std::monostate{};
        
        // 映射关键词到 TokenType
        switch (keywordType) {
            case KeywordMap::KeywordType::AUTO: tokenType = TokenType::AUTO; break;
            case KeywordMap::KeywordType::CONST: tokenType = TokenType::CONST; break;
            case KeywordMap::KeywordType::FUNCTION: tokenType = TokenType::FUNCTION; break;
            case KeywordMap::KeywordType::MAIN: tokenType = TokenType::MAIN; break;
            case KeywordMap::KeywordType::RETURN: tokenType = TokenType::RETURN; break;
            case KeywordMap::KeywordType::IF: tokenType = TokenType::IF; break;
            case KeywordMap::KeywordType::ELSE: tokenType = TokenType::ELSE; break;
            case KeywordMap::KeywordType::ELSEIF: tokenType = TokenType::ELSEIF; break;
            case KeywordMap::KeywordType::FOR: tokenType = TokenType::FOR; break;
            case KeywordMap::KeywordType::WHILE: tokenType = TokenType::WHILE; break;
            case KeywordMap::KeywordType::BREAK: tokenType = TokenType::BREAK; break;
            case KeywordMap::KeywordType::CONTINUE: tokenType = TokenType::CONTINUE; break;
            case KeywordMap::KeywordType::IMPORT: tokenType = TokenType::IMPORT; break;
            case KeywordMap::KeywordType::EXPORT: tokenType = TokenType::EXPORT; break;
            case KeywordMap::KeywordType::MODULE: tokenType = TokenType::MODULE; break;
            case KeywordMap::KeywordType::FROM: tokenType = TokenType::FROM; break;
            case KeywordMap::KeywordType::AS: tokenType = TokenType::AS; break;
            case KeywordMap::KeywordType::DEFAULT_: tokenType = TokenType::DEFAULT_; break;
            case KeywordMap::KeywordType::TRY: tokenType = TokenType::TRY; break;
            case KeywordMap::KeywordType::CATCH: tokenType = TokenType::CATCH; break;
            case KeywordMap::KeywordType::FINALLY: tokenType = TokenType::FINALLY; break;
            case KeywordMap::KeywordType::THROW: tokenType = TokenType::THROW; break;
            case KeywordMap::KeywordType::TRUE: 
                tokenType = TokenType::TRUE; 
                value = true;
                break;
            case KeywordMap::KeywordType::FALSE: 
                tokenType = TokenType::FALSE; 
                value = false;
                break;
            case KeywordMap::KeywordType::NULL_: 
                tokenType = TokenType::NULL_; 
                value = std::monostate{};
                break;
            case KeywordMap::KeywordType::UNDEFINED: 
                tokenType = TokenType::UNDEFINED; 
                value = std::monostate{};
                break;
            case KeywordMap::KeywordType::NEW: tokenType = TokenType::NEW; break;
            case KeywordMap::KeywordType::CLASS: tokenType = TokenType::CLASS; break;
            case KeywordMap::KeywordType::EXTENDS: tokenType = TokenType::EXTENDS; break;
            case KeywordMap::KeywordType::CONSTRUCTOR: tokenType = TokenType::CONSTRUCTOR; break;
            case KeywordMap::KeywordType::THIS: tokenType = TokenType::THIS; break;
            case KeywordMap::KeywordType::SUPER: tokenType = TokenType::SUPER; break;
            case KeywordMap::KeywordType::STATIC_: tokenType = TokenType::STATIC_; break;
            case KeywordMap::KeywordType::GET: tokenType = TokenType::GET; break;
            case KeywordMap::KeywordType::SET: tokenType = TokenType::SET; break;
            case KeywordMap::KeywordType::EACH: tokenType = TokenType::EACH; break;
            case KeywordMap::KeywordType::IN: tokenType = TokenType::IN; break;
            case KeywordMap::KeywordType::ASYNC: tokenType = TokenType::ASYNC; break;
            case KeywordMap::KeywordType::AWAIT: tokenType = TokenType::AWAIT; break;
            case KeywordMap::KeywordType::PROMISE: tokenType = TokenType::PROMISE; break;
            case KeywordMap::KeywordType::RESOLVE: tokenType = TokenType::RESOLVE; break;
            case KeywordMap::KeywordType::REJECT: tokenType = TokenType::REJECT; break;
            case KeywordMap::KeywordType::THREAD: tokenType = TokenType::THREAD; break;
            case KeywordMap::KeywordType::START: tokenType = TokenType::START; break;
            case KeywordMap::KeywordType::SLEEP: tokenType = TokenType::SLEEP; break;
            case KeywordMap::KeywordType::WAIT_ALL: tokenType = TokenType::WAIT_ALL; break;
            case KeywordMap::KeywordType::CLOSE: tokenType = TokenType::CLOSE; break;
            case KeywordMap::KeywordType::SWITCH: tokenType = TokenType::SWITCH; break;
            case KeywordMap::KeywordType::CASE: tokenType = TokenType::CASE; break;
            default: tokenType = TokenType::IDENTIFIER; break;
        }
        
        return Token(tokenType, value, text, startLine, startCol, text.length());
    }
    
    // 普通标识符
    return makeToken(TokenType::IDENTIFIER, text);
}

std::optional<Token> Lexer::scanNumber() {
    if (!isDigit(peekChar())) {
        return std::nullopt;
    }
    
    start_ = current_;
    size_t startLine = line_;
    size_t startCol = column_;
    
    // 整数部分
    while (isDigit(peekChar())) {
        advance();
    }
    
    // 小数部分
    if (peekChar() == '.' && isDigit(peekNext())) {
        advance(); // .
        while (isDigit(peekChar())) {
            advance();
        }
    }
    
    // 科学计数法
    if (peekChar() == 'e' || peekChar() == 'E') {
        advance();
        if (peekChar() == '+' || peekChar() == '-') {
            advance();
        }
        if (!isDigit(peekChar())) {
            addError("Expected digit after exponent", line_, column_);
        }
        while (isDigit(peekChar())) {
            advance();
        }
    }
    
    std::string text = source_.substr(start_, current_ - start_);
    
    try {
        double value = std::stod(text);
        return makeToken(TokenType::NUMBER, value);
    } catch (const std::exception& e) {
        addError("Invalid number: " + text, startLine, startCol);
        return makeToken(TokenType::NUMBER, 0.0);
    }
}

std::optional<Token> Lexer::scanString() {
    char quote = peekChar();
    if (quote != '"' && quote != '\'' && quote != '`') {
        return std::nullopt;
    }
    
    start_ = current_;
    size_t startLine = line_;
    size_t startCol = column_;
    
    advance(); // 开头的引号
    
    std::string value;
    while (!isAtEndInternal() && peekChar() != quote) {
        if (peekChar() == '\n') {
            addError("Unterminated string", startLine, startCol);
            break;
        }
        
        if (peekChar() == '\\') {
            advance();
            if (!isAtEndInternal()) {
                char escape = peekChar();
                switch (escape) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '"': case '\'': case '`': value += escape; break;
                    case '/': value += '/'; break;
                    case 'b': value += '\b'; break;
                    case 'f': value += '\f'; break;
                    default:
                        addError("Invalid escape sequence: \\" + std::string(1, escape), 
                                line_, column_);
                        value += escape;
                        break;
                }
                advance();
            }
        } else {
            value += peekChar();
            advance();
        }
    }
    
    if (isAtEndInternal()) {
        addError("Unterminated string", startLine, startCol);
    } else {
        advance(); // 结尾的引号
    }
    
    return makeToken(TokenType::STRING, value);
}

std::optional<Token> Lexer::scanOperator() {
    size_t startLine = line_;
    size_t startCol = column_;
    
    char c = advance();
    
    // 单字符运算符
    switch (c) {
        case '(': return makeToken(TokenType::LEFT_PAREN);
        case ')': return makeToken(TokenType::RIGHT_PAREN);
        case '{': return makeToken(TokenType::LEFT_BRACE);
        case '}': return makeToken(TokenType::RIGHT_BRACE);
        case '[': return makeToken(TokenType::LEFT_BRACKET);
        case ']': return makeToken(TokenType::RIGHT_BRACKET);
        case ',': return makeToken(TokenType::COMMA);
        case '.': return makeToken(TokenType::DOT);
        case ';': return makeToken(TokenType::SEMICOLON);
        case ':': return makeToken(TokenType::COLON);
        case '?': return makeToken(TokenType::QUESTION);
        case '+': return match('=') ? makeToken(TokenType::PLUS_EQUAL) : 
                                   (match('+') ? makeToken(TokenType::PLUS_PLUS) : 
                                    makeToken(TokenType::PLUS));
        case '-': return match('=') ? makeToken(TokenType::MINUS_EQUAL) :
                                   (match('-') ? makeToken(TokenType::MINUS_MINUS) :
                                    (match('>') ? makeToken(TokenType::ARROW) :
                                     makeToken(TokenType::MINUS)));
        case '*': return match('=') ? makeToken(TokenType::STAR_EQUAL) :
                                     (match('*') ? makeToken(TokenType::STAR_STAR) :
                                      makeToken(TokenType::STAR));
        case '/': return match('=') ? makeToken(TokenType::SLASH_EQUAL) :
                                     makeToken(TokenType::SLASH);
        case '%': return match('=') ? makeToken(TokenType::PERCENT_EQUAL) :
                                     makeToken(TokenType::PERCENT);
        case '!': return match('=') ? (match('=') ? makeToken(TokenType::BANG_EQUAL_EQUAL) :
                                                    makeToken(TokenType::BANG_EQUAL)) :
                                     makeToken(TokenType::BANG);
        case '=': return match('=') ? (match('=') ? makeToken(TokenType::EQUAL_EQUAL_EQUAL) :
                                                    makeToken(TokenType::EQUAL_EQUAL)) :
                                     makeToken(TokenType::EQUAL);
        case '<': return match('=') ? makeToken(TokenType::LESS_EQUAL) :
                                     (match('<') ? makeToken(TokenType::LESS_LESS) :
                                      makeToken(TokenType::LESS));
        case '>': return match('=') ? makeToken(TokenType::GREATER_EQUAL) :
                                     (match('>') ? makeToken(TokenType::GREATER_GREATER) :
                                      makeToken(TokenType::GREATER));
        case '&': return match('&') ? makeToken(TokenType::AMPERSAND_AMPERSAND) :
                                     makeToken(TokenType::AMPERSAND);
        case '|': return match('|') ? makeToken(TokenType::PIPE_PIPE) :
                                     makeToken(TokenType::PIPE);
        case '^': return makeToken(TokenType::CARET);
        case '~': return makeToken(TokenType::TILDE);
    }
    
    // 回退
    current_ = start_;
    column_ = startCol;
    
    return std::nullopt;
}

bool Lexer::isIdentifierStart(char c) const {
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
        return true;
    }
    // UTF-8 中文字符
    if (static_cast<unsigned char>(c) >= 0xE0) {
        return true;
    }
    return false;
}

bool Lexer::isIdentifierChar(char c) const {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
        return true;
    }
    // UTF-8 字符
    if (static_cast<unsigned char>(c) >= 0x80) {
        return true;
    }
    return false;
}

bool Lexer::isDigit(char c) const {
    return std::isdigit(static_cast<unsigned char>(c));
}

void Lexer::addError(const std::string& message, size_t line, size_t column) {
    errors_.push_back(Diagnostic::error(message, SourceLocation(filename_, line, column, 1)));
}

Token Lexer::makeToken(TokenType type, TokenValue value) {
    std::string lexeme = source_.substr(start_, current_ - start_);
    return Token(type, std::move(value), std::move(lexeme), line_, column_, current_ - start_);
}

Token Lexer::errorToken(const std::string& message) {
    addError(message, line_, column_);
    std::string lexeme(1, peekChar());
    advance();
    return Token(TokenType::ERROR, std::monostate{}, std::move(lexeme), line_, column_, 1);
}

} // namespace zcc

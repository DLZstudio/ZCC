#include "Parser.hpp"
#include <stdexcept>
#include <sstream>
#include <iostream>

namespace zcc {

Parser::Parser(std::vector<Token> tokens, std::string filename)
    : tokens_(std::move(tokens)), filename_(std::move(filename)), current_(0) {}

ProgramPtr Parser::parse() {
    auto program = std::make_shared<Program>(filename_);
    
    while (!isAtEnd()) {
        try {
            auto decl = parseDeclaration();
            if (decl) {
                program->declarations.push_back(decl);
            }
        } catch (...) {
            synchronize();
        }
    }
    
    return program;
}

bool Parser::isAtEnd() const {
    return current_ >= tokens_.size() || tokens_[current_].type == TokenType::EOF_;
}

Token Parser::peek() const {
    if (isAtEnd()) return tokens_.back();
    return tokens_[current_];
}

Token Parser::previous() const {
    if (current_ == 0 || current_ > tokens_.size()) {
        return tokens_[0];
    }
    return tokens_[current_ - 1];
}

Token Parser::advance() {
    if (!isAtEnd()) current_++;
    return previous();
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (TokenType type : types) {
        if (match(type)) return true;
    }
    return false;
}

Token Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    
    Token token = peek();
    error(token, message);
    return token;
}

void Parser::synchronize() {
    advance();
    
    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMICOLON) return;
        
        switch (peek().type) {
            case TokenType::FUNCTION:
            case TokenType::CLASS:
            case TokenType::AUTO:
            case TokenType::CONST:
            case TokenType::IF:
            case TokenType::FOR:
            case TokenType::WHILE:
            case TokenType::RETURN:
            case TokenType::TRY:
            case TokenType::IMPORT:
            case TokenType::EXPORT:
                return;
            default:
                break;
        }
        
        advance();
    }
}

void Parser::error(const Token& token, const std::string& message) {
    std::ostringstream oss;
    oss << message << " (at '" << token.lexeme << "')";
    errors_.push_back(Diagnostic::error(oss.str(), 
                                        SourceLocation(filename_, token.line, token.column, token.length)));
}

void Parser::error(size_t line, size_t column, const std::string& message) {
    errors_.push_back(Diagnostic::error(message, SourceLocation(filename_, line, column, 1)));
}

SourceLocation Parser::getLocation(const Token& token) const {
    return SourceLocation(filename_, token.line, token.column, token.length);
}

SourceLocation Parser::getCurrentLocation() const {
    return getLocation(peek());
}

// ==================== 解析声明 ====================

DeclPtr Parser::parseDeclaration() {
    try {
        // 调试输出
        Token currentToken = peek();
        std::cerr << "[DEBUG] parseDeclaration: token type=" << static_cast<int>(currentToken.type) 
                  << " lexeme=" << currentToken.lexeme << std::endl;
        
        // 模块声明
        if (match(TokenType::MODULE)) {
            return parseModuleDeclaration();
        }

        // 导入声明
        if (match(TokenType::IMPORT)) {
            return parseImportDeclaration();
        }

        // 导出声明
        if (match(TokenType::EXPORT)) {
            return parseExportDeclaration();
        }

        // 类声明
        if (match(TokenType::CLASS)) {
            return parseClassDeclaration();
        }

        // 函数声明
        if (match(TokenType::FUNCTION) || match(TokenType::ASYNC)) {
            std::cerr << "[DEBUG] Found FUNCTION or ASYNC keyword" << std::endl;
            return parseFunctionDeclaration();
        }

        // 变量声明
        if (match(TokenType::AUTO)) {
            return parseVariableDeclaration(false);
        }

        if (match(TokenType::CONST)) {
            return parseVariableDeclaration(true);
        }

        // 主函数 (特殊处理)
        if (match(TokenType::MAIN)) {
            if (check(TokenType::LEFT_PAREN)) {
                Token nameToken = previous();
                std::vector<std::string> params;

                expect(TokenType::LEFT_PAREN, "Expected '(' after 'main'");

                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        if (check(TokenType::IDENTIFIER)) {
                            params.push_back(advance().lexeme);
                        } else {
                            error(peek(), "Expected parameter name");
                            break;
                        }
                    } while (match(TokenType::COMMA));
                }

                expect(TokenType::RIGHT_PAREN, "Expected ')' after parameters");
                
                auto body = parseBlock();

                return std::make_shared<FunctionDecl>(
                    nameToken.lexeme, params, body, false, false, getLocation(nameToken));
            }
        }

        // 语句作为顶层声明
        auto stmt = parseStatement();
        if (stmt) {
            auto exprStmt = std::dynamic_pointer_cast<ExpressionStmt>(stmt);
            if (exprStmt) {
                auto loc = getLocation(previous());
                return std::make_shared<VariableDecl>("__top_level", exprStmt->expression, false, loc);
            }
        }

        return nullptr;

    } catch (...) {
        synchronize();
        return nullptr;
    }
}

DeclPtr Parser::parseVariableDeclaration(bool isConst) {
    Token nameToken = expect(TokenType::IDENTIFIER, "Expected variable name");
    std::string name = nameToken.lexeme;
    
    ExprPtr initializer;
    if (match(TokenType::EQUAL)) {
        initializer = parseExpression();
    }
    
    expect(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    
    return std::make_shared<VariableDecl>(name, initializer, isConst, getLocation(nameToken));
}

DeclPtr Parser::parseFunctionDeclaration() {
    // 检查是否是 async function
    bool isAsync = false;
    if (previous().type == TokenType::ASYNC) {
        isAsync = true;
        if (!match(TokenType::FUNCTION)) {
            error(peek(), "Expected 'function' after 'async'");
        }
    }
    // 如果前一个 token 是 FUNCTION，已经被 match 消耗了，不需要再处理
    // 如果前一个 token 是 ASYNC，已经在上面处理了

    // 允许 main 作为函数名
    Token nameToken;
    if (check(TokenType::MAIN)) {
        advance();
        Token prev = previous();
        nameToken = Token(TokenType::IDENTIFIER, std::monostate{}, "main", prev.line, prev.column, prev.length);
    } else {
        nameToken = expect(TokenType::IDENTIFIER, "Expected function name");
    }

    expect(TokenType::LEFT_PAREN, "Expected '(' after function name");
    
    std::vector<std::string> params;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            if (check(TokenType::IDENTIFIER)) {
                params.push_back(advance().lexeme);
            } else if (match(TokenType::DOT_DOT_DOT)) {
                if (check(TokenType::IDENTIFIER)) {
                    params.push_back("..." + advance().lexeme);
                }
            } else {
                error(peek(), "Expected parameter name");
                break;
            }
        } while (match(TokenType::COMMA));
    }
    
    expect(TokenType::RIGHT_PAREN, "Expected ')' after parameters");
    
    auto body = parseBlock();
    
    return std::make_shared<FunctionDecl>(
        nameToken.lexeme, params, body, isAsync, false, getLocation(nameToken));
}

DeclPtr Parser::parseClassDeclaration() {
    Token nameToken = expect(TokenType::IDENTIFIER, "Expected class name");
    std::string name = nameToken.lexeme;
    
    std::string superClass;
    if (match(TokenType::EXTENDS)) {
        Token superToken = expect(TokenType::IDENTIFIER, "Expected superclass name");
        superClass = superToken.lexeme;
    }
    
    expect(TokenType::LEFT_BRACE, "Expected '{' before class body");
    
    std::vector<DeclPtr> members;
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        if (match(TokenType::CONSTRUCTOR)) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after 'constructor'");
            
            std::vector<std::string> params;
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    if (check(TokenType::IDENTIFIER)) {
                        params.push_back(advance().lexeme);
                    }
                } while (match(TokenType::COMMA));
            }
            
            expect(TokenType::RIGHT_PAREN, "Expected ')' after constructor parameters");
            
            auto body = parseBlock();
            
            members.push_back(std::make_shared<FunctionDecl>(name, params, body, false, false, 
                                          SourceLocation(filename_, 0, 0, 0)));
        } else if (match(TokenType::STATIC_)) {
            if (match(TokenType::FUNCTION)) {
                Token methodToken = expect(TokenType::IDENTIFIER, "Expected method name");
                
                expect(TokenType::LEFT_PAREN, "Expected '(' after method name");
                
                std::vector<std::string> params;
                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        if (check(TokenType::IDENTIFIER)) {
                            params.push_back(advance().lexeme);
                        }
                    } while (match(TokenType::COMMA));
                }
                
                expect(TokenType::RIGHT_PAREN, "Expected ')' after method parameters");
                
                auto body = parseBlock();
                
                members.push_back(std::make_shared<FunctionDecl>(methodToken.lexeme, params, body, false, true,
                                              getLocation(methodToken)));
            }
        } else if (match(TokenType::FUNCTION) || match(TokenType::GET) || match(TokenType::SET)) {
            TokenType type = previous().type;
            Token methodToken = expect(TokenType::IDENTIFIER, "Expected method name");
            
            if (type == TokenType::GET || type == TokenType::SET) {
                expect(TokenType::LEFT_PAREN, "Expected '(' after getter/setter name");
                expect(TokenType::RIGHT_PAREN, "Expected ')' after getter/setter parameters");
                auto body = parseBlock();
                
                std::vector<std::string> params;
                members.push_back(std::make_shared<FunctionDecl>(methodToken.lexeme, params, body, false, false,
                                              getLocation(methodToken)));
            } else {
                expect(TokenType::LEFT_PAREN, "Expected '(' after method name");
                
                std::vector<std::string> params;
                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        if (check(TokenType::IDENTIFIER)) {
                            params.push_back(advance().lexeme);
                        }
                    } while (match(TokenType::COMMA));
                }
                
                expect(TokenType::RIGHT_PAREN, "Expected ')' after method parameters");
                
                auto body = parseBlock();
                
                members.push_back(std::make_shared<FunctionDecl>(methodToken.lexeme, params, body, false, false,
                                              getLocation(methodToken)));
            }
        } else if (check(TokenType::IDENTIFIER)) {
            Token fieldToken = advance();
            
            if (match(TokenType::EQUAL)) {
                parseExpression();
                expect(TokenType::SEMICOLON, "Expected ';' after field declaration");
            } else if (match(TokenType::SEMICOLON)) {
                // 无初始值
            } else {
                if (match(TokenType::LEFT_PAREN)) {
                    std::vector<std::string> params;
                    if (!check(TokenType::RIGHT_PAREN)) {
                        do {
                            if (check(TokenType::IDENTIFIER)) {
                                params.push_back(advance().lexeme);
                            }
                        } while (match(TokenType::COMMA));
                    }
                    expect(TokenType::RIGHT_PAREN, "Expected ')' after method parameters");
                    auto body = parseBlock();
                    members.push_back(std::make_shared<FunctionDecl>(fieldToken.lexeme, params, body, false, false,
                                                  getLocation(fieldToken)));
                } else {
                    expect(TokenType::SEMICOLON, "Expected ';' after field declaration");
                }
            }
        } else {
            advance();
        }
    }
    
    expect(TokenType::RIGHT_BRACE, "Expected '}' after class body");
    
    return std::make_shared<ClassDecl>(name, superClass, members, getLocation(nameToken));
}

DeclPtr Parser::parseModuleDeclaration() {
    Token nameToken = expect(TokenType::IDENTIFIER, "Expected module name");
    
    expect(TokenType::LEFT_BRACE, "Expected '{' before module body");
    
    auto body = parseBlock();
    
    expect(TokenType::RIGHT_BRACE, "Expected '}' after module body");
    
    return std::make_shared<ModuleDecl>(nameToken.lexeme, body, getLocation(nameToken));
}

DeclPtr Parser::parseImportDeclaration() {
    std::string defaultImport;
    std::vector<ImportDecl::ImportSpecifier> specifiers;
    std::string namespaceAlias;
    
    if (check(TokenType::IDENTIFIER)) {
        defaultImport = advance().lexeme;
        
        if (match(TokenType::COMMA)) {
            // 还有命名导入
        } else if (match(TokenType::FROM)) {
            // 只有默认导入
        }
    }
    
    if (match(TokenType::LEFT_BRACE)) {
        while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
            std::string importedName;
            std::string localName;
            
            if (check(TokenType::IDENTIFIER)) {
                importedName = advance().lexeme;
                localName = importedName;
                
                if (match(TokenType::AS)) {
                    Token localToken = expect(TokenType::IDENTIFIER, "Expected alias name");
                    localName = localToken.lexeme;
                }
            }
            
            specifiers.push_back({importedName, localName});
            
            if (!match(TokenType::COMMA)) break;
        }
        
        expect(TokenType::RIGHT_BRACE, "Expected '}' after import specifiers");
    }
    
    if (match(TokenType::STAR)) {
        expect(TokenType::AS, "Expected 'as' after '*' in import");
        Token aliasToken = expect(TokenType::IDENTIFIER, "Expected alias name");
        namespaceAlias = aliasToken.lexeme;
    }
    
    std::string modulePath;
    if (match(TokenType::FROM)) {
        Token pathToken = expect(TokenType::STRING, "Expected module path");
        modulePath = pathToken.getString();
    }
    
    expect(TokenType::SEMICOLON, "Expected ';' after import declaration");
    
    return std::make_shared<ImportDecl>(
        modulePath, namespaceAlias, specifiers, defaultImport, 
        SourceLocation(filename_, 0, 0, 0));
}

DeclPtr Parser::parseExportDeclaration() {
    bool isDefault = match(TokenType::DEFAULT_);
    
    DeclPtr declaration;
    std::string name;
    
    if (match(TokenType::FUNCTION)) {
        declaration = parseFunctionDeclaration();
        if (auto funcDecl = std::dynamic_pointer_cast<FunctionDecl>(declaration)) {
            name = funcDecl->name;
        }
    } else if (match(TokenType::CLASS)) {
        declaration = parseClassDeclaration();
        if (auto classDecl = std::dynamic_pointer_cast<ClassDecl>(declaration)) {
            name = classDecl->name;
        }
    } else if (match(TokenType::LEFT_BRACE)) {
        while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
            if (check(TokenType::IDENTIFIER)) {
                name = advance().lexeme;
            }
            if (!match(TokenType::COMMA)) break;
        }
        expect(TokenType::RIGHT_BRACE, "Expected '}' after export specifiers");
        
        if (match(TokenType::FROM)) {
            expect(TokenType::STRING, "Expected module path");
        }
    } else if (isDefault) {
        auto expr = parseExpression();
        name = "default";
    }
    
    expect(TokenType::SEMICOLON, "Expected ';' after export declaration");
    
    return std::make_shared<ExportDecl>(declaration, name, isDefault, 
                                        SourceLocation(filename_, 0, 0, 0));
}

// ==================== 解析语句 ====================

StmtPtr Parser::parseStatement() {
    if (check(TokenType::LEFT_BRACE)) {
        BlockStmt* block = parseBlock();
        return std::static_pointer_cast<Statement>(std::make_shared<BlockStmt>(*block));
    }

    if (match(TokenType::IF)) {
        return parseIfStatement();
    }

    if (match(TokenType::FOR)) {
        if (match(TokenType::EACH)) {
            return parseForEachStatement();
        }
        return parseForStatement();
    }

    if (match(TokenType::WHILE)) {
        return parseWhileStatement();
    }

    if (match(TokenType::RETURN)) {
        return parseReturnStatement();
    }

    if (match(TokenType::BREAK)) {
        return parseBreakStatement();
    }

    if (match(TokenType::CONTINUE)) {
        return parseContinueStatement();
    }

    if (match(TokenType::THROW)) {
        return parseThrowStatement();
    }

    if (match(TokenType::TRY)) {
        return parseTryStatement();
    }

    if (match(TokenType::SWITCH)) {
        return parseSwitchStatement();
    }

    if (match(TokenType::AUTO) || match(TokenType::CONST)) {
        bool isConst = previous().type == TokenType::CONST;
        Token nameToken = expect(TokenType::IDENTIFIER, "Expected variable name");
        
        ExprPtr initializer;
        if (match(TokenType::EQUAL)) {
            initializer = parseExpression();
        }
        
        expect(TokenType::SEMICOLON, "Expected ';' after variable declaration");
        
        return std::make_shared<VariableDecl>(nameToken.lexeme, initializer, isConst, 
                                              getLocation(nameToken));
    }

    return parseExpressionStatement();
}

BlockStmt* Parser::parseBlock() {
    expect(TokenType::LEFT_BRACE, "Expected '{' before block");
    
    std::vector<StmtPtr> statements;
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) {
            statements.push_back(stmt);
        }
    }
    
    expect(TokenType::RIGHT_BRACE, "Expected '}' after block");
    
    return new BlockStmt(statements, SourceLocation(filename_, 0, 0, 0));
}

StmtPtr Parser::parseExpressionStatement() {
    auto expr = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';' after expression");
    return std::make_shared<ExpressionStmt>(expr, expr->location);
}

StmtPtr Parser::parseIfStatement() {
    expect(TokenType::LEFT_PAREN, "Expected '(' after 'if'");
    auto condition = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after condition");
    
    auto thenBranch = parseStatement();
    StmtPtr elseBranch;
    
    if (match(TokenType::ELSE)) {
        elseBranch = parseStatement();
    }
    
    return std::make_shared<IfStmt>(condition, thenBranch, elseBranch, condition->location);
}

StmtPtr Parser::parseForStatement() {
    expect(TokenType::LEFT_PAREN, "Expected '(' after 'for'");

    StmtPtr initializer;
    if (match(TokenType::AUTO) || match(TokenType::CONST)) {
        bool isConst = previous().type == TokenType::CONST;
        Token nameToken = expect(TokenType::IDENTIFIER, "Expected variable name");

        ExprPtr initExpr;
        if (match(TokenType::EQUAL)) {
            initExpr = parseExpression();
        }

        initializer = std::make_shared<VariableDecl>(nameToken.lexeme, initExpr, isConst,
                                                     getLocation(nameToken));
    } else if (!check(TokenType::SEMICOLON)) {
        initializer = std::make_shared<ExpressionStmt>(parseExpression(), SourceLocation(filename_, 0, 0, 0));
    }

    expect(TokenType::SEMICOLON, "Expected ';' after initializer");

    ExprPtr condition;
    if (!check(TokenType::SEMICOLON)) {
        condition = parseExpression();
    }

    expect(TokenType::SEMICOLON, "Expected ';' after condition");

    ExprPtr increment;
    if (!check(TokenType::RIGHT_PAREN)) {
        increment = parseExpression();
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after for clauses");

    auto body = parseStatement();

    return std::make_shared<ForStmt>(initializer, condition, increment, body,
                                     SourceLocation(filename_, 0, 0, 0));
}

StmtPtr Parser::parseForEachStatement() {
    Token varToken = expect(TokenType::IDENTIFIER, "Expected variable name in for-each");
    std::string variable = varToken.lexeme;

    expect(TokenType::IN, "Expected 'in' after for-each variable");

    auto iterable = parseExpression();

    expect(TokenType::LEFT_BRACE, "Expected '{' before for-each body");
    auto body = parseBlock();
    expect(TokenType::RIGHT_BRACE, "Expected '}' after for-each body");

    return std::make_shared<ForEachStmt>(variable, iterable, body, getLocation(varToken));
}

StmtPtr Parser::parseWhileStatement() {
    expect(TokenType::LEFT_PAREN, "Expected '(' after 'while'");
    auto condition = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after condition");
    
    auto body = parseStatement();
    
    return std::make_shared<WhileStmt>(condition, body, condition->location);
}

StmtPtr Parser::parseReturnStatement() {
    Token token = previous();
    
    ExprPtr value;
    if (!check(TokenType::SEMICOLON) && !check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        value = parseExpression();
    }
    
    expect(TokenType::SEMICOLON, "Expected ';' after return statement");
    
    return std::make_shared<ReturnStmt>(value, getLocation(token));
}

StmtPtr Parser::parseBreakStatement() {
    Token token = previous();
    expect(TokenType::SEMICOLON, "Expected ';' after 'break'");
    return std::make_shared<BreakStmt>(getLocation(token));
}

StmtPtr Parser::parseContinueStatement() {
    Token token = previous();
    expect(TokenType::SEMICOLON, "Expected ';' after 'continue'");
    return std::make_shared<ContinueStmt>(getLocation(token));
}

StmtPtr Parser::parseThrowStatement() {
    Token token = previous();
    auto exception = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';' after throw statement");
    return std::make_shared<ThrowStmt>(exception, getLocation(token));
}

StmtPtr Parser::parseTryStatement() {
    Token token = previous();
    auto tryBlock = parseBlock();
    
    std::string catchVariable;
    BlockStmt* catchBlock = nullptr;
    BlockStmt* finallyBlock = nullptr;
    
    if (match(TokenType::CATCH)) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after 'catch'");
        Token varToken = expect(TokenType::IDENTIFIER, "Expected catch variable name");
        catchVariable = varToken.lexeme;
        expect(TokenType::RIGHT_PAREN, "Expected ')' after catch variable");
        
        catchBlock = parseBlock();
    }
    
    if (match(TokenType::FINALLY)) {
        finallyBlock = parseBlock();
    }
    
    return std::make_shared<TryStmt>(tryBlock, catchVariable, catchBlock, finallyBlock,
                                     getLocation(token));
}

StmtPtr Parser::parseSwitchStatement() {
    Token token = previous();
    
    expect(TokenType::LEFT_PAREN, "Expected '(' after 'switch'");
    auto discriminant = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after switch expression");
    
    expect(TokenType::LEFT_BRACE, "Expected '{' before switch body");
    
    std::vector<SwitchStmt::Case> cases;
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        SwitchStmt::Case case_;
        
        if (match(TokenType::CASE)) {
            case_.value = parseExpression();
            case_.isDefault = false;
            expect(TokenType::COLON, "Expected ':' after case value");
        } else if (match(TokenType::DEFAULT_)) {
            case_.isDefault = true;
            expect(TokenType::COLON, "Expected ':' after 'default'");
        } else {
            break;
        }
        
        while (!check(TokenType::CASE) && !check(TokenType::DEFAULT_) && 
               !check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
            auto stmt = parseStatement();
            if (stmt) {
                case_.statements.push_back(stmt);
            }
        }
        
        cases.push_back(case_);
    }
    
    expect(TokenType::RIGHT_BRACE, "Expected '}' after switch body");
    
    return std::make_shared<SwitchStmt>(discriminant, cases, getLocation(token));
}

// ==================== 解析表达式 ====================

ExprPtr Parser::parseExpression() {
    return parseAssignment();
}

ExprPtr Parser::parseAssignment() {
    auto expr = parseTernary();
    
    if (match(TokenType::EQUAL)) {
        if (auto ident = std::dynamic_pointer_cast<IdentifierExpr>(expr)) {
            Token token = peek();
            auto value = parseAssignment();
            return std::make_shared<AssignExpr>(ident->name, value, ident->location);
        } else {
            error(previous(), "Invalid assignment target");
        }
    }
    
    return expr;
}

ExprPtr Parser::parseTernary() {
    auto condition = parseLogicalOr();
    
    if (match(TokenType::QUESTION)) {
        auto thenBranch = parseExpression();
        expect(TokenType::COLON, "Expected ':' in ternary expression");
        auto elseBranch = parseExpression();
        return std::make_shared<TernaryExpr>(condition, thenBranch, elseBranch, condition->location);
    }
    
    return condition;
}

ExprPtr Parser::parseLogicalOr() {
    auto expr = parseLogicalAnd();
    
    while (match(TokenType::PIPE_PIPE)) {
        Token op = previous();
        auto right = parseLogicalAnd();
        expr = std::make_shared<BinaryExpr>(expr, op.type, right, getLocation(op));
    }
    
    return expr;
}

ExprPtr Parser::parseLogicalAnd() {
    auto expr = parseBitwiseOr();
    
    while (match(TokenType::AMPERSAND_AMPERSAND)) {
        Token op = previous();
        auto right = parseBitwiseOr();
        expr = std::make_shared<BinaryExpr>(expr, op.type, right, getLocation(op));
    }
    
    return expr;
}

ExprPtr Parser::parseBitwiseOr() {
    auto expr = parseBitwiseXor();
    
    while (match(TokenType::PIPE)) {
        Token op = previous();
        auto right = parseBitwiseXor();
        expr = std::make_shared<BinaryExpr>(expr, op.type, right, getLocation(op));
    }
    
    return expr;
}

ExprPtr Parser::parseBitwiseXor() {
    auto expr = parseBitwiseAnd();
    
    while (match(TokenType::CARET)) {
        Token op = previous();
        auto right = parseBitwiseAnd();
        expr = std::make_shared<BinaryExpr>(expr, op.type, right, getLocation(op));
    }
    
    return expr;
}

ExprPtr Parser::parseBitwiseAnd() {
    auto expr = parseEquality();
    
    while (match(TokenType::AMPERSAND)) {
        Token op = previous();
        auto right = parseEquality();
        expr = std::make_shared<BinaryExpr>(expr, op.type, right, getLocation(op));
    }
    
    return expr;
}

ExprPtr Parser::parseEquality() {
    auto expr = parseComparison();
    
    while (match({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL, 
                  TokenType::EQUAL_EQUAL_EQUAL, TokenType::BANG_EQUAL_EQUAL})) {
        Token op = previous();
        auto right = parseComparison();
        expr = std::make_shared<BinaryExpr>(expr, op.type, right, getLocation(op));
    }
    
    return expr;
}

ExprPtr Parser::parseComparison() {
    auto expr = parseShift();
    
    while (match({TokenType::LESS, TokenType::GREATER, 
                  TokenType::LESS_EQUAL, TokenType::GREATER_EQUAL})) {
        Token op = previous();
        auto right = parseShift();
        expr = std::make_shared<BinaryExpr>(expr, op.type, right, getLocation(op));
    }
    
    return expr;
}

ExprPtr Parser::parseShift() {
    auto expr = parseAdditive();
    
    while (match({TokenType::LESS_LESS, TokenType::GREATER_GREATER})) {
        Token op = previous();
        auto right = parseAdditive();
        expr = std::make_shared<BinaryExpr>(expr, op.type, right, getLocation(op));
    }
    
    return expr;
}

ExprPtr Parser::parseAdditive() {
    auto expr = parseMultiplicative();
    
    while (match({TokenType::PLUS, TokenType::MINUS})) {
        Token op = previous();
        auto right = parseMultiplicative();
        expr = std::make_shared<BinaryExpr>(expr, op.type, right, getLocation(op));
    }
    
    return expr;
}

ExprPtr Parser::parseMultiplicative() {
    auto expr = parseExponentiation();
    
    while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        Token op = previous();
        auto right = parseExponentiation();
        expr = std::make_shared<BinaryExpr>(expr, op.type, right, getLocation(op));
    }
    
    return expr;
}

ExprPtr Parser::parseExponentiation() {
    auto expr = parseUnary();
    
    if (match(TokenType::STAR_STAR)) {
        Token op = previous();
        auto right = parseUnary();
        expr = std::make_shared<BinaryExpr>(expr, op.type, right, getLocation(op));
    }
    
    return expr;
}

ExprPtr Parser::parseUnary() {
    if (match({TokenType::BANG, TokenType::MINUS, TokenType::TILDE,
               TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
        Token op = previous();
        auto operand = parseUnary();
        return std::make_shared<UnaryExpr>(op.type, operand, getLocation(op));
    }
    
    return parseCall();
}

ExprPtr Parser::parseCall() {
    auto expr = parsePrimary();
    
    while (true) {
        if (match(TokenType::LEFT_PAREN)) {
            auto args = parseArguments();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after arguments");
            expr = std::make_shared<CallExpr>(expr, args, expr->location);
        } else if (match(TokenType::DOT)) {
            Token nameToken = expect(TokenType::IDENTIFIER, "Expected property name after '.'");
            expr = std::make_shared<MemberExpr>(expr, nameToken.lexeme, getLocation(nameToken));
        } else if (match(TokenType::LEFT_BRACKET)) {
            auto index = parseExpression();
            expect(TokenType::RIGHT_BRACKET, "Expected ']' after index");
            expr = std::make_shared<IndexExpr>(expr, index, expr->location);
        } else {
            break;
        }
    }
    
    return expr;
}

ExprPtr Parser::parsePrimary() {
    if (match(TokenType::TRUE)) {
        return std::make_shared<LiteralExpr>(true, TokenType::TRUE, 
                                             SourceLocation(filename_, 0, 0, 0));
    }
    
    if (match(TokenType::FALSE)) {
        return std::make_shared<LiteralExpr>(false, TokenType::FALSE,
                                             SourceLocation(filename_, 0, 0, 0));
    }
    
    if (match(TokenType::NULL_)) {
        return std::make_shared<LiteralExpr>(std::monostate{}, TokenType::NULL_,
                                             SourceLocation(filename_, 0, 0, 0));
    }
    
    if (match(TokenType::UNDEFINED)) {
        return std::make_shared<LiteralExpr>(std::monostate{}, TokenType::UNDEFINED,
                                             SourceLocation(filename_, 0, 0, 0));
    }
    
    if (match(TokenType::NUMBER)) {
        Token token = previous();
        return std::make_shared<LiteralExpr>(token.getNumber(), TokenType::NUMBER,
                                            getLocation(token));
    }
    
    if (match(TokenType::STRING)) {
        Token token = previous();
        return std::make_shared<LiteralExpr>(token.getString(), TokenType::STRING,
                                            getLocation(token));
    }
    
    if (match(TokenType::IDENTIFIER)) {
        Token token = previous();
        return std::make_shared<IdentifierExpr>(token.lexeme, getLocation(token));
    }
    
    if (match(TokenType::LEFT_PAREN)) {
        auto expr = parseExpression();
        expect(TokenType::RIGHT_PAREN, "Expected ')' after expression");
        return expr;
    }
    
    if (match(TokenType::LEFT_BRACKET)) {
        return parseArrayLiteral();
    }
    
    if (match(TokenType::LEFT_BRACE)) {
        return parseObjectLiteral();
    }
    
    if (match(TokenType::FUNCTION)) {
        return parseFunctionExpression(false);
    }
    
    if (match(TokenType::ASYNC)) {
        return parseFunctionExpression(true);
    }
    
    if (match(TokenType::NEW)) {
        auto callee = parsePrimary();
        
        std::vector<ExprPtr> args;
        if (match(TokenType::LEFT_PAREN)) {
            args = parseArguments();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after arguments");
        }
        
        return std::make_shared<CallExpr>(callee, args, callee->location);
    }
    
    if (match(TokenType::THIS)) {
        return std::make_shared<IdentifierExpr>("this", SourceLocation(filename_, 0, 0, 0));
    }
    
    if (match(TokenType::SUPER)) {
        return std::make_shared<IdentifierExpr>("super", SourceLocation(filename_, 0, 0, 0));
    }
    
    // 箭头函数简写
    if (check(TokenType::IDENTIFIER)) {
        Token paramToken = peek();
        size_t savedCurrent = current_;
        advance();
        
        if (match(TokenType::ARROW)) {
            current_ = savedCurrent;
            
            std::vector<std::string> params;
            if (check(TokenType::IDENTIFIER)) {
                params.push_back(advance().lexeme);
            }
            
            expect(TokenType::ARROW, "Expected '=>' in arrow function");
            
            if (check(TokenType::LEFT_BRACE)) {
                auto body = parseBlock();
                return std::make_shared<ArrowFunctionExpr>(params, nullptr, body->statements, false,
                                                          getLocation(paramToken));
            } else {
                auto exprBody = parseExpression();
                return std::make_shared<ArrowFunctionExpr>(params, exprBody, 
                                                           std::vector<StmtPtr>{}, false,
                                                           getLocation(paramToken));
            }
        }
        
        current_ = savedCurrent;
    }
    
    Token token = peek();
    error(token, "Expected expression");
    advance();
    return std::make_shared<LiteralExpr>(std::monostate{}, TokenType::NULL_,
                                        getLocation(token));
}

ExprPtr Parser::parseFunctionExpression(bool isAsync) {
    Token nameToken;
    std::string name;
    
    if (check(TokenType::IDENTIFIER)) {
        nameToken = advance();
        name = nameToken.lexeme;
    }
    
    expect(TokenType::LEFT_PAREN, "Expected '(' after function name");
    
    std::vector<std::string> params;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            if (check(TokenType::IDENTIFIER)) {
                params.push_back(advance().lexeme);
            } else if (match(TokenType::DOT_DOT_DOT)) {
                if (check(TokenType::IDENTIFIER)) {
                    params.push_back("..." + advance().lexeme);
                }
            } else {
                error(peek(), "Expected parameter name");
                break;
            }
        } while (match(TokenType::COMMA));
    }
    
    expect(TokenType::RIGHT_PAREN, "Expected ')' after parameters");
    
    expect(TokenType::LEFT_BRACE, "Expected '{' before function body");
    
    std::vector<StmtPtr> body;
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) {
            body.push_back(stmt);
        }
    }
    
    expect(TokenType::RIGHT_BRACE, "Expected '}' after function body");
    
    return std::make_shared<FunctionExpr>(name, params, body, isAsync, 
                                          SourceLocation(filename_, 0, 0, 0));
}

ExprPtr Parser::parseArrowFunction(std::vector<std::string> params, bool isAsync) {
    expect(TokenType::ARROW, "Expected '=>' in arrow function");
    
    if (check(TokenType::LEFT_BRACE)) {
        auto body = parseBlock();
        return std::make_shared<ArrowFunctionExpr>(params, nullptr, body->statements, isAsync,
                                                   SourceLocation(filename_, 0, 0, 0));
    } else {
        auto exprBody = parseExpression();
        return std::make_shared<ArrowFunctionExpr>(params, exprBody, 
                                                   std::vector<StmtPtr>{}, isAsync,
                                                   SourceLocation(filename_, 0, 0, 0));
    }
}

ExprPtr Parser::parseArrayLiteral() {
    Token token = previous();
    std::vector<ExprPtr> elements;
    
    if (!check(TokenType::RIGHT_BRACKET)) {
        do {
            if (match(TokenType::DOT_DOT_DOT)) {
                auto expr = parseExpression();
                elements.push_back(expr);
            } else {
                elements.push_back(parseExpression());
            }
        } while (match(TokenType::COMMA));
    }
    
    expect(TokenType::RIGHT_BRACKET, "Expected ']' after array elements");
    
    return std::make_shared<ArrayExpr>(elements, getLocation(token));
}

ExprPtr Parser::parseObjectLiteral() {
    Token token = previous();
    std::vector<ObjectExpr::Property> properties;
    
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        ObjectExpr::Property prop;
        prop.isMethod = false;
        prop.isShorthand = false;
        
        if (match(TokenType::FUNCTION)) {
            prop.isMethod = true;
            Token nameToken = expect(TokenType::IDENTIFIER, "Expected method name");
            prop.key = nameToken.lexeme;
            
            expect(TokenType::LEFT_PAREN, "Expected '(' after method name");
            
            std::vector<std::string> params;
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    if (check(TokenType::IDENTIFIER)) {
                        params.push_back(advance().lexeme);
                    }
                } while (match(TokenType::COMMA));
            }
            
            expect(TokenType::RIGHT_PAREN, "Expected ')' after method parameters");
            
            auto body = parseBlock();
            
            prop.value = std::make_shared<FunctionExpr>("", params, body->statements, false,
                                                        SourceLocation(filename_, 0, 0, 0));
        } else if (match(TokenType::GET) || match(TokenType::SET)) {
            TokenType type = previous().type;
            Token nameToken = expect(TokenType::IDENTIFIER, "Expected property name");
            prop.key = nameToken.lexeme;
            
            expect(TokenType::LEFT_PAREN, "Expected '(' after getter/setter name");
            expect(TokenType::RIGHT_PAREN, "Expected ')' after getter/setter parameters");

            auto body = parseBlock();
            
            std::vector<std::string> params;
            prop.value = std::make_shared<FunctionExpr>("", params, body->statements, false,
                                                        SourceLocation(filename_, 0, 0, 0));
            prop.isMethod = true;
        } else {
            Token nameToken = expect(TokenType::IDENTIFIER, "Expected property name");
            prop.key = nameToken.lexeme;
            
            if (match(TokenType::COLON)) {
                prop.value = parseExpression();
            } else {
                prop.isShorthand = true;
                prop.value = std::make_shared<IdentifierExpr>(prop.key, getLocation(nameToken));
            }
        }
        
        properties.push_back(prop);
        
        if (!match(TokenType::COMMA)) break;
    }
    
    expect(TokenType::RIGHT_BRACE, "Expected '}' after object properties");
    
    return std::make_shared<ObjectExpr>(properties, getLocation(token));
}

std::vector<ExprPtr> Parser::parseArguments() {
    std::vector<ExprPtr> arguments;
    
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            arguments.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }
    
    return arguments;
}

} // namespace zcc

#include <iostream>
#include <cassert>
#include "Lexer.hpp"
#include "Compiler.hpp"

using namespace zcc;

// 测试辅助宏
#define TEST(name) void name()
#define RUN_TEST(name) do { \
    std::cout << "Running " << #name << "... "; \
    name(); \
    std::cout << "PASSED" << std::endl; \
} while(0)

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        std::cerr << "FAILED: " << #expected << " != " << #actual << std::endl; \
        std::exit(1); \
    } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAILED: " << #cond << " is false" << std::endl; \
        std::exit(1); \
    } \
} while(0)

// ===== 词法分析器测试 =====

TEST(testLexer_numbers) {
    Lexer lexer("123 45.67 0.5 1e10 1.5E-3", "test");
    auto tokens = lexer.tokenize();
    
    // 应该有 5 个数字 token + 1 个 EOF
    ASSERT_EQ(6, tokens.size());
    ASSERT_EQ(TokenType::NUMBER, tokens[0].type);
    ASSERT_EQ(TokenType::NUMBER, tokens[1].type);
    ASSERT_EQ(TokenType::NUMBER, tokens[2].type);
    ASSERT_EQ(TokenType::NUMBER, tokens[3].type);
    ASSERT_EQ(TokenType::NUMBER, tokens[4].type);
}

TEST(testLexer_strings) {
    Lexer lexer(R"("hello" 'world' `template`)", "test");
    auto tokens = lexer.tokenize();
    
    // 3 个字符串 + EOF
    ASSERT_EQ(4, tokens.size());
    ASSERT_EQ(TokenType::STRING, tokens[0].type);
    ASSERT_EQ(TokenType::STRING, tokens[1].type);
    ASSERT_EQ(TokenType::STRING, tokens[2].type);
}

TEST(testLexer_identifiers) {
    Lexer lexer("foo bar 测试 中文变量 _private $special", "test");
    auto tokens = lexer.tokenize();
    
    // 6 个标识符 + EOF
    ASSERT_EQ(7, tokens.size());
    for (int i = 0; i < 6; ++i) {
        ASSERT_EQ(TokenType::IDENTIFIER, tokens[i].type);
    }
}

TEST(testLexer_keywords_cn) {
    Lexer lexer("定义 常量 函数 主函数 返回 如果 否则 循环 当 中断 继续", "test");
    auto tokens = lexer.tokenize();
    
    // 11 个关键词 + EOF
    ASSERT_EQ(12, tokens.size());
    ASSERT_EQ(TokenType::AUTO, tokens[0].type);
    ASSERT_EQ(TokenType::CONST, tokens[1].type);
    ASSERT_EQ(TokenType::FUNCTION, tokens[2].type);
    ASSERT_EQ(TokenType::MAIN, tokens[3].type);
    ASSERT_EQ(TokenType::RETURN, tokens[4].type);
    ASSERT_EQ(TokenType::IF, tokens[5].type);
    ASSERT_EQ(TokenType::ELSE, tokens[6].type);
    ASSERT_EQ(TokenType::FOR, tokens[7].type);
    ASSERT_EQ(TokenType::WHILE, tokens[8].type);
    ASSERT_EQ(TokenType::BREAK, tokens[9].type);
    ASSERT_EQ(TokenType::CONTINUE, tokens[10].type);
}

TEST(testLexer_keywords_en) {
    Lexer lexer("auto const function main return if else for while break continue", "test");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(12, tokens.size());
    ASSERT_EQ(TokenType::AUTO, tokens[0].type);
    ASSERT_EQ(TokenType::CONST, tokens[1].type);
    ASSERT_EQ(TokenType::FUNCTION, tokens[2].type);
    ASSERT_EQ(TokenType::MAIN, tokens[3].type);
    ASSERT_EQ(TokenType::RETURN, tokens[4].type);
    ASSERT_EQ(TokenType::IF, tokens[5].type);
    ASSERT_EQ(TokenType::ELSE, tokens[6].type);
    ASSERT_EQ(TokenType::FOR, tokens[7].type);
    ASSERT_EQ(TokenType::WHILE, tokens[8].type);
    ASSERT_EQ(TokenType::BREAK, tokens[9].type);
    ASSERT_EQ(TokenType::CONTINUE, tokens[10].type);
}

TEST(testLexer_operators) {
    Lexer lexer("+ - * / % ** ++ -- = += -= *= /= %=", "test");
    auto tokens = lexer.tokenize();
    
    // 14 个运算符 + EOF
    ASSERT_EQ(15, tokens.size());
    ASSERT_EQ(TokenType::PLUS, tokens[0].type);
    ASSERT_EQ(TokenType::MINUS, tokens[1].type);
    ASSERT_EQ(TokenType::STAR, tokens[2].type);
    ASSERT_EQ(TokenType::SLASH, tokens[3].type);
    ASSERT_EQ(TokenType::PERCENT, tokens[4].type);
    ASSERT_EQ(TokenType::STAR_STAR, tokens[5].type);
    ASSERT_EQ(TokenType::PLUS_PLUS, tokens[6].type);
    ASSERT_EQ(TokenType::MINUS_MINUS, tokens[7].type);
    ASSERT_EQ(TokenType::EQUAL, tokens[8].type);
    ASSERT_EQ(TokenType::PLUS_EQUAL, tokens[9].type);
    ASSERT_EQ(TokenType::MINUS_EQUAL, tokens[10].type);
    ASSERT_EQ(TokenType::STAR_EQUAL, tokens[11].type);
    ASSERT_EQ(TokenType::SLASH_EQUAL, tokens[12].type);
    ASSERT_EQ(TokenType::PERCENT_EQUAL, tokens[13].type);
}

TEST(testLexer_comparators) {
    Lexer lexer("== != === !== < > <= >=", "test");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(9, tokens.size());
    ASSERT_EQ(TokenType::EQUAL_EQUAL, tokens[0].type);
    ASSERT_EQ(TokenType::BANG_EQUAL, tokens[1].type);
    ASSERT_EQ(TokenType::EQUAL_EQUAL_EQUAL, tokens[2].type);
    ASSERT_EQ(TokenType::BANG_EQUAL_EQUAL, tokens[3].type);
    ASSERT_EQ(TokenType::LESS, tokens[4].type);
    ASSERT_EQ(TokenType::GREATER, tokens[5].type);
    ASSERT_EQ(TokenType::LESS_EQUAL, tokens[6].type);
    ASSERT_EQ(TokenType::GREATER_EQUAL, tokens[7].type);
}

TEST(testLexer_logical) {
    Lexer lexer("! && ||", "test");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(4, tokens.size());
    ASSERT_EQ(TokenType::BANG, tokens[0].type);
    ASSERT_EQ(TokenType::AMPERSAND_AMPERSAND, tokens[1].type);
    ASSERT_EQ(TokenType::PIPE_PIPE, tokens[2].type);
}

TEST(testLexer_comments) {
    Lexer lexer("// 单行注释\n/* 多行\n注释 */", "test");
    auto tokens = lexer.tokenize();
    
    // 只有 EOF (注释被过滤)
    ASSERT_EQ(1, tokens.size());
    ASSERT_EQ(TokenType::EOF_, tokens[0].type);
}

TEST(testLexer_literals) {
    Lexer lexer("是 否 空 未定义 true false null undefined", "test");
    auto tokens = lexer.tokenize();
    
    ASSERT_EQ(9, tokens.size());
    ASSERT_EQ(TokenType::TRUE, tokens[0].type);
    ASSERT_EQ(TokenType::FALSE, tokens[1].type);
    ASSERT_EQ(TokenType::NULL_, tokens[2].type);
    ASSERT_EQ(TokenType::UNDEFINED, tokens[3].type);
    ASSERT_EQ(TokenType::TRUE, tokens[4].type);
    ASSERT_EQ(TokenType::FALSE, tokens[5].type);
    ASSERT_EQ(TokenType::NULL_, tokens[6].type);
    ASSERT_EQ(TokenType::UNDEFINED, tokens[7].type);
}

// ===== 编译器测试 =====

TEST(testCompiler_hello) {
    const char* source = R"(
函数 主函数 () {
    打印行 ("你好，世界!");
    返回 0;
}
)";
    
    Compiler compiler;
    auto result = compiler.compile(source, "hello.zcc");
    
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(!result.binaryData.empty());
}

TEST(testCompiler_variables) {
    const char* source = R"(
函数 主函数 () {
    定义 x = 100;
    定义 y = 3.14;
    定义 名字 = "ZCC";
    返回 0;
}
)";
    
    Compiler compiler;
    auto result = compiler.compile(source, "variables.zcc");
    
    ASSERT_TRUE(result.success);
}

TEST(testCompiler_arithmetic) {
    const char* source = R"(
函数 主函数 () {
    定义 结果 = 1 + 2 * 3;
    定义 幂 = 2 ** 10;
    返回 结果 + 幂;
}
)";
    
    Compiler compiler;
    auto result = compiler.compile(source, "arithmetic.zcc");
    
    ASSERT_TRUE(result.success);
}

TEST(testCompiler_control) {
    const char* source = R"(
函数 主函数 () {
    定义 x = 10;
    
    如果 (x > 5) {
        打印行 ("大于 5");
    } 否则 {
        打印行 ("小于等于 5");
    }
    
    循环 (定义 i = 0; i < 10; i = i + 1) {
        打印行 (i);
    }
    
    返回 0;
}
)";
    
    Compiler compiler;
    auto result = compiler.compile(source, "control.zcc");
    
    ASSERT_TRUE(result.success);
}

TEST(testCompiler_functions) {
    const char* source = R"(
函数 加法 (a, b) {
    返回 a + b;
}

函数 主函数 () {
    定义 结果 = 加法 (3, 5);
    打印行 (结果);
    返回 0;
}
)";
    
    Compiler compiler;
    auto result = compiler.compile(source, "functions.zcc");
    
    ASSERT_TRUE(result.success);
}

TEST(testCompiler_arrays) {
    const char* source = R"(
函数 主函数 () {
    定义 数组 = [1, 2, 3, 4, 5];
    打印行 (数组 [0]);
    返回 0;
}
)";
    
    Compiler compiler;
    auto result = compiler.compile(source, "arrays.zcc");
    
    ASSERT_TRUE(result.success);
}

TEST(testCompiler_objects) {
    const char* source = R"(
函数 主函数 () {
    定义 用户 = {
        姓名："张三",
        年龄：25
    };
    打印行 (用户。姓名);
    返回 0;
}
)";
    
    Compiler compiler;
    auto result = compiler.compile(source, "objects.zcc");
    
    ASSERT_TRUE(result.success);
}

int main() {
    std::cout << "=== ZCC Compiler Tests ===\n\n";
    
    std::cout << "--- Lexer Tests ---\n";
    RUN_TEST(testLexer_numbers);
    RUN_TEST(testLexer_strings);
    RUN_TEST(testLexer_identifiers);
    RUN_TEST(testLexer_keywords_cn);
    RUN_TEST(testLexer_keywords_en);
    RUN_TEST(testLexer_operators);
    RUN_TEST(testLexer_comparators);
    RUN_TEST(testLexer_logical);
    RUN_TEST(testLexer_comments);
    RUN_TEST(testLexer_literals);
    
    std::cout << "\n--- Compiler Tests ---\n";
    RUN_TEST(testCompiler_hello);
    RUN_TEST(testCompiler_variables);
    RUN_TEST(testCompiler_arithmetic);
    RUN_TEST(testCompiler_control);
    RUN_TEST(testCompiler_functions);
    RUN_TEST(testCompiler_arrays);
    RUN_TEST(testCompiler_objects);
    
    std::cout << "\n=== All Tests Passed! ===\n";
    return 0;
}

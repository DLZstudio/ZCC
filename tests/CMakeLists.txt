cmake_minimum_required(VERSION 3.16)
project(ZCCTests)

set(CMAKE_CXX_STANDARD 17)

# 查找测试框架 (这里使用简单的测试，不依赖外部框架)
add_executable(zcc_test
    test_lexer.cpp
)

target_include_directories(zcc_test PRIVATE ../include)
target_link_libraries(zcc_test PRIVATE zcc)

add_test(NAME LexerTest COMMAND zcc_test)

cmake_minimum_required(VERSION 3.16)
project(ZCCCompiler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 选项
option(ZCC_BUILD_TESTS "Build tests" ON)
option(ZCC_BUILD_EXAMPLES "Build examples" ON)

# 源代码文件
set(ZCC_SOURCES
    src/main.cpp
    src/Compiler.cpp
    src/Lexer.cpp
    src/Token.cpp
    src/Parser.cpp
    src/AST.cpp
    src/SemanticAnalyzer.cpp
    src/CodeGenerator.cpp
    src/Bytecode.cpp
    src/SymbolTable.cpp
    src/Diagnostics.cpp
)

set(ZCC_HEADERS
    include/Compiler.hpp
    include/Lexer.hpp
    include/Token.hpp
    include/Parser.hpp
    include/AST.hpp
    include/SemanticAnalyzer.hpp
    include/CodeGenerator.hpp
    include/Bytecode.hpp
    include/SymbolTable.hpp
    include/Diagnostics.hpp
    include/Keywords.hpp
)

# 创建可执行文件
add_executable(zcc ${ZCC_SOURCES} ${ZCC_HEADERS})
target_include_directories(zcc PUBLIC include)

# 平台特定设置
if(WIN32)
    target_compile_definitions(zcc PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# 测试
if(ZCC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 示例
if(ZCC_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# 安装规则
install(TARGETS zcc DESTINATION bin)

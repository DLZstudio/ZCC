cmake_minimum_required(VERSION 3.20)
project(ZCCCompiler VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译选项
option(ZCC_BUILD_TESTS "Build tests" ON)
option(ZCC_BUILD_EXAMPLES "Build examples" ON)
option(ZCC_ENABLE_LLVM "Enable LLVM backend" OFF)
option(ZCC_ENABLE_CACHE "Enable cache system" ON)

# 查找 LLVM (可选)
if(ZCC_ENABLE_LLVM)
    find_package(LLVM 15.0 CONFIG)
    if(LLVM_FOUND)
        message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
        add_definitions(${LLVM_DEFINITIONS})
        include_directories(${LLVM_INCLUDE_DIRS})
    else()
        message(WARNING "LLVM not found, LLVM backend will be disabled")
        set(ZCC_ENABLE_LLVM OFF)
    endif()
endif()

# 源代码文件
set(ZCC_CORE_SOURCES
    src/core/Compiler.cpp
    src/core/CacheManager.cpp
    src/core/Diagnostics.cpp
    src/core/Config.cpp
)

set(ZCC_FRONTEND_SOURCES
    src/frontend/Lexer.cpp
    src/frontend/Parser.cpp
    src/frontend/SemanticAnalyzer.cpp
    src/frontend/AST.cpp
)

set(ZCC_MIDEND_SOURCES
    src/midend/ZCCIRGenerator.cpp
    src/midend/ZCCIROptimizer.cpp
    src/midend/TypeSpecializer.cpp
    src/midend/IRGenerator.cpp
)

set(ZCC_BACKEND_SOURCES
    src/backend/BackendManager.cpp
    src/backend/CBackend.cpp
    src/backend/WASMBackend.cpp
    src/backend/MCBackend.cpp
    src/backend/BIOSBackend.cpp
    src/backend/MicroPythonBackend.cpp
)

if(ZCC_ENABLE_LLVM)
    list(APPEND ZCC_BACKEND_SOURCES src/backend/LLVMBackend.cpp)
endif()

# 创建核心库
add_library(zcc_core STATIC ${ZCC_CORE_SOURCES})
target_include_directories(zcc_core PUBLIC include/core)

# 创建前端库
add_library(zcc_frontend STATIC ${ZCC_FRONTEND_SOURCES})
target_include_directories(zcc_frontend PUBLIC include/frontend)
target_link_libraries(zcc_frontend PUBLIC zcc_core)

# 创建中端库
add_library(zcc_midend STATIC ${ZCC_MIDEND_SOURCES})
target_include_directories(zcc_midend PUBLIC include/midend)
target_link_libraries(zcc_midend PUBLIC zcc_frontend)

# 创建后端库
add_library(zcc_backend STATIC ${ZCC_BACKEND_SOURCES})
target_include_directories(zcc_backend PUBLIC include/backend)
target_link_libraries(zcc_backend PUBLIC zcc_midend)

if(ZCC_ENABLE_LLVM)
    target_link_libraries(zcc_backend PUBLIC ${LLVM_AVAILABLE_LIBS})
endif()

# 创建可执行文件
add_executable(zcc src/main.cpp)
target_link_libraries(zcc PRIVATE zcc_backend)
target_compile_definitions(zcc PRIVATE 
    ZCC_VERSION="${PROJECT_VERSION}"
    ZCC_ENABLE_LLVM=$<BOOL:${ZCC_ENABLE_LLVM}>
)

# 平台特定设置
if(WIN32)
    target_compile_definitions(zcc PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        target_compile_options(zcc PRIVATE /utf-8)
    endif()
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
install(DIRECTORY include/ DESTINATION include/zcc)

# ZCC 编译器

**Zero-Core Command (零核指令)** 编程语言的 C++ 实现编译器

## 项目概述

ZCC 是一门为跨平台、跨环境设计的通用编程语言，采用中英双语关键词设计。这个仓库包含 ZCC 语言的编译器实现，可将 ZCC 源代码编译为中间字节码。

### 核心特性

- **双语编程**: 支持中文和英文关键词自由切换
- **高精度数字**: 数字以字符串存储，支持任意精度计算
- **动态类型**: 运行时类型可自由切换
- **零依赖**: 最小运行时仅需 64KB 内存
- **跨平台**: 从 BIOS 到云端都能运行
- **字节码输出**: 生成独立中间代码，可由第三方实现解释器/JIT

## 项目结构

```
ZCC/
├── CMakeLists.txt          # 主构建配置
├── include/                # 头文件
│   ├── AST.hpp            # 抽象语法树定义
│   ├── Bytecode.hpp       # 字节码格式定义
│   ├── CodeGenerator.hpp  # 代码生成器
│   ├── Compiler.hpp       # 编译器主类
│   ├── Diagnostics.hpp    # 诊断信息
│   ├── Keywords.hpp       # 关键词映射
│   ├── Lexer.hpp          # 词法分析器
│   ├── Parser.hpp         # 语法分析器
│   └── Token.hpp          # Token 定义
├── src/                    # 源代码
│   ├── CodeGenerator.cpp  # 代码生成器实现
│   ├── Compiler.cpp       # 编译器实现
│   ├── Lexer.cpp          # 词法分析器实现
│   ├── main.cpp           # 命令行工具
│   └── Parser.cpp         # 语法分析器实现
├── tests/                  # 测试代码
│   ├── CMakeLists.txt
│   └── test_lexer.cpp     # 词法分析测试
└── examples/               # 示例代码
    ├── CMakeLists.txt
    ├── hello.zcc          # Hello World
    ├── calculator.zcc     # 计算器示例
    └── fibonacci.zcc      # 斐波那契数列
```

## 构建说明

### 前置要求

- CMake 3.16 或更高版本
- C++17 兼容的编译器 (GCC 7+, Clang 5+, MSVC 2017+)
- Windows: Visual Studio 2017 或更高版本
- Linux: GCC 7+ 或 Clang 5+
- macOS: Xcode 9+ 或 Homebrew 安装的 GCC/Clang

### 构建步骤

```bash
# 克隆仓库
git clone https://github.com/your-repo/zcc-compiler.git
cd zcc-compiler

# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 构建
cmake --build .

# 运行测试
ctest

# 安装 (可选)
cmake --install . --prefix /usr/local
```

### Windows 特定说明

```powershell
# 使用 Visual Studio 生成器
cmake -G "Visual Studio 16 2019" ..
cmake --build . --config Release

# 或使用 Ninja (如果已安装)
cmake -G Ninja ..
cmake --build .
```

## 使用方法

### 编译 ZCC 源代码

```bash
# 编译为字节码
./zcc compile hello.zcc -o hello.zcb

# 带优化编译
./zcc compile main.zcc -O2 -g -o main.zcb

# 指定目标平台
./zcc compile app.zcc --target wasm -o app.zcb
```

### 反汇编字节码

```bash
# 查看字节码内容
./zcc disasm hello.zcb

# 输出到文件
./zcc disasm hello.zcb > hello.disasm
```

### 运行程序

```bash
# 编译并运行 (需要解释器)
./zcc run hello.zcc
```

### 命令行选项

```
用法:
  zcc <命令> [选项] <源文件>

命令:
  compile, c    编译源文件为字节码
  run, r        编译并运行 (需要解释器)
  disasm, d     反汇编字节码文件
  help, h       显示帮助信息

选项:
  -o <文件>     指定输出文件名
  -O0           无优化
  -O1           基础优化
  -O2           中级优化
  -O3           高级优化
  -g            生成调试信息
  --target <平台>  目标平台 (windows/linux/macos/wasm)
  --version     显示版本信息
```

## 字节码格式

ZCC 字节码文件格式如下：

```
+------------------+
| 文件头 (6 字节)    |
| - 魔数 "ZCCB"     |
| - 版本号          |
+------------------+
| 源文件名         |
+------------------+
| 常量池           |
+------------------+
| 函数列表         |
+------------------+
| 类列表           |
+------------------+
| 全局变量         |
+------------------+
| 导入/导出信息    |
+------------------+
| 源代码映射       |
+------------------+
```

### 操作码分类

| 类别 | 操作码范围 | 说明 |
|------|-----------|------|
| 栈操作 | 0x00-0x0F | POP, DUP, SWAP 等 |
| 常量加载 | 0x10-0x1F | LOAD_NULL, LOAD_CONST 等 |
| 变量加载 | 0x20-0x2F | LOAD_LOCAL, LOAD_GLOBAL 等 |
| 属性访问 | 0x30-0x3F | LOAD_PROPERTY, LOAD_SUBSCRIPT 等 |
| 算术运算 | 0x40-0x4F | ADD, SUBTRACT, MULTIPLY 等 |
| 比较运算 | 0x50-0x5F | EQUAL, LESS, GREATER 等 |
| 逻辑运算 | 0x60-0x6F | NOT, AND, OR 等 |
| 控制流 | 0x70-0x7F | JUMP, JUMP_IF_FALSE 等 |
| 函数调用 | 0x80-0x8F | CALL, RETURN, CLOSURE 等 |
| 类与对象 | 0x90-0x9F | NEW_INSTANCE, NEW_ARRAY 等 |
| 异常处理 | 0xA0-0xAF | THROW, TRY_BEGIN, CATCH 等 |

## 开发解释器

第三方可以在不同平台实现 ZCC 解释器。以下是一个简单的解释器框架示例：

```cpp
#include "Bytecode.hpp"

class ZCCInterpreter {
public:
    void loadModule(const zcc::BytecodeModule& module);
    void run();
    
private:
    zcc::BytecodeModule module_;
    std::vector<zcc::Constant> stack_;
    size_t ip_; // 指令指针
};
```

### Minecraft 中使用 ZCC

ZCC 字节码可以很容易地在 Minecraft 中通过命令方块或模组实现：

1. **命令方块方式**: 将字节码转换为命令序列
2. **模组方式**: 实现完整的 ZCC 虚拟机
3. **数据包方式**: 使用函数系统模拟简单操作

## API 参考

### 编译器 API

```cpp
#include "Compiler.hpp"

using namespace zcc;

// 创建编译器
Compiler compiler;

// 编译源代码
CompileResult result = compiler.compile(sourceCode, "test.zcc");

// 检查错误
if (result.hasErrors()) {
    for (const auto& diag : result.diagnostics) {
        std::cerr << diag.toString() << std::endl;
    }
    return 1;
}

// 保存字节码
Compiler::saveBytecode(result.bytecode, "output.zcb");

// 反汇编
std::string disassembly = Compiler::disassemble(result.bytecode);
std::cout << disassembly << std::endl;
```

## 测试

运行所有测试：

```bash
cd build
ctest --verbose
```

运行特定测试：

```bash
ctest -R LexerTest --verbose
```

## 示例程序

### Hello World

```zcc
// hello.zcc
函数 主函数 () {
    打印行 ("你好，世界!");
    返回 0;
}
```

### 计算器

```zcc
// calculator.zcc
函数 主函数 () {
    定义 a = 100;
    定义 b = 23;
    
    打印行 ("a + b = " + (a + b));
    打印行 ("a * b = " + (a * b));
    
    返回 0;
}
```

### 斐波那契数列

```zcc
// fibonacci.zcc
函数 斐波那契 (n) {
    如果 (n <= 1) {
        返回 n;
    }
    返回 斐波那契 (n - 1) + 斐波那契 (n - 2);
}

函数 主函数 () {
    循环 (定义 i = 0; i < 10; i = i + 1) {
        打印行 ("F(" + i + ") = " + 斐波那契 (i));
    }
    返回 0;
}
```

## 贡献指南

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 提交 Pull Request

## 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 致谢

- 感谢所有贡献者
- 参考了 Lua、Python、JavaScript 等语言的设计
- 使用了类似 Java 字节码的中间表示设计

## 相关链接

- [ZCC 语言规范](docs/spec.md)
- [在线教程](https://zcc.dev/learn)
- [示例项目](https://github.com/zcc/examples)
- [社区论坛](https://forum.zcc.dev)

---

**开始您的 ZCC 编程之旅！**

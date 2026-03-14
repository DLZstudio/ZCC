# ZCC 编译器 v0.2.0 - 重构版

## 📋 项目概述

ZCC（Zero-Core Command，零核指令）编译器是一个现代化的、高性能的跨平台编译器，采用三阶段架构设计：

```
前端 (Frontend) → 中端 (Midend) → 后端 (Backend)
     ↓                ↓              ↓
  Lexer/Parser    ZCC-IR 生成    多后端支持
  AST 生成        优化器         - C/C++
  语义分析        类型特化       - WebAssembly
                               - Minecraft
                               - BIOS
                               - MicroPython
```

## 🎯 核心特性

### 跨平台支持
- **x86_64/ARM64** - Windows/Linux/macOS
- **WebAssembly** - 浏览器和 Node.js
- **Minecraft** - Java Mod/数据包
- **BIOS** - x86 实模式引导
- **MicroPython** - 微控制器/嵌入式

### 高性能
- 接近 C++ 性能的本地代码
- 多级优化 (O0-O3, Os, Oz)
- 智能缓存系统，避免重复编译

### 现代化架构
- C++20 标准
- 模块化设计
- 易于扩展新后端
- 完整的类型系统

## 🏗️ 架构设计

### 目录结构
```
ZCC/
├── include/
│   ├── core/           # 核心组件
│   │   ├── Platform.hpp    # 平台抽象层
│   │   ├── Compiler.hpp    # 编译器主类
│   │   └── CacheManager.hpp # 缓存管理
│   ├── frontend/       # 前端
│   │   ├── Lexer.hpp       # 词法分析
│   │   ├── Parser.hpp      # 语法分析
│   │   └── AST.hpp         # 抽象语法树
│   ├── midend/         # 中端
│   │   ├── ZCCIR.hpp       # IR 定义
│   │   ├── Types.hpp       # 类型系统
│   │   └── Optimizer.hpp   # 优化器
│   └── backend/        # 后端
│       ├── Backend.hpp     # 后端接口
│       ├── CBackend.hpp    # C 后端
│       ├── WASMBackend.hpp # WASM 后端
│       └── MCBackend.hpp   # MC 后端
├── src/
│   ├── core/           # 核心实现
│   ├── frontend/       # 前端实现
│   ├── midend/         # 中端实现
│   ├── backend/        # 后端实现
│   └── main.cpp        # 主程序
└── tests/            # 测试
```

### 平台抽象层

平台抽象层提供统一的接口，使编译器可以在不同平台运行：

```cpp
// 文件系统接口
class IFileSystem {
    virtual std::optional<std::vector<uint8_t>> readFile(const std::string& path) = 0;
    virtual bool writeFile(const std::string& path, const std::vector<uint8_t>& data) = 0;
    // ...
};

// 控制台接口
class IConsole {
    virtual void write(const std::string& text) = 0;
    virtual void writeln(const std::string& text) = 0;
    virtual std::string readLine() = 0;
};
```

已实现的平台：
- `StandardPlatform` - 标准 PC/服务器
- `MCPlatform` - Minecraft
- `BIOSPlatform` - BIOS/实模式
- `MicroPythonPlatform` - 微控制器

### ZCC-IR 中间表示

ZCC-IR 是一种低级的、平台无关的中间表示，采用 SSA 形式：

```
; ZCC-IR 示例
.version 1.0
.target x86_64-pc-windows
.module "example"

.func i64 @add(i64 %a, i64 %b) {
.entry:
    %1 = add i64 %a, %b
    ret i64 %1
}
```

支持的操作码：
- 控制流：JMP, JMP_IF, RET, CALL
- 算术运算：ADD, SUB, MUL, DIV, POW
- 高精度运算：BIG_ADD, BIG_SUB, BIG_MUL
- 内存操作：LOAD, STORE, ALLOCA

## 🚀 快速开始

### 环境要求
- C++20 兼容编译器 (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.20+
- (可选) LLVM 15.0+

### 构建
```bash
# 克隆仓库
git clone https://github.com/zcc-lang/zcc.git
cd zcc

# 创建构建目录
mkdir build && cd build

# 配置
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build . -j$(nproc)

# 测试
ctest
```

### 使用
```bash
# 编译为本地可执行文件
zcc compile main.zcc -o main

# 编译为 WebAssembly
zcc compile app.zcc --target wasm32 -o app.wasm

# 编译为 Minecraft Mod
zcc compile game.zcc --backend mc -o mod.jar

# 编译为 BIOS 引导
zcc compile boot.zcc --target i386-none-bios -o boot.bin

# 编译为 MicroPython
zcc compile script.zcc --backend micropython -o script.mpy
```

## 📦 后端支持

### C 后端
生成标准 C 代码，可在任何有 C 编译器的平台运行。

```c
// 生成的 C 代码
int64_t add(int64_t a, int64_t b) {
    return a + b;
}
```

### WebAssembly 后端
生成 WASM 二进制，可在浏览器或 Node.js 中运行。

### Minecraft 后端
生成 Java 代码并打包为 Mod。

```java
// 生成的 Minecraft Mod
@Mod("zcc_mod")
public class ZCCMod {
    @EventHandler
    public void init(FMLInitializationEvent event) {
        // ZCC 代码
    }
}
```

### BIOS 后端
生成 x86 实模式二进制，可作为引导扇区。

```asm
; 生成的 BIOS 代码
[BITS 16]
[ORG 0x7C00]
start:
    mov ax, 0x0003
    int 0x10
```

### MicroPython 后端
生成 MicroPython 字节码，可在微控制器运行。

```python
# 生成的 MicroPython 代码
def add(a, b):
    return a + b
```

## 🔧 扩展指南

### 添加新后端

1. 创建后端类，继承 `Backend` 接口
2. 实现 `initialize()` 和 `compile()` 方法
3. 注册后端

```cpp
class MyBackend : public Backend {
    bool initialize() override { return true; }
    
    CompileResult compile(const ZCCIRModule& module,
                         const CompileOptions& options) override {
        // 实现编译逻辑
    }
    
    std::string getName() const override { return "mybackend"; }
};

// 注册
BackendManager::getInstance().registerBackend(
    std::make_unique<MyBackend>());
```

### 添加新平台

1. 实现平台接口
2. 在 PlatformManager 中注册

```cpp
class MyPlatform : public IFileSystem, public IConsole {
    // 实现接口方法
};

PlatformManager::getInstance().setPlatform(
    std::make_unique<MyPlatform>(),
    // ...
);
```

## 📊 性能对比

| 测试项 | C++ | ZCC (目标) | 说明 |
|--------|-----|------------|------|
| 数值计算 | 1.0x | 1.1x | 高精度计算 |
| 字符串处理 | 1.0x | 1.3x | 动态类型开销 |
| 启动时间 | 1.0x | 1.0x | 本地可执行文件 |
| 编译时间 | 1.0x | 1.5x | 多层优化 |

## 🧪 测试

```bash
# 运行所有测试
ctest --output-on-failure

# 运行特定测试
ctest -R BackendTest

# 性能测试
./tests/performance_test
```

## 📝 示例项目

查看 `examples/` 目录：
- `hello.zcc` - Hello World
- `calculator.zcc` - 计算器
- `fibonacci.zcc` - 斐波那契数列
- `high_precision_cn.zcc` - 高精度计算（中文）
- `high_precision_en.zcc` - 高精度计算（英文）

## 🤝 贡献

### 代码规范
1. 使用 C++20 标准
2. 遵循项目 .clang-format 配置
3. 重要函数需要注释
4. 新功能必须包含测试

### 提交流程
1. Fork 项目
2. 创建特性分支
3. 编写代码和测试
4. 提交 Pull Request

## 📄 许可证

MIT License

## 🔗 相关链接

- [官方文档](https://zcc.dev/docs)
- [在线教程](https://zcc.dev/learn)
- [示例项目](https://github.com/zcc/examples)
- [社区论坛](https://forum.zcc.dev)

---

**开始您的 ZCC 跨平台编程之旅！**

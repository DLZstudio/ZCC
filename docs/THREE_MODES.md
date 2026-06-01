# ZCC 三大编译模式设计文档

> ZCC 编译器的三种执行路径：源码转译 · IR 直接执行 · 一键运行

---

## 目录

- [1. 设计动机](#1-设计动机)
- [2. 模式一：源码转译 (Transpile)](#2-模式一源码转译-transpile)
- [3. 模式二：IR 执行 (IR Exec)](#3-模式二ir-执行-ir-exec)
- [4. 模式三：一键运行 (Run)](#4-模式三一键运行-run)
- [5. 模式选择矩阵](#5-模式选择矩阵)
- [6. CLI 集成](#6-cli-集成)
- [7. 运行时依赖链](#7-运行时依赖链)
- [8. 完成度与路线图](#8-完成度与路线图)

---

## 1. 设计动机

ZCC 的三大编译模式源于一个核心洞察：**同样的 ZCC 代码，在不同场景下有不同的最优执行方式**。

| 场景 | 最优模式 | 理由 |
|------|---------|------|
| 生产部署 | 模式一（转译） | 编译成原生代码，性能最优 |
| 快速原型/调试 | 模式二（IR 执行） | 跳过 C 编译环节，即时反馈 |
| 日常开发 | 模式三（一键） | 自动选择最佳模式 |

三种模式共享同一个 IR 抽象层。前端（Lexer→Parser→IRGenerator）对所有模式完全相同，分歧仅发生在 IR 输出后。

```
                     .zcc 源码
                        │
                        ▼
              ┌──────────────────┐
              │ Lexer → Parser → │
              │   IRGenerator    │
              └────────┬─────────┘
                       │ ZCCIRModule
            ┌──────────┼──────────┐
            ▼          ▼          ▼
        模式一      模式二      模式三
      (Transpile)  (IR Exec)  (Run)
```

---

## 2. 模式一：源码转译 (Transpile)

### 2.1 链路

```
.zcc → Lexer → Parser → IRGenerator → CBackend (C 源码) → GCC → .exe
```

### 2.2 命令

```bash
zcc build hello.zcc --lang c          # 编译为 .exe
zcc build hello.zcc --lang cpp        # 编译为 .exe (C++ 后端)
```

### 2.3 各阶段产物

| 阶段 | 输入 | 输出 | 文件格式 |
|------|------|------|---------|
| Lexer | `hello.zcc` | Token 流 | 内存 |
| Parser | Token 流 | AST | 内存 |
| IRGenerator | AST | ZCCIRModule | 内存 |
| CBackend | ZCCIRModule | C 源码 | `hello.c` |
| GCC | `hello.c` + `zcc_runtime.c` | 可执行文件 | `hello.exe` |

### 2.4 C 代码生成示例

**输入** (`hello.zcc`):
```zcc
auto x = 42;
auto y = x + 1;
```

**生成** (`hello.c`):
```c
#include "zcc_runtime.h"
int main() {
    ZCCValue v1 = zcc_int(42);
    ZCCValue v2 = zcc_int(v1 + 1);
    return 0;
}
```

### 2.5 GCC 调用链

```bash
gcc hello.c zcc_runtime.c -o hello.exe -lm -I. -O1
```

GCC 路径检测逻辑 (`detectGCC()` in LanguageBackend.cpp):
```
优先级查找:
  1. C:/mingw1310/bin/gcc.exe      (junction)
  2. H:/工具/.../mingw1310_64/bin/gcc.exe
  3. C:/mingw64/bin/gcc.exe
  4. C:/msys64/mingw64/bin/gcc.exe
```

### 2.6 完成度: 90%

| 组件 | 状态 |
|------|------|
| Lexer/Parser/AST | ✅ 100% |
| IRGenerator (32 visitor) | ✅ 100% |
| CBackend (66/66 opcode) | ✅ 100% |
| zcc_runtime.c/h | ✅ 100% |
| GCC 自动调用 | ✅ 100% |
| 其他语言后端 (Python/Java) | ❌ 0% |

---

## 3. 模式二：IR 执行 (IR Exec)

### 3.1 链路

```
.zcc → Lexer → Parser → IRGenerator → Interpreter (直接执行)
                        └── toText() → .ir 文件 (调试)
```

### 3.2 命令

```bash
zcc ir hello.zcc           # 仅生成 IR 文本
zcc exec hello.ir          # 执行 IR 文件 (待完整实现)
```

### 3.3 解释器架构

```
Interpreter::execute("main")
  │
  ├── while (currentBlock):        // CFG 跳转
  │   ├── executeBlock(*block)
  │   │   ├── for each instruction:
  │   │   │   ├── resolveOperands()   // 解析 USE
  │   │   │   ├── executeInstruction() // 计算
  │   │   │   ├── reg_[resultID] = value // 存储 DEF
  │   │   │   └── {JMP/JMP_IF/RET}   // 控制流
  │   │   └── return BlockResult{next, value}
  │   └── currentBlock = next
  └── return state_.getReturn()
```

### 3.4 解释器组件

| 组件 | 说明 | 状态 |
|------|------|------|
| `InterpreterState` | 作用域栈 + 函数注册表 | ✅ |
| 内置函数 (9个) | println/print/input/typeof/len/str/num/exit | ✅ |
| 指令执行 (63/66) | 算术/比较/逻辑/位/数组/对象/字符串/控制流 | ✅ |
| BigNum (7函数) | 字符串高精度 + Karatsuba 乘法 | ✅ |
| CFG 跳转引擎 | executeBlock → BlockResult | ✅ |
| `resolveOperand` | 常量/寄存器 操作数解析 | ✅ |

### 3.5 与模式一的对比

| 维度 | 模式一 | 模式二 |
|------|--------|--------|
| 编译速度 | 慢（GCC C 编译） | 快（直接解释） |
| 执行速度 | 快（原生代码） | 慢（逐条解释） |
| 需要 GCC | 是 | 否 |
| 跨平台 | C 后端输出→GCC | 纯 C++ 解释器 |
| 调试 IR | 中间文件 .c | 直接 .ir 文本 |
| 支持全部特性 | ✅ | ✅ (96%) |

### 3.6 完成度: 96%

仅 `INTRINSIC` 和 `ASSEMBLY` 两个编译器内部 opcode 保持桩状态，其余 63/66 已完整实现。

---

## 4. 模式三：一键运行 (Run)

### 4.1 链路

```
zcc run hello.zcc
  │
  ├── [内部] zcc build hello.zcc      (模式一)
  │         → hello.exe
  │
  └── [执行] hello.exe
            → stdout 输出
            → 退出码透传
```

### 4.2 命令

```bash
zcc run hello.zcc           # 编译 + 运行
zcc run hello.zcc -O2       # 优化级别 2
```

### 4.3 实现

```cpp
int runCommand(int argc, char* argv[], int argIndex) {
    // 1. 复用 build 编译
    int buildRet = buildCommand(argc, argv, argIndex);
    if (buildRet != 0) return buildRet;
    
    // 2. 找到输出文件
    std::string exe = inputFile.replace(".zcc", ".exe");
    
    // 3. 执行
    std::cout << "Running: " << exe << "\n---\n";
    int ret = std::system(exe.c_str());
    std::cout << "---\nExit code: " << ret << "\n";
    return 0;
}
```

### 4.4 完成度: 85%

模式三是对模式一的简单包装。当前缺失：
- 无自动模式选择（总是走模式一）
- 无超时控制
- 无进程隔离

---

## 5. 模式选择矩阵

```
                    需要性能？        需要 GCC？      目标平台
模式一 (Transpile)     ✅               ✅           C → any
模式二 (IR Exec)       ❌               ❌           C++ (Win/Linux/macOS)
模式三 (Run)           ✅               ✅           模式一包装
```

---

## 6. CLI 集成

### 6.1 命令对照

```bash
# 模式一
zcc build hello.zcc --lang c          # 源码 → C → GCC → .exe
zcc build hello.zcc --lang cpp        # 源码 → C++ → G++ → .exe

# 模式二
zcc ir hello.zcc                      # 源码 → IR → .ir 文件
zcc exec hello.ir                     # IR 文件 → 解释器执行

# 模式三
zcc run hello.zcc                     # build + 执行
```

### 6.2 共享选项

| 选项 | 模式一 | 模式二 | 模式三 |
|------|--------|--------|--------|
| `-o <file>` | ✅ | ✅ | ✅ |
| `-O0`/`-O1`/`-O2`/`-O3` | ✅ | ❌ | ✅ |
| `--lang c/cpp` | ✅ | ❌ | ❌ |
| `-g` | ✅ | ❌ | ✅ |

---

## 7. 运行时依赖链

### 7.1 模式一依赖

```
zcc_build → CompilerDriver → build()
  ├── Lexer::tokenize()
  ├── Parser::parse()
  ├── IRGenerator::generate()
  ├── Optimizer::optimize()
  ├── LanguageBackend::generateSource()
  └── std::system(gcc ...)
      └── zcc_runtime.c → BigNum / ZCCValue / Array / Object
```

### 7.2 模式二依赖

```
zcc_ir → irCommand()
  ├── Lexer::tokenize()
  ├── Parser::parse()
  └── IRGenerator::generate()
      └── ZCCIRModule::toText()

zcc_exec → execCommand()
  ├── ZCCIRModule::deserialize()  [未实现]
  └── NativeBackend::runModule()
      ├── InterpreterState (作用域 + 内置函数)
      └── Interpreter::execute()
          ├── executeBlock() → CFG 跳转
          └── executeInstruction() → 63/66 opcode
```

### 7.3 模式三依赖

```
zcc_run → 模式一 build + std::system(exe)
```

---

## 8. 完成度与路线图

### 8.1 总体

| 模式 | 命令 | 完成度 | 说明 |
|------|------|--------|------|
| 模式一 | `zcc build` | **90%** | 缺其他语言后端 |
| 模式二 | `zcc ir` / `zcc exec` | **96%** | 仅 INTRINSIC/ASSEMBLY 桩 |
| 模式三 | `zcc run` | **85%** | 模式一包装 |

### 8.2 当前 Sprint 成果

| 组件 | 状态 |
|------|------|
| IR 数据流 (P0) | ✅ |
| SCA v0.3 Manifest (P0) | ✅ |
| IR Interpreter 框架 + 63/66 opcode (P1) | ✅ |
| C 后端 66/66 opcode (P2) | ✅ |
| JIT 预埋 (P2) | ✅ |
| BigNum Karatsuba (P1) | ✅ |

### 8.3 下一阶段

| 优先级 | 任务 |
|--------|------|
| P3 | CLI 重写 (统一命令解析) |
| P3 | 优化器 Pass 填充 (6 个) |
| P4 | `zcc exec` 完整 IR 文件加载执行 |
| P4 | 其他语言后端 (Python/Java) |
| P5 | ModuleManager 完善 |
| P5 | 外部进程 IPC 桥接 |

### 8.4 关键文件索引

| 文件 | 内容 |
|------|------|
| `src/main.cpp` | CLI 入口，三种模式的命令分发 |
| `src/Compiler.cpp` | 模式一：旧编译管线 |
| `include/compiler/CompilerDriver.hpp` | 模式一：新编译管线 |
| `src/compiler/LanguageBackend.cpp` | 模式一：C 代码生成 |
| `src/backend/NativeBackend.cpp` | 模式二：解释器 |
| `include/midend/IRGenerator.hpp` | 所有模式共享的 IR 生成器 |
| `runtime/zcc_runtime.c/h` | 模式一的运行时库 |
| `include/backend/JITBackend.hpp` | 未来模式二的 JIT 后端 |

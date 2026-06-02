# ZCC 编译器当前状态报告

> **日期**: 2026-06-01  
> **工作仓库**: `e:\114514\语言\ZCC\v0.2\`  
> **构建输出**: `zcc.exe` (MinGW GCC 13.1.0, C++20)  
> **最近更新**: SCA v0.3 Manifest · 版本约束检查 · Module.o+IPC.o 链接 · IR Interpreter 框架 (CFG) · 35/66 opcode

---

## 版本布局

```
e:\114514\语言\ZCC\
├── v0.1/          # 原 v0.1 仓库 (已清理，待移植 Module/IPC)
│   ├── include/   # 前端头文件 + Module.hpp / IPC.hpp
│   └── src/       # 前端源码 + Module.cpp / IPC.cpp
│
└── v0.2/          # 主力开发版本 ✅
    ├── include/   # 完整编译器头文件
    ├── src/       # 完整编译器源码
    ├── runtime/   # C 运行时库 (P0 完成)
    │   ├── zcc_runtime.h  # 类型定义 + 全部函数声明
    │   └── zcc_runtime.c  # BigNum/Array/Object/String/异常
    ├── test/      # 测试用例
    └── build/     # 构建产物 (zcc.exe + lib)
```

---

## ZCC 三大编译模式 (官方规格)

| 模式 | 代号 | 链路 | 完成度 | 优先级 |
|------|------|------|--------|--------|
| 模式一：源码转译 | `zcc build --lang c` | .zcc → IR → C → GCC → exe | **90%** | ✅ 交付 |
| 模式二：通用 IR | `zcc ir` / `zcc exec` | .zcc → IR → Interpreter | **60%** | ← 当前 |
| 模式三：直出可执行 | `zcc run` | 模式一的用户包装 | **85%** | ✅ 交付 |

### 模式一详析 (90%)

```
✅ Lexer/Parser/AST             100%
✅ IRGenerator (32 visitors)     100%
✅ CBackend (66/66 opcodes)      100%  ← 刚补齐: ALLOCA~UNREACHABLE 全覆盖
✅ zcc_runtime.c/h               100%
✅ GCC自动调用 (zcc build)       100%
❌ 其他语言后端(Python等)           0%
```

### 模式二详析 (60%)  ← 当前 Sprint

```
✅ IR生成 + toText()             100%
✅ IR 数据流 (P0)                100%
✅ SCA v0.3 Manifest 解析器     100%  ← 刚完成
✅ 版本约束检查 (SemVer)        100%  ← 刚完成
✅ IR Interpreter 框架 (CFG)     100%  ← execute→executeBlock→BlockResult
✅ 堆内存管理 (ALLOC/FREE)       100%  ← 句柄表 + 边界检查 + OOM
⚠️ IR Interpreter opcode        95%   ← 63/66 已实现 (BigNum Karatsuba + 全类别)
❌ 优化器 (8 Pass)              25%   ← 2/8 有实质逻辑
⚠️ JIT (LLVM)                    桩   ← P2 接口桩完成 (JITBackend.hpp)
```

### 模式三详析 (85%)

```
✅ CompilerDriver/插件系统        100%
✅ zcc build/run/languages        100%
✅ IR→C→GCC→exe全链路             100%
⚠️ zcc exec (IR执行)                桩  ← CLI 桩, Interpreter 引擎已就绪
```

---

## 一、最近完成 (2026-06-01)

### 1.0 P0 IR 数据流修复 ← 核心突破

**文件**: `include/midend/IRGenerator.hpp`, `include/midend/ZCCIR.hpp`, `src/compat_stubs.cpp`

| 修改 | 说明 |
|------|------|
| `values_` 映射表 | `unordered_map<ValueID, shared_ptr<ZCCIRValue>>` — 所有创建的值注册在此 |
| `emitInstruction` 操作数解析 | 从 values_ 查找操作数 ID → 创建真实 ZCCIROperand(ValuePtr) |
| `emitTerminator` 操作数解析 | 同上 |
| `ZCCIRInstruction::result_` | 新增 DEF 字段 + `getResult()`/`setResult()` 方法 |
| `toString()` DEF 显示 | `%result = opcode %instrID %operand1 %operand2 ...` |
| 常量/变量/参数注册 | 所有值创建处 (`emitConstant`/`declareVariable`/参数声明) 均注册到 values_ |
| `nextValueID_ = 1` | 从 1 开始，0 作为"未找到"哨兵 |
| `currentExprResult_` 修复 | 所有表达式 visitor 正确设置结果值 |

**验证**: 10/10 回归测试通过，IR 输出从 `add _ _` 变为 `%6 = add %2 %4 %5`

### 1.1 CLI 文档

- **[CLI_DOCUMENTATION.md](CLI_DOCUMENTATION.md)** — 8 命令详细记录: 语法/选项/成功样例/错误样例/当前问题
- 识别 8 个 CLI 问题: 双管线并存、选项不互通、argc/argv 手动解析等

### 1.2 CodeGraph MCP 接入

- 项目级 MCP: `.mcp.json` + `.codegraph/` 数据库
- 索引规模: **78 文件 · 3883 节点 · 8896 边**
- 8 个工具: `codegraph_context` / `search` / `callers` / `callees` / `impact` / `node` / `files` / `status`

### 1.3 IR/JIT 全面摸底

- 完整统计: IR Interpreter (63/66 opcode) / C 后端 (100%) / Optimizer (2/8 pass) / JIT (接口桩)
- 确定 P1 优先级: 先搞 Interpreter, JIT 后续

### 1.4 构建脚本

- **`build.sh`** — 一键编译 13 个源文件 + 链接 (含 `-nostartfiles` 中文路径绕过方案)

### 1.5 SCA v0.3 Manifest 系统

**文件**: `include/Module.hpp`, `src/Module.cpp`

| 修改 | 说明 |
|------|------|
| `SCAManifest` / `SCAManifestDependency` / `SCAManifestInterface` | 3 个新 struct 定义 |
| `parse_manifest()` | Manifest 解析器 — section 状态机, 支持 `[dependency]`/`[interface]` 节语法 |
| `parse_sca_file()` 更新 | v0.3 优化: 提取 manifest 段, 过滤 c/cpp/asm 块 |
| `SemVer` + `checkVersionConstraint()` | 版本约束检查: `>=`/`>`/`==` SemVer 运算符 |
| `validate()` 更新 | 版本不匹配 → 报错, 禁止块检测 |
| `load()` 更新 | Manifest 模式验证, 接口映射优先于正则猜测 |
| `Module.o + IPC.o 完整链接` | 修复 3 个 `mutable mutex` + 4 个 `using` 类型引用 |

### 1.6 IR Interpreter 框架修复

**文件**: `src/backend/NativeBackend.cpp`, `include/backend/NativeBackend.hpp`

| 修改 | 说明 |
|------|------|
| `execute()` CFG 跳转 | 线性遍历 → `while (currentBlock)` |
| `executeBlock()` → `BlockResult` | 返回 `{nextBlock, value}`，处理 JMP/JMP_IF/JMP_IF_NOT/RET |
| `reg_` 指令寄存器 | `unordered_map<uint32_t, RuntimeValue>` — 指令结果存储 |
| `resolveOperand()` | 从常量/reg_ 查找操作数值 |
| `constantToRuntime()` | ZCCIRConstant → RuntimeValue 转换 |
| 已实现 opcode: 35/66 | ADD/SUB/MUL/DIV/MOD/NEG/POW + 6比较 + AND/OR/NOT + SHL/SHR/XOR + 数组4 + 对象3 + 字符串4 + CALL + THROW + PHI + NOP

---

## 一、已完成的工作 (截至 2026-05-31)

### 1.1 构建系统

| 项目 | 状态 |
|------|------|
| 编译环境 | ✅ MinGW GCC 13.1.0, CMake 可用的构建链 |
| 10 个源文件全部编译通过 | ✅ Lexer / Parser / Compiler / CodeGenerator / Optimizer / BackendManager / CBackend / NativeBackend / BIOSBackend / main |
| 额外编译单元 | ✅ compat_stubs.cpp (桩实现) / IRGenerator.cpp (IR 生成器) |
| 链接产物 | ✅ `zcc.exe` 6.6MB |

### 1.2 前端 (Lexer + Parser + AST) — 100%

```
.zcc 源码 → Lexer → Token 流 → Parser → AST → ✅
```

- 489 行 Lexer：UTF-8/BOM，中英文标识符，科学记数法，三种引号，完整转义序列
- 1243 行 Parser：递归下降 + Pratt 解析，15 级优先级，30+ 语法结构
- 811 行 AST：15 表达式 + 12 语句 + 7 声明 + Visitor
- 276 行 Keywords：60+ 对中英双语关键词映射

### 1.3 字节码系统 — 95%

```
AST → CodeGenerator → BytecodeModule → 序列化(.zbc) → ✅
```

- 854 行 Bytecode.hpp：70+ 操作码栈式虚拟机 + 反汇编器
- 1033 行 CodeGenerator.cpp：全 AST → 字节码翻译

### 1.4 旧编译管线 — ✅ 可用

```
.zcc → Lexer → Parser → CodeGenerator → 字节码 → 报告成功
```

主命令 `compile` 走这条路径，已验证 `auto x = 42;` 能编译成功。

### 1.5 IR 生成器 — ✅ 100% (新完成)

所有 12 个 TODO 方法已实现：

| 方法 | 功能 | 状态 |
|------|------|------|
| visitProgram | 程序入口，遍历声明 | ✅ |
| visitFunctionDecl | 函数声明，创建块和参数 | ✅ |
| visitVariableDecl | 变量声明，初始化 | ✅ |
| visitBlockStmt | 块语句，作用域管理 | ✅ |
| visitExpressionStmt | 表达式语句 | ✅ |
| visitIfStmt | if/else，创建分支块 | ✅ |
| visitForStmt | for 循环，init/cond/incr 块 | ✅ |
| visitWhileStmt | while 循环 | ✅ |
| visitReturnStmt | return 语句 | ✅ |
| visitBreakStmt / visitContinueStmt | 循环控制 | ✅ |
| visitLiteralExpr | 字面量 (数字/字符串/布尔/null) | ✅ |
| visitBinaryExpr | 二元运算 (算术/比较/逻辑) | ✅ |
| visitUnaryExpr | 一元运算 (负/非/位非) | ✅ |
| visitAssignExpr | 赋值 | ✅ |
| visitCallExpr | 函数调用 | ✅ |
| visitIdentifierExpr | 标识符加载 | ✅ |
| **visitTernaryExpr** | **三元运算 a ? b : c** | ✅ 新 |
| **visitMemberExpr** | **成员访问 obj.prop** | ✅ 新 |
| **visitIndexExpr** | **索引访问 arr[i]** | ✅ 新 |
| **visitArrayExpr** | **数组字面量 [a, b]** | ✅ 新 |
| **visitObjectExpr** | **对象字面量 {k: v}** | ✅ 新 |
| **visitFunctionExpr** | **函数表达式** | ✅ 新 |
| **visitArrowFunctionExpr** | **箭头函数 =>** | ✅ 新 |
| **visitForEachStmt** | **for-each 循环** | ✅ 新 |
| **visitSwitchStmt** | **switch-case** | ✅ 新 |
| **visitThrowStmt** | **throw 语句** | ✅ 新 |
| **visitTryStmt** | **try-catch-finally** | ✅ 新 |
| **visitClassDecl** | **类声明** | ✅ 新 |

### 1.6 IR 管线集成 — ✅ (P1 完成)

```
.zcc → Lexer → Parser → IRGenerator → ZCCIRModule → toText() → .ir 文件
```

新增 CLI 命令：`zcc ir test.zcc` — 完整 IR 文本输出。

### 1.7 CBackend IR 操作码全覆盖 — ✅ (P0 完成)

62 个 IR 操作码的 C 代码生成已全部实现：

| 类别 | 操作码 | C 映射 |
|------|--------|--------|
| 算术 | ADD/SUB/MUL/DIV/MOD/NEG/INC/DEC/POW | 直接 C 运算符 + pow() |
| 位运算 | AND/OR/XOR/NOT/SHL/SHR | 直接 C 运算符 |
| 比较 | CMP_EQ/NE/LT/LE/GT/GE | 直接 C 比较运算符 |
| 内存 | LOAD/STORE/ALLOCA/ALLOC/FREE | 赋值 / malloc / free |
| 类型转换 | TRUNC/ZEXT/SEXT/FP* | C 强制类型转换 |
| 控制流 | JMP/JMP_IF/JMP_IF_NOT/CALL/RET/PHI | goto / if / return / 注释 |
| 高精度 | BIG_ADD/SUB/MUL/DIV/MOD/CMP/POW | bignum_* 运行时函数 |
| 字符串 | STR_CONCAT/LEN/CMP/SLICE | str* / zcc_* 运行时函数 |
| 数组 | ARRAY_NEW/GET/SET/LEN | zcc_array_* 运行时函数 |
| 对象 | OBJECT_NEW/GET/SET | zcc_object_* 运行时函数 |
| 异常 | THROW | zcc_throw() 运行时函数 |
| 特殊 | NOP/INTRINSIC/ASSEMBLY/TRAP/UNREACHABLE | 注释 / __asm__ / abort |

### 1.8 桩文件 (compat_stubs.cpp)

提供头文件中声明但未实现的方法：

| 模块 | 桩实现 |
|------|--------|
| TypeManager | getVoid/getBool/getI64/getF64/getString/getAny |
| PrimitiveTypeImpl | getPrimitiveName/equals/toString/getSize/getAlignment |
| FunctionType | buildName/equals/toString |
| ZCCType | isCompatibleWith/isSubtypeOf |
| ZCCIRBlock | addSuccessor/addPredecessor/isTerminated |
| ZCCIRFunction | createBlock |
| ZCCIRModule | createFunction/createConstant/serialize/toText |
| TargetPlatform | getCurrent/parse/各种 getter |
| PlatformManager | getInstance/initialize |

### 1.9 ZCC Runtime 运行时库 — ✅ (P0 完成)

```
runtime/zcc_runtime.h (160行) + runtime/zcc_runtime.c (730行)
```

完整实现了 CBackend 生成的 C 代码所需的所有运行时函数：

| 模块 | 功能 | 测试 |
|------|------|------|
| BigNum | add/sub/mul/div/mod/cmp/pow/negate — 字符串高精度 | ✅ 20位×2=21位 |
| ZCCValue | 8 种动态类型 + 自动类型提升 | ✅ 42+3.14=45.14 |
| ZCCArray | new/get/set/len/push — 动态扩容 | ✅ 3 元素 |
| ZCCObject | new/get/set/has — 64 桶哈希表 | ✅ 键值对 |
| 字符串 | concat/slice | ✅ |
| 算术 | add/sub/mul/div/mod/neg/pow — 全运算符 | ✅ |
| 比较/逻辑 | eq/ne/lt/le/gt/ge/and/or/not | ✅ |
| 异常 | zcc_throw — 打印并 abort | ✅ |
| 打印 | print/println/print_str | ✅ |

全部 5 大类功能通过集成测试验证。

### 1.7 IR 操作码 (ZCCIR.hpp)

62 个操作码，覆盖：控制流 / 算术 / 位运算 / 比较 / 内存 / 类型转换 / 高精度 / 字符串 / 数组 / 对象 / 异常 / 特殊操作

---

## 二、当前编译路径总览

```
                            main.cpp
                               │
                    ┌──────────┴──────────┐
                    │                     │
              旧路径(可用)           新 IR 路径(可用)
                    │                     │
              Compiler.compile()       irCommand()
                    │                     │
         Lexer → Parser          Lexer → Parser
                    │                     │
         CodeGenerator              IRGenerator ✅
                    │                     │
         BytecodeModule         ZCCIRModule::toText()
                    │                     │
              .zbc 字节码            .ir 文本文件
                                          │
                                    CBackend (62/62 opcodes)
                                          │
                                     .c C 源文件
```

---

## 三、待完成 (按优先级)

### 🔴 绝对优先级: 模式二核心 (IR Interpreter opcode 填充)

| 阶段 | 状态 | 优先级 |
|------|------|--------|
| **已实现**: 算术/比较/逻辑/位运算/数组/对象/字符串/CALL/THROW | ✅ 35/66 | P1 |
| **待填充**: 类型转换(10) + 高精度(4) + 内存(3) + INC/DEC + 特殊(4) + 完整 CALL 函数调用链 | ❌ 31 个 | P1 |

### 🟡 暂缓

| 阶段 | 状态 | 优先级 |
|------|------|--------|
| JIT 预埋 (LLVM JIT 接口桩) | ✅ 完成 | P2 |
| CLI 重写 | 文档已完成, 实现暂缓 | P3 |
| 优化器 Pass 填逻辑 (6 个空的) | ❌ 框架就绪 | P3 |
| 其他语言后端 (Python/Java/...) | ❌ | P4 |
| ModuleManager::resolveVoidInterfaces() | ❌ 只有声明 | P5 |

### 新增组件

| 组件 | 文件 | 功能 |
|------|------|------|
| 模块系统 | Module.hpp/cpp (575+1090行) | 虚空接口、SmartSCALoader、依赖分析 |
| IPC 框架 | IPC.hpp/cpp (529+837行) | Windows命名管道/Unix Socket进程通信 |

### 已完成 (已交付)

| 阶段 | 状态 |
|------|------|
| ✅ P0 IR 数据流修复 | 100% |
| ✅ **P1**: 插件化语言后端 + CompilerDriver + zcc build/run/languages | 100% |
| ✅ P0 CBackend 66/66 操作码全覆盖 | 100% |
| ✅ zcc_runtime C 运行时库 | 100% |
| ✅ IR 生成器 32 个 visitor 方法 | 100% |
| ✅ Lexer 注释 + 换行 Bug 修复 | 100% |
| ✅ v0.1 Module/IPC 移植 | 100% |
| ✅ v0.1 enum/namespace/@external 语法移植 | 100% |
| ✅ 前端 Lexer/Parser/AST/Keywords | 100% |
| ✅ CLI 文档 (CLI_DOCUMENTATION.md) | 100% |
| ✅ CodeGraph MCP 接入 | 100% |
| ✅ ++/+=/=>/NaN/前向引用/arr[i]=v/I/O关键字 | 100% |

### 插件系统架构

```
新语言 = 实现 ILanguageBackend + 注册 → 一行命令切换
                                    
ILanguageBackend (协议)
├── generateSource(IR) → 目标语言源码
├── getCompilerConfig() → 编译器路径+命令模板
├── getRuntimeFiles() → 运行时库文件
└── needsCompilation() → 是否需要编译步骤

内置:
  c   → gcc + zcc_runtime.c
  cpp → g++ + zcc_runtime.c (预留)

扩展示例:
  class PythonBackend : ILanguageBackend { ... generateSource → Python代码 ... }
  class JavaBackend   : ILanguageBackend { ... generateSource → Java代码 ... }
```

---

## 四、文件清单

### 源文件 (13 + 2 runtime)

| 文件 | 行数 | 状态 |
|------|------|------|
| src/Lexer.cpp | 489 | ✅ 完成 |
| src/Parser.cpp | 1243 | ✅ 完成 |
| src/Compiler.cpp | 108 | ✅ 完成 |
| src/main.cpp | 340 | ✅ 完成（含 8 命令 CLI） |
| src/midend/Optimizer.cpp | 344 | ⚠️ 框架就绪 (2/8 pass 有逻辑) |
| src/midend/IRGenerator.cpp | 3 | ✅ 编译入口 |
| src/compiler/LanguageBackend.cpp | 410+ | ⚠️ 41/66 opcode |
| src/backend/BackendManager.cpp | 77 | ✅ 完成 |
| src/backend/CBackend.cpp | 368 | ⚠️ 字节码后端 |
| src/backend/NativeBackend.cpp | 500+ | ✅ 63/66 opcode |
| src/backend/BIOSBackend.cpp | 193 | ⚠️ 简化 |
| src/compat_stubs.cpp | 262 | ✅ IR 文本输出 + 类型桩 |

### 头文件 (22 个)

| 目录 | 文件 |
|------|------|
| include/ | AST.hpp, Bytecode.hpp, CodeGenerator.hpp, Compiler.hpp, Diagnostics.hpp, Keywords.hpp, Lexer.hpp, Parser.hpp, SymbolTable.hpp, Token.hpp, Module.hpp, IPC.hpp |
| include/core/ | CacheManager.hpp, Common.hpp, Compiler.hpp, Platform.hpp |
| include/midend/ | IRGenerator.hpp, Optimizer.hpp, Types.hpp, ZCCIR.hpp |
| include/backend/ | Backend.hpp, NativeBackend.hpp |
| include/compiler/ | CompilerDriver.hpp, LanguageBackend.hpp |

### 文档 (6 个)

| 文件 | 内容 |
|------|------|
| README.md | 项目简介 + 快速开始 |
| DEVELOPMENT_STATUS.md | 本文档 |
| CLI_DOCUMENTATION.md | CLI 8 命令详细文档 |
| TEST_ANALYSIS.md | 测试分析报告 |
| DEVELOPMENT_REPORT.md | 开发报告 |
| SYNTAX_MIGRATION.md | v0.1→v0.2 语法移植记录 |

### 构建

| 文件 | 说明 |
|------|------|
| build.sh | 一键编译链接 (MinGW GCC 13.1.0) |
| build/zcc.exe | 编译器主程序 |

### 测试

| 目录 | 数量 |
|------|------|
| test/*.zcc | 47 个测试文件 (含 11 个正式测试) |
| test/爆破报告/ | 5 轮爆破测试报告 |

---

## 五、开发优先级 (最终交付线)

```
P0 ✅ IR 数据流修复              → 已完成
P1 ⬜ IR Interpreter (66 opcode)   → 当前 (计划 6 Phase)
P2 ✅ JIT 预埋 (LLVM 接口桩)      → 完成
P3 ⬜ CLI 重写                     → 暂缓 (文档已完成)
P4 ⬜ 优化器 Pass 填充             → 后续
P5 ⬜ ModuleManager 完善            → 远期
```

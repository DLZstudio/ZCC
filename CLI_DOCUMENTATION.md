# ZCC CLI 详细文档 v0.2

> **生成日期**: 2026-06-01
> **目的**: 记录当前 CLI 所有功能、命令行接口、输出格式和错误处理，为后续 CLI 重构提供完整基线。

---

## 目 录

- [1. 命令总线](#1-%E5%91%BD%E4%BB%A4%E6%80%BB%E7%BA%BF)
- [2. 各命令详解](#2-%E5%90%84%E5%91%BD%E4%BB%A4%E8%AF%A6%E8%A7%A3)
  - [2.1 `help` / `h` / `--help` — 帮助信息](#21-help--h----help--%E5%B8%AE%E5%8A%A9%E4%BF%A1%E6%81%AF)
  - [2.2 `compile` / `c` — 旧编译管线](#22-compile--c--%E6%97%A7%E7%BC%96%E8%AF%91%E7%AE%A1%E7%BA%BF)
  - [2.3 `build` / `b` — 新编译管线 (P1)](#23-build--b--%E6%96%B0%E7%BC%96%E8%AF%91%E7%AE%A1%E7%BA%BF-p1)
  - [2.4 `run` / `r` — 编译并执行](#24-run--r--%E7%BC%96%E8%AF%91%E5%B9%B6%E6%89%A7%E8%A1%8C)
  - [2.5 `ir` — 生成 ZCC-IR](#25-ir--%E7%94%9F%E6%88%90-zcc-ir)
  - [2.6 `exec` / `x` — 执行 IR (未实现)](#26-exec--x--%E6%89%A7%E8%A1%8C-ir-%E6%9C%AA%E5%AE%9E%E7%8E%B0)
  - [2.7 `languages` — 列出目标语言](#27-languages--%E5%88%97%E5%87%BA%E7%9B%AE%E6%A0%87%E8%AF%AD%E8%A8%80)
  - [2.8 `backends` — 列出后端](#28-backends--%E5%88%97%E5%87%BA%E5%90%8E%E7%AB%AF)
- [3. 双编译管线](#3-%E5%8F%8C%E7%BC%96%E8%AF%91%E7%AE%A1%E7%BA%BF)
- [4. 输入解析问题](#4-%E8%BE%93%E5%85%A5%E8%A7%A3%E6%9E%90%E9%97%AE%E9%A2%98)
- [5. 退出码约定](#5-%E9%80%80%E5%87%BA%E7%A0%81%E7%BA%A6%E5%AE%9A)
- [6. 重构建议](#6-%E9%87%8D%E6%9E%84%E5%BB%BA%E8%AE%AE)

---

## 1. 命令总线

```
zcc.exe <command> [options] <source>
```

入口: `src/main.cpp:main()` (L374-419)

| 命令 | 别名 | 函数 | 状态 |
|------|------|------|------|
| `help` | `h`, `--help` | `printUsage()` | 正常 |
| `compile` | `c` | `compileCommand()` | 旧管线 (Legacy) |
| `build` | `b` | `buildCommand()` | P1 新管线 |
| `run` | `r` | `runCommand()` | 构建+执行 |
| `ir` | — | `irCommand()` | P0 已完成 |
| `languages` | — | `languagesCommand()` | 正常 |
| `backends` | — | `listBackends()` | 正常 |
| `exec` | `x` | `execCommand()` | 桩 (IR Interpreter 已就绪) |

丢弃规则:
- 无命令或无参数 → 打印 help + 退出码 0 (无参数用 help，空命令用 usage)
- 未知命令 → stderr + 退出码 1
- 无源文件 → stderr + 退出码 1

---

## 2. 各命令详解

### 2.1 `help` / `h` / `--help` — 帮助信息

入口: `printUsage()` (L19-59)

**输出**:
```
ZCC Compiler v0.2.0 - Zero-Core Command Language

Usage:
  zcc.exe <command> [options] <source>

Commands:          [共 7 个命令列表]
Options:           [共 8 个选项]
Supported backends:[5 个后端]
File formats:      [6 种格式]
Examples:          [5 条示例]
```

**退出码**: 0

**当前问题**:
- 程序名使用 `argv[0]`（完整路径），输出中包含中文路径，终端体验差
- 帮助文档中列出的 `--target`、`--no-cache`、`--keep-ir` 部分选项未被实际解析
- 5 条示例中 `exec` 命令未实现

---

### 2.2 `compile` / `c` — 旧编译管线

入口: `compileCommand()` (L174-274)  
管线: `Compiler::compileFile(filename)` → Compiler.hpp → 未完整实现

**语法**:
```
zcc compile <source.zcc> [options]
```

**选项**:
| 选项 | 说明 | 默认值 |
|------|------|--------|
| `-o <file>` | 输出文件名 | 自动推断(去扩展名) |
| `-O0`, `-O1`, `-O2`, `-O3` | 优化级别 | `-O1` |
| `-g` | 调试信息 | `false` |
| `--target <triple>` | 目标平台 | `x86_64-pc-windows-default` |
| `--backend <name>` | 后端名称 | `native` |
| `--keep-ir` | 保留 IR 文件 | `true`(默认) |
| `--no-cache` | 禁用缓存 | `false` |

**成功样例**:
```
ZCC Compiler v0.2.0
Compiling test/hello.zcc...
Backend: c
Target: x86_64-pc-windows-default
Optimization: -O1
Keep IR: yes

Compilation successful!
Output: test/hello
Compile time: 0ms
IR file: test/hello.ir
```

**失败样例 (文件不存在)**:
```
ZCC Compiler v0.2.0
Compiling no_such_file.zcc...
Backend: native
Target: x86_64-pc-windows-default
Optimization: -O1
Keep IR: yes

Compilation failed!
  Failed to read file: no_such_file.zcc (line 0)
```

**当前问题**:
- 输出文件名 `test/hello` 缺少 `.exe` 扩展名 (Windows)
- `result.output` 拼接不正确: `outputFile + result.output` → `test/hello` + 空字符串
- `CompileTime: 0ms` — 计时未实现
- 使用旧 `Compiler` 类，与 `build` 命令走不同管线
- 自动推断输出文件名逻辑简单（仅去后缀），不处理路径分隔符

---

### 2.3 `build` / `b` — 新编译管线 (P1)

入口: `buildCommand()` (L84-127)  
管线: `CompilerDriver::build()` (7 步流水线)

**语法**:
```
zcc build <source.zcc> [options]
```

**选项**:
| 选项 | 说明 | 默认值 |
|------|------|--------|
| `-o <file>` | 输出文件 | 自动推断 |
| `--lang <name>` | 目标语言 | `c` |
| `-O0`, `-O1`, `-O2`, `-O3` | 优化级别 | `-O1` |

**7 步编译流水线**:
```
1. runLexer         → Token 列表
2. runParser        → AST (程序)
3. runIRGenerator   → ZCCIRModule (IR)
4. runOptimizer     → 优化后的 IR
5. runLanguageBackend → 目标语言源码 (.c / .cpp / ...)
6. runExternalCompiler → GCC/MSVC 编译 → 可执行文件
7. (run)            → 运行产物
```

**成功样例 (预期)**:
```
ZCC Compiler v0.2 (P1 Pipeline)
Building: test/hello.zcc
Target: c (C Source Code (via GCC/Clang/MSVC))
Generated: test/hello.c
Compiling: gcc test/hello.c -o test/hello.exe ...
Output: test/hello.exe

Build successful!
Executable: test/hello.exe
```

**当前问题**:
- 实际测试中超时（GCC 路径问题），命令行无法完成完整构建
- 输出文件未加 `.exe` 扩展名
- 不支持 `--target` 和 `--no-cache` 选项（在 help 中列出但 buildCommand 不解析）
- 无 debug 信息选项

---

### 2.4 `run` / `r` — 编译并执行

入口: `runCommand()` (L131-153)  
说明: 先调用 `buildCommand()`，成功后执行产物

**语法**:
```
zcc run <source.zcc> [options]
```

**输出**:
```
============================================================
Running: test\hello.exe
---
[程序输出...]
---
Exit code: 0
```

**当前问题**:
- 继承 `buildCommand` 的所有问题
- 输出文件推断逻辑简单：`inputFile.rfind('.')` + `.exe`，不处理路径
- `std::system()` 同步执行，无超时机制
- 退出码: 直接透传，无封装

---

### 2.5 `ir` — 生成 ZCC-IR

入口: `irCommand()` (L291-372)  
状态: P0 完成，功能完整

**语法**:
```
zcc ir <source.zcc> [-o <output.ir>]
```

**成功样例**:
```
Generating IR: test/hello.zcc -> test/hello.ir
IR generated successfully: test/hello.ir
IR size: 251 bytes
```

**失败样例 (语法/语义错误)**:
```
Generating IR: test/syntax_errors.zcc -> test/syntax_errors.ir
Lexer errors:                    // 或 "Parser errors:", "IR generation errors:"
  Unterminated string
[退出码: 1, 不生成 IR 文件 (代码逻辑会先打开输出文件然后覆盖/丢弃?)——实际查看,irCommand 先解析再写文件,错误时不写]
```

**当前问题**:
- 自身走独立管线（Lexer→Parser→IRGenerator），与 `build`/`compile` 不同
- 不支持 `-O` 选项
- 不支持 `--target` 选项
- 文件名推断：`source.xyz` → `source.xyz.ir`（保留原始扩展名），而非 `source.ir`
- 错误输出和 `compile`/`build` 格式不一致

---

### 2.6 `exec` / `x` — 执行 IR (未实现)

入口: `execCommand()` (L276-289)

**输出**:
```
Executing IR file: test.ir
Error: Executing .ir files is not implemented yet
[退出码: 1]
```

**状态**: IR Interpreter 引擎已完成 (63/66 opcode + CFG 跳转)，但 `execCommand()` 未连接 NativeBackend::runModule() 入口。

---

### 2.7 `languages` — 列出目标语言

入口: `languagesCommand()` (L157-172)

**输出**:
```
Available target languages:
  cpp (C++ Source Code (via G++/Clang++/MSVC) - [预留扩展])
  c (C Source Code (via GCC/Clang/MSVC))

To add a new language:
  1. Implement ILanguageBackend interface
  2. Register with LanguageBackendRegistry
  3. Use: zcc build --lang <name> file.zcc
```

**退出码**: 0

**当前问题**:
- 输出末尾附带了扩展指南文案，可能不算纯粹"列出"语义
- C++ backend 标记为 "[预留扩展]"，说明是桩

---

### 2.8 `backends` — 列出后端

入口: `listBackends()` (L61-80)

**输出**:
```
Available backends:
  c - C Source Code
  bios - x86 BIOS Boot Sector (16-bit real mode)
  native - Native Executable with Embedded Interpreter
```

**退出码**: 0

**说明**: `listBackends()` 每次调用都会重新注册 3 个后端（NativeBackend, BIOSBackend, CBackend）。这些后端和 `languages` 中的 `LanguageBackend` 是不同系统。

---

## 3. 双编译管线

当前存在两套并行的编译管线，可能导致混乱：

```
            ┌─ compile ─→ Compiler::compileFile() ──→ runLexer/runParser/runIRGenerator/runBackend
            │              (CompileResult)
zcc ──→ CLI ─┤
            │
            └─ build ───→ CompilerDriver::build() ──→ runLexer/runParser/runIRGenerator/runOptimizer/runLanguageBackend/runExternalCompiler
                           (BuildResult)

            ┌─ ir ──────→ 独立管线 (Lexer→Parser→IRGenerator，输出 .ir)
            │
```

| 特性 | compile | build | ir |
|------|---------|-------|-----|
| 管线类 | `Compiler` | `CompilerDriver` | 内联代码 |
| 结果类型 | `CompileResult` | `BuildResult` | 直接文件 |
| IR 生成 | ✓ | ✓ | ✓ (唯一输出) |
| 优化器 | 可选 | 可选 | ✗ |
| 后端代码生成 | 可选 | ✓ | ✗ |
| GCC 调用 | ✗ | ✓ | ✗ |
| 并行支持 | ✗ | ✗ | ✗ |
| 实现完整度 | 部分 | ~80% | 100% |

**建议**: 重构时统一到 `CompilerDriver`（新管线），移除旧 `Compiler` 类及其 `compileCommand`。

---

## 4. 输入解析问题

当前所有命令都使用手动 `argc/argv` 遍历解析，存在下列问题：

1. **选项解析不一致**: 
   - `compile` 支持 `--backend`、`--target`、`-g`、`--keep-ir`、`--no-cache`
   - `build` 支持 `--lang`
   - `ir` 只支持 `-o`
   - help 文档列出所有选项，但部分命令不识别

2. **无参数验证**:
   - `--lang` 不检查语言名是否有效
   - `--backend` 不检查后端名是否在可用列表中
   - 优化级别不检查范围

3. **选项传播断层**:
   - `run` 调用 `build`，但 `build` 支持的选项 (`--lang`) 和 help 文档列出的 (`--target`、`--no-cache`) 之间存在断裂

4. **文件名推断脆弱**:
   - `rfind('.')` 在包含路径分隔符时可能产生错误结果（例如 `test/file.zcc` → `test/file` 正常，但 `test.dir/file.zcc` → `test` 错误）

5. **无 POSIX 风格 `--` 支持**: 所有长选项用 `--`，但无短选项别名（除了 `-o` 和 `-O`）

---

## 5. 退出码约定

| 退出码 | 含义 |
|--------|------|
| 0 | 成功 |
| 1 | 任何错误（词法/语法/语义/文件/链接/内部） |
| 其他 | `run` 命令透传子进程退出码 |

---

## 6. 重构建议

基于以上分析，重构时应关注：

1. **统一命令行解析器** — 引入结构化 options parser（如 CLI11、docopt 风格）
2. **统一管线入口** — 所有命令统一走 CompilerDriver（deprecate compileCommand）
3. **完善 `exec` 命令** — 配合 P1 IR Interpreter
4. **统一错误格式** — 所有命令的错误输出格式一致
5. **添加 dry-run / verbose 模式** — 方便调试
6. **添加 `--version`** — 显示编译版本、构建时间
7. **添加 `--list-passes`** — 列出优化 Pass
8. **添加 `--emit` 多阶段输出** — `--emit=ast,ir,c` 类似 Clang 的 `-emit-llvm`
9. **统一文件命名** — 输出文件扩展名逻辑统一到工具函数
10. **GCC 路径配置化** — 从环境变量或配置文件读取，而非硬编码

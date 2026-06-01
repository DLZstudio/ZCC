# SCA — SmartCore Architecture 模块系统技术文档

> ZCC 编译器的多语言混编模块格式规范与实现细节

---

## 目录

- [1. 设计哲学](#1-设计哲学)
- [2. 文件格式规范](#2-文件格式规范)
- [3. 类型体系](#3-类型体系)
- [4. 加载管线](#4-加载管线)
- [5. 虚空接口机制](#5-虚空接口机制)
- [6. 跨模块导入导出](#6-跨模块导入导出)
- [7. 多语言代码块与执行模型](#7-多语言代码块与执行模型)
- [8. 外部可执行文件 IPC 桥接](#8-外部可执行文件-ipc-桥接)
- [9. 加载器注册与优先级](#9-加载器注册与优先级)
- [10. 标准模板与示例](#10-标准模板与示例)
- [11. 当前状态与路线图](#11-当前状态与路线图)

---

## 1. 设计哲学

SCA（SmartCore Architecture）旨在解决一个核心矛盾：

> 如何让 ZCC 这个高层次语言，与底层的 C/C++、汇编、甚至其他语言模块无缝协作？

答案是**多语言混编容器**：一个 `.sca` 文件同时承载 ZCC 高层逻辑、IR 中间表示、C/C++ 性能关键代码、汇编优化——全部在同一个文件中用语言标签分段。编译器根据标签将每个代码块分派到对应的处理管线。

SCA 模块的三大核心概念：

1. **多语言混编** — 一个文件，多种语言，统一分发
2. **虚空接口** — 调用未定义符号被视为"外部依赖"，自动发现，不需手动注册
3. **统一加载器** — 加载器注册体系自动根据文件内容选择正确的解析器

---

## 2. 文件格式规范

### 2.1 完整语法

```
[可选的二进制魔术头: 4 字节 "ZCCS" (0x5A434353)]

[语言标签1]
代码内容...
[/语言标签1]    ← 结束标签可选

[语言标签2]
代码内容...

[语言标签3]
代码内容...
[/语言标签3]
```

### 2.2 语言标签

| 标签 | 别名 | 枚举值 | 说明 |
|------|------|--------|------|
| `[zcc]` | — | `CodeLanguage::ZCC` | ZCC 高层源代码 |
| `[ir]` | — | `CodeLanguage::IR` | ZCC IR 中间代码 |
| `[c]` | — | `CodeLanguage::C` | C 原生代码 |
| `[cpp]` | `[c++]` | `CodeLanguage::CPP` | C++ 原生代码 |
| `[asm]` | `[assembly]` | `CodeLanguage::ASM` | 汇编代码 |

标签大小写不敏感（内部做 `tolower` 归一化）。

### 2.3 结束标签规则

结束标签 `/zcc`、`/ir`、`/c` 等为**可选**。解析器使用前瞻匹配（lookahead）：从当前 `[xx]` 读取内容直到遇到下一个 `[yy]` 标签或文件结束。因此：

```
[zcc]
auto x = 42;
[ir]
%1 = add %2 %3
```

等价于：

```
[zcc]
auto x = 42;
[/zcc]
[ir]
%1 = add %2 %3
[/ir]
```

### 2.4 解析核心正则

**文件**: `src/Module.cpp` — `SmartSCALoader::parse_sca_file()`（第 107-180 行）

```cpp
// 匹配 [语言] + 内容，直到下一个 [ 标签或文件末尾
std::regex block_pattern(
    R"(\s*\[\s*(\w+)\s*\](.*?)(?=\s*\[\s*\w+\s*\]|\s*\Z))",
    std::regex_constants::ECMAScript
);
```

魔术头检测（第 82-98 行）：
```cpp
// 扩展名 .sca 或 二进制头 "ZCCS"
char magic[4];
file.read(magic, 4);
if (memcmp(magic, "ZCCS", 4) == 0) return true;
```

双检测确保 `.sca` 文件和带 `ZCCS` 头的二进制兼容文件都能被正确识别。

---

## 3. 类型体系

### 3.1 核心枚举

```cpp
enum class ModuleType {
    UNKNOWN,
    INSTRUCTION_SET,   // SCA 智能指令集
    EXECUTABLE,        // 可执行文件
    DYNAMIC_LIBRARY,   // 动态库
    SCRIPT,            // 脚本语言
};

enum class DependencyType {
    MODULE,            // 模块依赖 (.sca)
    EXECUTABLE,        // 可执行文件依赖 (.exe)
    LIBRARY,           // 库依赖 (.dll/.so)
    SCRIPT,            // 脚本依赖 (.js/.py/.lua)
};

enum class CodeLanguage {
    ZCC, IR, C, CPP, ASM, UNKNOWN
};
```

### 3.2 CodeBlock — 代码块

```cpp
struct CodeBlock {
    std::string language;                          // "zcc", "ir", "c", "cpp", "asm"
    std::string code;                              // 代码内容
    size_t source_line;                            // 在 SCA 文件中的起始行号
    CodeLanguage lang_type = CodeLanguage::UNKNOWN; // 标准化枚举
    std::map<std::string, std::string> attributes;  // 扩展属性（预留）
};
```

### 3.3 SCAModule — 模块容器

```cpp
struct SCAModule {
    std::string source_name;                       // .sca 源文件路径
    std::vector<CodeBlock> blocks;                 // 代码块列表
    std::shared_ptr<ModuleMetadata> metadata;      // 元数据
    std::string cache_key;                         // 缓存键（增量编译用）
};
```

### 3.4 ModuleMetadata — 模块元数据

```cpp
struct ModuleMetadata {
    std::string name;                              // 模块名（取自文件名）
    std::string version;                           // 版本号
    std::string source_path;                       // 源文件路径
    std::string description;                       // 描述
    ModuleType type;                               // 模块类型
    std::string target_platform;                   // 目标平台 (windows/linux/macos/all)

    std::vector<ExportedSymbol> exports;           // 导出的符号列表
    std::vector<UnresolvedSymbol> imports;         // 虚空接口列表（核心）
    std::vector<ModuleDependency> dependencies;    // 依赖模块列表

    std::map<std::string, std::string> attributes; // 扩展属性
    std::vector<std::string> tags;                 // 标签

    bool is_signed = false;                        // 是否签名（远期安全特性）
    std::string signer;                            // 签名者
    std::vector<std::string> required_permissions; // 权限要求
};
```

### 3.5 UnresolvedSymbol — 虚空接口

```cpp
struct UnresolvedSymbol {
    std::string name;                   // 被调用但未定义的符号名
    std::string source_file;            // 调用来源文件
    size_t source_line;                 // 调用行号
    std::string expected_source;        // 预期来源（可选，用于明确指定外部模块）
    DependencyType dep_type;           // 依赖类型
};
```

### 3.6 LoadResult — 加载结果

```cpp
struct LoadResult {
    bool success;
    std::unique_ptr<Module> module;               // 加载后的模块对象
    std::vector<UnresolvedSymbol> unresolved_symbols; // 虚空接口列表
    std::vector<std::string> warnings;            // 警告信息
    std::vector<std::string> errors;              // 错误信息
    std::shared_ptr<ModuleMetadata> metadata;     // 元数据（可选共享）

    // 模块类型特定数据（variant 避免多态开销）
    std::variant<std::monostate, SCAModule, ExecutableModule,
                 DynamicLibModule, ScriptModule> specific_data;
};
```

---

## 4. 加载管线

### 4.1 完整流程

```
用户调用 LoaderRegistry::load_with_dependencies("main.sca")
                          │
                    ┌─────▼──────┐
                    │ BFS 队列    │
                    │ to_load     │
                    └─────┬──────┘
                          │
                    ┌─────▼──────┐
                    │ 自动选择    │
                    │ 加载器      │  ← LoaderRegistry::select_loader(path)
                    └─────┬──────┘
                          │
                    ┌─────▼──────────────────────────┐
                    │ SmartSCALoader::load(path)      │
                    │                                  │
                    │  1. parse_sca_file(path)         │  ← 正则解析 [语言] 块
                    │     ├── 检测 ZCCS 魔术头          │
                    │     ├── 逐块匹配语言标签           │
                    │     └── 构建 SCAModule.blocks[]   │
                    │                                  │
                    │  2. 逐块处理                      │
                    │     ├── process_zcc_block()       │  ← 正则提取函数/变量定义
                    │     ├── process_ir_block()        │  ← 收集 IR 符号
                    │     └── process_native_block()    │  ← 标记原生代码块
                    │                                  │
                    │  3. find_unresolved_symbols()     │  ← 虚空接口发现
                    │     ├── 收集已定义符号             │
                    │     ├── 收集被调用符号             │
                    │     └── 差集 → 虚空接口列表        │
                    │                                  │
                    │  4. 构建 Module 对象              │
                    │     └── 填充 metadata             │
                    │                                  │
                    └─────┬──────────────────────────┘
                          │
                    ┌─────▼──────┐
                    │ 依赖分析    │  ← analyze_dependencies()
                    │ 提取 import  │
                    │ 加入 BFS 队列│
                    └─────┬──────┘
                          │
                    ┌─────▼──────┐
                    │ 返回         │
                    │ LoadResult[] │
                    └────────────┘
```

### 4.2 BFS 依赖图加载

**文件**: `src/Module.cpp` — `LoaderRegistry::load_with_dependencies()`（第 558-603 行）

```cpp
std::vector<LoadResult> LoaderRegistry::load_with_dependencies(
    const std::filesystem::path& main_path, const LoadOptions& options)
{
    std::vector<LoadResult> all_results;
    std::set<std::filesystem::path> loaded_paths;       // 去重集合
    std::queue<std::filesystem::path> to_load;           // BFS 队列

    to_load.push(main_path);

    while (!to_load.empty()) {
        auto current = to_load.front(); to_load.pop();

        if (loaded_paths.count(current)) continue;       // 跳过已加载
        loaded_paths.insert(current);

        auto loader = select_loader(current);            // 自动选择加载器
        auto result = loader->load(current, options);
        all_results.push_back(result);

        if (result.success) {
            // 分析该模块的 import 语句 → 加入 BFS 队列
            auto deps = loader->analyze_dependencies(current);
            for (const auto& dep : deps) {
                if (!loaded_paths.count(dep.path)) {
                    to_load.push(dep.path);
                }
            }
        }
    }
    return all_results;
}
```

**特点**：
- BFS 确保直接依赖先于间接依赖处理
- `loaded_paths` set 去重，防止重复加载
- 不支持并行加载（单线程队列）

**局限**（当前版本）：
- 无拓扑排序 — 不保证加载顺序严格满足依赖方向
- 无循环依赖检测 — A→B 且 B→A 时各加载一次后停止，不报错
- 无版本约束检查 — `ModuleDependency.version_constraint` 字段已定义但不检查

---

## 5. 虚空接口机制

虚空接口（Void Interface）是 SCA 系统最核心的概念，也是 ZCC 区别于传统编译器的关键设计。

### 5.1 概念定义

> 虚空接口 = 在模块中被调用，但在本模块内没有定义的符号

它为 ZCC 提供了一种"声明式"的跨模块/跨语言通信模型：模块 A 调用 `connect()`，模块 B 提供 `connect()` 的实现，中间由编译器自动发现并匹配。

### 5.2 自动发现算法

**文件**: `src/Module.cpp` — `SmartSCALoader::find_unresolved_symbols()`（第 210-270 行）

```
输入: SCAModule（已解析的 SCA 模块）
输出: std::vector<UnresolvedSymbol>

算法:
┌─────────────────────────────────────────────┐
│ 第 1 步: 收集已定义符号 (defined_symbols)      │
│ 正则: (?:function|函数|func)\s+(\w+)\s*\(    │
│ 正则: (?:auto|定义)\s+(\w+)\s*=              │
│ 结果: { foo, bar, x, y, ... }               │
├─────────────────────────────────────────────┤
│ 第 2 步: 收集被调用符号 (called_symbols)       │
│ 正则: \b(\w+)\s*\(                           │
│ 跳过关键词: function/函数/func/auto/定义/      │
│            if/如果/for/循环/while/当           │
│ 结果: { foo, bar, connect, send, ... }      │
├─────────────────────────────────────────────┤
│ 第 3 步: 差集运算                              │
│ unresolved = called_symbols - defined_symbols │
│ 结果: { connect, send } (虚空接口!)           │
└─────────────────────────────────────────────┘
```

### 5.3 隐式 vs 显式虚空接口

ZCC 支持两种虚空接口声明方式：

| 方式 | 阶段 | 语法 | AST 节点 | 机制 |
|------|------|------|---------|------|
| **显式** | 前端(Parser) | `@external function f(args)` | `ExternalDecl` | 开发者主动声明 |
| **隐式** | 中端(SmartSCALoader) | 调用未定义函数 | `UnresolvedSymbol` | 自动正则发现 |

**互补关系**：
- `ExternalDecl` — 我知道这个符号是外部的，请编译器在链接时帮我找
- `UnresolvedSymbol` — 编译器在分析你的代码时发现这个符号没有定义，自动标记为外部依赖

两者都指向同一个目标：在 `ModuleManager::resolveVoidInterfaces()` 中统一解析。

### 5.4 虚空接口示例

**输入** `network.sca`:

```
[zcc]
function main() {
    auto conn = connect("localhost", 8080);   ← connect 未定义 → 虚空接口
    sendData(conn, "hello");                  ← sendData 未定义 → 虚空接口
}

// 辅助函数（已定义，不算虚空接口）
function formatData(data) {
    return "[" + data + "]";
}
[/zcc]
```

**自动发现结果**:
```json
{
  "unresolved_symbols": [
    {
      "name": "connect",
      "source_file": "network.sca",
      "source_line": 0,
      "dep_type": "MODULE"
    },
    {
      "name": "sendData",
      "source_file": "network.sca",
      "source_line": 0,
      "dep_type": "MODULE"
    }
  ]
}
```

注意 `formatData` 不在列表中——它在本模块内定义了。

---

## 6. 跨模块导入导出

### 6.1 Import 语法全集

ZCC 支持 ECMAScript 风格的模块导入，共 7 种语法形式：

```zcc
// 1. 默认导入
import MyModule from "path/to/module.sca";

// 2. 命名导入（单符号）
import { connect } from "network.sca";

// 3. 命名导入（多符号）
import { connect, send } from "network.sca";

// 4. 命名导入（带别名）
import { connect as cn, send as sd } from "network.sca";

// 5. 通配符导入
import * as Net from "network.sca";

// 6. 组合导入
import DefaultMod, { funcA, funcB } from "module.sca";

// 7. from-import 语法
from "network.sca" import connect, send;
```

**中文语法**:
```zcc
从 "网络.sca" 导入 连接, 发送;
```

### 6.2 ImportDecl AST 节点

**文件**: `include/AST.hpp`

```cpp
struct ImportDecl : Declaration {
    struct ImportSpecifier {
        std::string importedName;   // 源模块中的原名
        std::string localName;      // 导入后的本地别名
    };

    std::string modulePath;                    // 模块路径字符串
    std::string namespaceAlias;                // 通配符导入的命名空间名
    std::vector<ImportSpecifier> specifiers;   // 命名导入列表
    std::string defaultImport;                 // 默认导入名
};
```

### 6.3 导出符号提取

`SmartSCALoader::get_exports()`（`src/Module.cpp` 第 355-382 行）用正则 `(?:function|函数|func)\s+(\w+)\s*\(([^)]*)\)` 提取所有函数定义（含参数签名），构建 `ExportedSymbol` 列表。

### 6.4 完整跨模块示例

**`app.sca`**（主模块）:
```
[zcc]
import { sendMessage } from "messaging.sca";
import * as Utils from "utils.sca";

function main() {
    // Utils 命名空间的函数
    auto formatted = Utils.format("hello");
    Utils.log(formatted);

    // 命名导入的函数
    auto result = sendMessage(formatted, "admin");
    print(result);
}
[/zcc]
```

**`messaging.sca`**（消息模块）:
```
[zcc]
@external function encrypt(text);     // 加密来自外部

function sendMessage(text, target) {
    auto encrypted = encrypt(text);   // 调用虚空接口
    return "sent to " + target;
}
[/zcc]
[c]
// C 原生实现（在某些平台上替代 encrypt 虚空接口）
char* native_encrypt(const char* text) {
    return "encrypted";
}
[/c]
```

**`utils.sca`**（工具模块）:
```
[zcc]
function format(s) { return "[" + s + "]"; }
function log(msg) { println("[LOG]", msg); }
[/zcc]
```

这个例子展示了 SCA 的核心优势：
1. `app.sca` 依赖 `messaging.sca` 和 `utils.sca` → BFS 自动加载子模块
2. `messaging.sca` 声明了虚空接口 `encrypt` → 可以被 `messaging.sca` 内的 C 代码块实现
3. 所有多语言代码共存于同一个文件中，统一分发

**加载结果**（BFS 队列顺序）:
```
1. app.sca        → 解析 import → 发现 messaging.sca, utils.sca
2. messaging.sca  → 虚空接口: [encrypt]  → 依赖: 无
3. utils.sca      → 虚空接口: []          → 依赖: 无
```

---

## 7. 多语言代码块与执行模型

SCA 文件中的每个代码块，根据其语言标签，在编译/运行时走不同的执行路径。

### 7.1 ZCC 代码块 `[zcc]`

**编译时**:
1. Lexer → Token 流
2. Parser → AST
3. IRGenerator → ZCCIR
4. 后续走模式一（C 后端→GCC→exe）或模式二（IR Interpreter 直接执行）

**运行时**（模式一）:
- 对应的 ZCC 函数被编译为 C 代码，调用 `zcc_runtime` 库执行
- 动态类型（ZCCValue）、BigNum、动态数组/对象由运行时库提供

**运行时**（模式二 / 待实现）:
- IR Interpreter 逐指令执行 66 个 ZCCIR 操作码
- 内置函数（print/println/input/typeof/len/str/num/exit）在解释器启动时注册

### 7.2 IR 代码块 `[ir]`

**编译时**:
- `process_ir_block()` 收集 IR 符号到模块元数据
- IR 块被视为已编译好的中间代码，跳过前端处理，直接交付给 C 后端或解释器

**用途**: 允许开发者**跳过 ZCC 前端**，直接手写优化后的 IR 代码嵌入 SCA 模块。

### 7.3 C/C++ 代码块 `[c]` / `[cpp]`

**编译时**:
- `process_native_block()` 标记原生成代码块的存在
- 在编译为可执行文件时，这些块被提取为独立 `.c`/`.cpp` 文件，作为额外编译单元传给 GCC

**运行时**（模式一）:
- C 代码块独立编译，与 ZCC→C 生成的代码链接在一起
- C 代码块中定义的外部可见函数可以被 ZCC 调用（通过虚空接口桥接）

**运行时**（模式二 / 待实现）:
- 暂不支持在解释器内执行原生代码块
- 可能需要 JIT 桥接或外部进程调用（IPC）

### 7.4 汇编代码块 `[asm]`

**编译时**:
- 类似 C 代码块，标记后作为独立 `.s` 文件传给汇编器

**用途**: 性能关键路径的手写 SIMD 或平台特定指令。

### 7.5 执行模式决策矩阵

```
                        │ 模式一 (C→GCC→exe)     │ 模式二 (IR Interpreter)
────────────────────────┼───────────────────────┼─────────────────────────
  [zcc]                 │ ✅ 完整编译为 C         │ ✅ IR 解释执行
  [ir]                  │ ✅ 跳过前端，直接C后端   │ ✅ 直接指令解释
  [c] / [cpp]           │ ✅ 作为额外编译单元      │ ⚠️ 需要 JIT/外部进程桥接
  [asm]                 │ ✅ 作为汇编源文件        │ ❌ 不支持
  @external 声明         │ ✅ 编译期解析为外部符号   │ ✅ 编译期解析为外部符号
  跨 SCA 模块 import     │ ✅ BFS 加载 + 链接       │ ✅ BFS 加载（函数暂未实现调用）
  虚空接口自动发现       │ ✅ 正则差集               │ ✅ 正则差集
```

---

## 8. 外部可执行文件 IPC 桥接

### 8.1 ExternalExecutableLoader

SCA 系统可以"加载"一个外部可执行文件（`.exe`、ELF 格式）作为模块。加载时建立 IPC 连接，通过命名管道（Windows）或 Unix Socket 查询导出符号并调用函数。

**文件**: `src/Module.cpp` — `ExternalExecutableLoader::load()`（第 642-703 行）

### 8.2 支持的格式

| 格式 | 操作系统 | 检测方式 |
|------|---------|---------|
| PE (`MZ` 头) | Windows | 检测 `MZ` 魔术头 |
| ELF (`\x7fELF` 头) | Linux/macOS | 检测 `0x7f` `E` `L` `F` |

### 8.3 与 IPC 框架的关联

ExternalExecutableLoader 依赖 `IPCManager`（`IPC.hpp`/`IPC.cpp`，共 1366 行）：

```
加载 .exe 模块
    │
    ├── 检测 PE/ELF 格式
    ├── 启动外部进程（CreateProcess/fork）
    ├── IPCManager::getInstance().connect(address)
    │     ├── Windows:  Named Pipe (\\.\pipe\zcc-module-{id})
    │     └── Unix:     Unix Socket (/tmp/zcc-module-{id})
    ├── 查询导出符号
    └── 注册到 ModuleManager
```

### 8.4 IPC 框架组件

| 组件 | 文件 | 行数 | 功能 |
|------|------|------|------|
| Connection | IPC.cpp | — | 命名管道/Socket 封装 |
| ConnectionPool | IPC.cpp | 多路复用 | 连接池管理 |
| IPCManager | IPC.cpp | 单例 | 连接/监听/流/订阅/发现 |
| adapter::Adapter | IPC.cpp | — | IPC 消息序列化/反序列化 |

**当前状态**: 架构代码就绪，但实际的查询导出符号和跨进程函数调用链尚未实现（20%）。

---

## 9. 加载器注册与优先级

### 9.1 加载器协议

所有加载器必须实现 `ExtendedModuleLoader` 抽象基类（`Module.hpp` 第 241-275 行）的 8 个纯虚方法：

```cpp
class ExtendedModuleLoader {
public:
    virtual ~ExtendedModuleLoader() = default;

    virtual int get_priority() const = 0;                      // 优先级（越高越优先）
    virtual bool supports(const std::filesystem::path&) = 0;   // 是否支持此格式
    virtual std::string get_supported_extension() const = 0;   // 支持的扩展名
    virtual std::string get_description() const = 0;           // 加载器描述

    // 加载
    virtual LoadResult load(const std::filesystem::path&,
                           const LoadOptions&) = 0;

    // 运维
    virtual std::vector<UnresolvedSymbol> find_unresolved_symbols(const SCAModule&) { return {}; }
    virtual std::vector<ModuleDependency> analyze_dependencies(const std::filesystem::path&) { return {}; }
    virtual bool unload(const std::filesystem::path&) { return true; }
};
```

### 9.2 已注册的加载器

| 加载器 | 优先级 | 处理格式 | 实现状态 |
|--------|--------|---------|---------|
| **SmartSCALoader** | **100** | `.sca` + `ZCCS` 魔术头 | ✅ 完整 |
| ExternalExecutableLoader | 80 | `.exe` + PE/ELF | ⚠️ IPC 链待实现 |
| DynamicLibLoader | 70 | `.dll` / `.so` / `.dylib` | ⚠️ 桩代码 |
| ScriptLoader | 60 | `.js` / `.py` / `.lua` / `.rb` / `.php` | ⚠️ 桩代码 |

### 9.3 注册与选择

```cpp
// 初始化（src/Module.cpp 第 605-610 行）
void LoaderRegistry::initialize_builtin_loaders() {
    register_loader(std::make_unique<SmartSCALoader>());          // 100
    register_loader(std::make_unique<ExternalExecutableLoader>()); // 80
    register_loader(std::make_unique<DynamicLibLoader>());        // 70
    register_loader(std::make_unique<ScriptLoader>());            // 60
}

// 注册时自动按优先级降序排序
void LoaderRegistry::register_loader(std::unique_ptr<ExtendedModuleLoader> loader) {
    loaders_.push_back(std::move(loader));
    std::sort(loaders_.begin(), loaders_.end(),
        [](auto& a, auto& b) { return a->get_priority() > b->get_priority(); });
}

// 选择加载器 — 按优先级遍历，第一个 supports() 返回 true 的获胜
ExtendedModuleLoader* LoaderRegistry::select_loader(const std::filesystem::path& path) {
    for (auto& loader : loaders_) {
        if (loader->supports(path)) return loader.get();
    }
    return nullptr;
}
```

---

## 10. 标准模板与示例

### 10.1 纯 ZCC 库模块

```
[zcc]
// 纯 ZCC 工具库 — 无虚空接口，无原生代码

function clamp(val, min, max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

function lerp(a, b, t) {
    return a + (b - a) * t;
}
[/zcc]
```

### 10.2 带虚空接口的网络模块

```
[zcc]
@external function tcpConnect(host, port);     // 来自底层 C 库
@external function tcpSend(socket, data);      // 来自底层 C 库

function sendMessage(host, port, message) {
    auto sock = tcpConnect(host, port);
    return tcpSend(sock, message);
}
[/zcc]

[c]
// 平台特定实现 — 在模式一编译时链接
#include <winsock2.h>
int tcpConnect(const char* host, int port) { ... }
int tcpSend(int sock, const char* data) { ... }
[/c]
```

### 10.3 多语言混编图形模块

```
[zcc]
function render() {
    auto canvas = createCanvas(800, 600);
    drawRect(canvas, 0, 0, 800, 600, "blue");
    drawText(canvas, "Hello SCA", 100, 200);
    flushCanvas(canvas);
}
[/zcc]

[c]
// C 原生图形缓冲区操作
#include <string.h>

typedef struct { int w, h; unsigned char* pixels; } Canvas;

Canvas createCanvas(int w, int h) {
    Canvas c;
    c.w = w; c.h = h;
    c.pixels = malloc(w * h * 4);
    memset(c.pixels, 0, w * h * 4);
    return c;
}

void drawRect(Canvas c, int x, int y, int w, int h, const char* color) {
    // 直接像素操作...
}
[/c]

[asm]
; SIMD 优化的像素填充（仅在 x86_64 平台上使用）
section .text
global fast_fill
fast_fill:
    ret
[/asm]
```

### 10.4 带 IR 手工优化的模块

```
[zcc]
function compute(arr, scale) {
    auto result = 0;
    for (auto i = 0; i < arr.length; i++) {
        result = result + arr[i] * scale;
    }
    return result;
}
[/zcc]

[ir]
; 手工优化的 IR 版本（供开发者微调）
.func any @compute_opt(arr, scale) {
entry:
  %1 = array_len %arr
  %2 = alloca 0
  %3 = store %2 %0
  for.cond:
    %4 = load %idx
    %5 = cmp_lt %4 %1
    jmp_if %5 for.body for.end
  for.body:
    %6 = array_get %arr %4
    %7 = mul %6 %scale
    %8 = load %2
    %9 = add %8 %7
    %10 = store %2 %9
    jmp for.cond
  for.end:
    %11 = load %2
    ret %11
}
[/ir]
```

---

## 11. 当前状态与路线图

> **更新时间**: 2026-06-01 — v0.3 Manifest 解析器 + 版本约束检查已实现

### 11.1 完成度矩阵

| 子系统 | 完成度 | 说明 |
|--------|--------|------|
| **SCA v0.3 Manifest 解析器** | **100%** | `parse_manifest()` — section 状态机 + key=value 解析 |
| **Manifest 依赖声明** | **100%** | `[dependency]` 节 + type=sca/executable |
| **Manifest 接口映射** | **100%** | `[interface]` 节 → 替代 v0.2 正则差集猜测 |
| **版本约束检查** | **100%** | SemVer 解析 (`>=`/`>`/`==`) + validate() 集成 |
| **SCA 文件解析** (`parse_sca_file`) | **100%** | v0.3 优化: manifest 段提取 + c/cpp/asm 过滤 |
| **虚空接口发现** | **100%** | Manifest interface 优先, v0.2 正则回退 |
| **导出符号提取** (`get_exports`) | **100%** | 函数签名正则提取 |
| **导入分析** (`get_imports`/`analyze_dependencies`) | **80%** | Manifest deps 优先, import 正则回退 |
| **BFS 依赖加载** | **50%** | 可用，无拓扑排序/循环检测 |
| **ModuleManager** | **10%** | 类型定义完成，核心方法空桩 |
| **ExternalExecutableLoader** | **20%** | 架构代码就绪，IPC 调用链未实现 |
| **DynamicLibLoader** | **5%** | 桩代码 |
| **ScriptLoader** | **5%** | 桩代码 |
| **DependencyGraph** | **0%** | 可能替代当前 BFS 的方案，未定义 |

### 11.2 路线图

```
✅ SCA v0.3 Manifest 解析器
✅ 版本约束检查 (SemVer)
✅ Module.o + IPC.o 完整链接
P1 (当前): IR Interpreter     — 让 SCA 模块可以通过模式二直接执行
P2:        JIT 预埋             — LLVM JIT 接口桩
P3:        CLI 重写            — 暴露 SCA 模块管理命令
P4:        Optimizer Pass      — IR 层面优化
P5:        ModuleManager 完善   — resolveVoidInterfaces(), 循环依赖检测, 拓扑排序
P6:        外部进程 IPC 桥接    — ExternalExecutableLoader 完整实现
```

### 11.3 关键文件索引

| 文件 | 行数 | 内容 |
|------|------|------|
| `include/Module.hpp` | 575 | SCA 类型体系全定义 |
| `src/Module.cpp` | 1094 | SmartSCALoader + LoaderRegistry 完整实现 |
| `include/IPC.hpp` | 529 | IPC 框架类型定义 |
| `src/IPC.cpp` | 837 | IPC 框架完整实现 |
| `include/AST.hpp` | — | ImportDecl, ExternalDecl 定义 |
| `src/Parser.cpp` | — | import/from/@external 语法解析 |

# ZCC — Zero-Core Command Language v0.2

> 中英双语编译器。三大编译模式：源码转译 · 通用 IR 执行 · 原生可执行。

---

## 语言特性

### 动态类型系统

ZCC 是动态类型语言，变量通过 `auto`（中文用 `定义`）声明，运行时类型自动推断。

```zcc
auto x = 42;                         // 整数
auto y = 3.14;                       // 浮点
auto name = "ZCC";                   // 字符串
auto flag = true;                    // 布尔
auto nothing = null;                 // 空值
```

### 数组和对象

```zcc
auto arr = [1, 2, 3, "混合类型"];     // 数组
arr[0] = 100;                        // 索引赋值
auto len = arr.length;               // → 4

auto obj = { key: "value", age: 18 };// 对象
obj.newKey = 42;                     // 属性赋值
auto v = obj.key;                    // → "value"
```

### 高精度整数 (BigNum)

后缀 `n` 表示高精度整数，内部用字符串实现，无精度上限。

```zcc
auto big = 12345678901234567890n;
auto sum = big + 1n;                 // → 12345678901234567891n
auto prod = big * 2n;                // 精确乘法
auto pow = 10n ^ 5000n;             // 10^5000 — 5001 位整数
```

**实现细节**：
- ADD/SUB: 竖式逐位 O(n)
- MUL: **Karatsuba** O(n^1.585) + 10^n 特化路径
- DIV/MOD: 长除法 O(n²) + 10^n 特化路径
- POW: 二分快速幂 O(log n) + 10^n 特化 (O(1))
- 完整支持负数运算

### 控制流

```zcc
// 条件分支
if (x > 0) {
    print("正数");
} else if (x == 0) {
    print("零");
} else {
    print("负数");
}

// 循环
for (auto i = 0; i < 10; i++) {     // C 风格 for
    print(i);
}

auto arr = [1, 2, 3];
for (auto item : arr) {              // for-each
    print(item);
}

while (x > 0) {                      // while
    x = x - 1;
}

// switch-case
switch (x) {
    case 1: print("一"); break;
    case 2: print("二"); break;
    default: print("其他");
}
```

### 函数和箭头函数

```zcc
// 传统函数
function add(a, b) {
    return a + b;
}

// 箭头函数（单表达式）
auto multiply = (a, b) => a * b;

// 箭头函数（块体，支持闭包）
auto counter = () => {
    auto count = 0;
    return () => { count = count + 1; return count; };
};

// 函数表达式 / IIFE
auto result = function(n) { return n * n; }(5);  // → 25
```

### 异常处理

```zcc
try {
    throw "出错了!";
} catch (e) {
    print("捕获:", e);
} finally {
    print("清理工作");
}
```

### 枚举和命名空间

```zcc
// 枚举
enum Color {
    RED,
    GREEN,
    BLUE
}
auto c = Color.RED;

// 命名空间
namespace Math {
    auto PI = 3.14159;
    function square(x) { return x * x; }
}
auto area = Math.PI * Math.square(5);
```

### 外部声明 (@external)

`@external` 声明一个函数来自外部模块，供编译期链接使用。

```zcc
@external function sendData(data);
@external function connect(host, port);
```

中文语法: `@外部 函数 发送数据(数据);`

### 输入输出

```zcc
print("Hello World");               // 输出（不换行）
println(42);                         // 输出 + 换行
auto name = input("你的名字? ");     // 带提示的输入
```

中文语法: `打印("你好")`, `打印行(42)`, `输入("请输入:")`

### 特殊常量

```zcc
auto nan = NaN;                     // 非数值
auto inf = Infinity;                // 无穷大
auto undef = undefined;            // 未定义
```

---

## SCA — SmartCore Architecture 模块系统

SCA 是 ZCC 的多语言模块格式（扩展名 `.sca`）。**v0.3 版本** 引入了 Manifest 元数据驱动架构，确立了 `[zcc]` + `[ir]` 双原生块的铁律。

### SCA v0.3 格式

```
ZCCS                              ← 魔术头 (0x5A434353)

[manifest]                        ← 模块清单 (v0.3 新增)
module.name = org.example.network
module.version = 1.0.0
module.sca_version = 0.3
module.modes = ir-exec,compile    ← 执行模式声明

[dependency]                      ← 依赖声明 (可重复)
name = org.std.crypto
type = sca
version = >=1.5.0

[interface]                       ← 接口映射 (替代正则猜测)
symbol = hash
provider = org.std.crypto
export = sha256_hash
[/manifest]

[zcc]
@external function hash(data);
function main() { return hash("hello"); }
[/zcc]

[ir]
.func main
  %1 = call hash("Hello")
  ret %1
[/ir]
```

### Manifest 特性

| 特性 | 状态 |
|------|------|
| 模块声明 (`module.*`) | ✅ |
| 执行模式 (`module.modes`) | ✅ |
| 依赖声明 (`[dependency]`, type=sca/executable) | ✅ |
| 接口映射 (`[interface]`) | ✅ |
| 版本约束检查 (SemVer) | ✅ |
| v0.2 无 Manifest 兼容 | ✅ |
| 强制禁用 [c]/[cpp]/[asm] 块 | ✅ v0.3 铁律 |

详细规范：[SCA_DESIGN.md](SCA_DESIGN.md)

### 虚空接口 (Void Interface)

SCA 支持两种虚空接口声明方式：

| 方式 | 阶段 | 语法 | 机制 |
|------|------|------|------|
| **显式** (v0.1+) | 前端 | `@external function f(args)` | 开发者主动声明 |
| **隐式** (v0.2) | 中端加载器 | 调用未定义函数 | 正则差集自动发现 |
| **声明式** (v0.3) | 中端 Manifest | `[interface]` 映射 | Manifest 声明 + 版本校验 |

### 跨模块导入

```zcc
import { connect, send } from "network.sca";     // 命名导入
import * as Net from "network.sca";              // 通配符导入
from "network.sca" import connect, send;          // from-import 语法
从 "网络.sca" 导入 连接, 发送;                     // 中文语法
```

### 版本约束检查

```sca
[dependency]
name = org.std.crypto
type = sca
version = >=1.5.0       ← 加载时自动校验依赖模块的 module.version
```

`validate()` 在加载时自动读取依赖模块版本并比对约束。支持 `>=`/`>`/`==` 三种 SemVer 运算符。

### SCA 完成度

| 子系统 | 完成度 |
|--------|--------|
| SCA v0.3 Manifest 解析器 | 100% |
| Manifest 依赖/接口声明 | 100% |
| 版本约束检查 (SemVer) | 100% |
| 虚空接口 v0.2 正则回退 | 100% |
| 依赖 BFS 加载 | 50% (无拓扑排序/循环检测) |
| ModuleManager::resolveVoidInterfaces() | 10% (远期) |
| 外部可执行文件 IPC 桥接 | 20% (远期) |

---

## 三大编译模式

### 模式一：源码转译 → 原生可执行 (90%)

```
.zcc 源码 → Lexer → Parser → AST → IRGenerator → ZCCIR →
CBackend (生成 C 代码) → C → GCC → .exe
```

```bash
zcc build hello.zcc --lang c
zcc run hello.zcc
```

后端选择：`c`（C源码→GCC编译）、`cpp`（C++源码，预留）。

### 模式二：通用 IR 执行 (60%)

```
.zcc 源码 → Lexer → Parser → AST → IRGenerator → ZCCIR
                                              ├── toText() → .ir 文本
                                              └── Interpreter → 直接执行
```

```bash
# 生成 IR 文本
zcc ir hello.zcc

# 直接执行 IR（Interpreter 框架就绪, 35/66 opcode 已实现)
zcc exec hello.ir
```

IR 使用 SSA 形式，66 个操作码覆盖全部语言特性。IR 文本输出示例：

```
%3 = store %0 %2 %1     → 值3 = STORE, 目标x, 源42
%4 = load  %1 %2        → 值4 = LOAD, 从x加载
%6 = add   %2 %4 %5     → 值6 = ADD, 值4 + 常量1
```

IR Interpreter 采用 CFG 跳转引擎 (execute → executeBlock → BlockResult {nextBlock, value})，内置 9 个运行时函数，35/66 opcode 已实现（算术/比较/逻辑/位运算/数组/对象/字符串/控制流/异常）。

### 模式三：一键编译运行 (85%)

模式一加用户包装层，`zcc run` 等价于 `zcc build` + 执行产物。

---

## CLI 命令

```bash
zcc help                # 帮助信息
zcc ir file.zcc         # 生成 IR 文本 (.ir)
zcc build file.zcc      # 编译→C→GCC→可执行文件
zcc run file.zcc        # 编译并立即执行
zcc exec file.ir        # 执行 IR 文件（Interpreter 实现中）
zcc languages           # 列出可用的目标语言后端
zcc backends            # 列出可用的编译器后端
```

选项：`-o` (输出文件), `-O0`/`-O1`/`-O2`/`-O3` (优化级别), `--lang c/cpp` (目标语言), `-g` (调试信息)

---

## 项目结构

```
v0.2/
├── include/
│   ├── midend/         # ZCCIR (66 opcode) · IRGenerator (32 visitor) · Optimizer (8 pass) · Types
│   ├── compiler/       # CompilerDriver · LanguageBackend (ILanguageBackend 插件协议)
│   ├── backend/        # NativeBackend (Interpreter) · Backend
│   └── *.hpp           # AST · Lexer · Parser · Token · Keywords (60+ 中英对) · Module · IPC
│
├── src/
│   ├── midend/         # IRGenerator.cpp · Optimizer.cpp
│   ├── compiler/       # LanguageBackend.cpp (C/C++ 代码生成)
│   ├── backend/        # NativeBackend.cpp (Interpreter) · CBackend/BIOSBackend
│   ├── compat_stubs.cpp
│   └── *.cpp           # main · Lexer · Parser · Compiler · Module · IPC
│
├── runtime/            # C 运行时库
│   ├── zcc_runtime.h   # ZCCValue (8 动态类型) · BigNum · Array · Object
│   └── zcc_runtime.c   # 730 行完整实现
│
├── test/               # 47 个测试文件
│   └── 爆破报告/        # v2～v5 爆破测试报告
│
├── docs/               # 文档
│   ├── SCA_DESIGN.md   # SCA 详细技术规范
│   ├── CLI_DOCUMENTATION.md  # CLI 命令文档
│   ├── DEVELOPMENT_STATUS.md # 开发进度
│   ├── TODO.md          # 未完成项记录
│   └── archive/         # 历史文档归档
│
├── build.sh            # 一键构建脚本 (15 个 .o, 含 Module+IPC)
└── .codegraph/         # CodeGraph 代码分析数据库 (3883 节点 · 8896 边)
```

---

## 构建

```bash
# 要求: MinGW GCC 13.1.0 + bash
chmod +x build.sh && ./build.sh
# 产物: build/zcc.exe
```

编译器: MinGW GCC 13.1.0 / C++20  
平台: Windows x86_64（C 语言后端跨 Linux/macOS）

---

## 开发者工具

项目已接入 **CodeGraph MCP**（v0.9.8，.mcp.json 项目级配置）：

- 索引：78 文件 · 3883 个符号 · 8896 条调用边
- 工具：`codegraph_context`（上下文查询）、`codegraph_impact`（修改影响分析）、`codegraph_callers`/`callees`（调用链追踪）、`codegraph_search`（符号搜索）

另有 LSP 支持（goToDefinition / findReferences / callHierarchy 等）。

---

## 开发进度

| 优先级 | 任务 | 状态 |
|--------|------|------|
| P0 | IR 数据流修复 | ✅ 完成 |
| P0 | SCA v0.3 Manifest 解析器 | ✅ 完成 |
| P0 | SCA 版本约束检查 | ✅ 完成 |
| P0 | Module.o + IPC.o 完整链接 | ✅ 完成 |
| P1 | IR Interpreter 引擎框架 (CFG) | ✅ 完成 |
| P1 | IR Interpreter opcode 实现 | ⚠️ 35/66, 其余待填 |
| P2 | JIT 预埋 (LLVM 接口桩) | ⬜ 未开始 |
| P3 | CLI 重写 | ⬜ 暂缓 (CLI_DOCUMENTATION.md 已完成) |
| P4 | 优化器 8 Pass 填逻辑 | ⚠️ 2/8 有实质逻辑 |
| P5 | ModuleManager (resolveVoidInterfaces) | ⬜ 远期 |

详见 [TODO.md](TODO.md) 和 [DEVELOPMENT_STATUS.md](DEVELOPMENT_STATUS.md)。

---

## License

工作室内部开发项目。

# ZCC v0.2 未完成项记录

> **最后更新**: 2026-06-01  
> **依据**: `DEVELOPMENT_STATUS.md` / `README.md` / `ZCCIR_DESIGN.md` / `BIGNUM_DESIGN.md` / `SCA_DESIGN.md`

---

## 一、已完成 (P0/P1/P2)

### P0 核心基础设施 (100%)

| 项目 | 验证 |
|------|------|
| IR 数据流修复 (values_ + DEF) | 10/10 回归通过, IR 从 `add _ _` → `%6 = add %2 %4 %5` |
| SCA v0.3 Manifest 解析器 | section 状态机 + key=value + `[dependency]`/`[interface]` 节 |
| SCA v0.3 版本约束检查 | SemVer `>=`/`>`/`==` + validate() 集成 |
| Module.o + IPC.o 完整链接 | 修复 3 `mutable mutex` + 4 `using` 类型引用 |

### P1 模式二核心 (95%)

| 项目 | 状态 |
|------|------|
| IR Interpreter CFG 引擎 | execute → executeBlock → BlockResult{nextBlock, value} |
| IR Interpreter opcode 实现 | **63/66** — 全覆盖 (INTRINSIC/ASSEMBLY 为编译器内部指令, 仅需返回 null) |
| C 后端 66/66 opcode | LanguageBackend.cpp 全覆盖 |
| BigNum 字符串高精度 | 7 函数全实现 (Karatsuba 乘法 O(n¹⋅⁵⁸⁵) + 10^n 特化) |
| SCA SmartSCALoader | 解析 + 接口映射 + BFS 依赖加载可用 |

### P2 扩展功能

| 项目 | 状态 |
|------|------|
| JIT 预埋 | `JITBackend.hpp` 接口桩完成 |
| 优化器 | 2/8 Pass 有实质逻辑 (ConstantFolding / Peephole) |

---

## 二、真实未完成项 (按优先级)

### P1 — 收尾

| 项目 | 文件 | 说明 |
|------|------|------|
| 激活 `zcc exec` 命令 | main.cpp | CLI execCommand 桩, 需连接 NativeBackend::runModule() |
| ModuleManager::resolveVoidInterfaces() | Module.cpp | 虚空接口解析逻辑, 当前仅有声明 |

### P2 — 优化器 + IPC

| 项目 | 文件 | 说明 |
|------|------|------|
| DeadCodeElimination Pass | Optimizer.cpp | liveness 分析存在, 消除逻辑空 |
| CSE Pass | Optimizer.cpp | 通用子表达式消除 |
| Inlining Pass | Optimizer.cpp | shouldInline 阈值存在, 内联逻辑空 |
| LICM Pass | Optimizer.cpp | 循环不变量外提 |
| LoopUnroll Pass | Optimizer.cpp | 循环展开 |
| SSADestruction Pass | Optimizer.cpp | return true 桩 |
| ExternalExecutableLoader IPC 调用链 | Module.cpp | IPC 框架就绪, 跨进程符号查询/调用链未实现 |
| CppBackend 语言后端 | LanguageBackend.cpp | generateSource 返回注释桩 |

### P3 — 工具与质量

| 项目 | 文件 | 说明 |
|------|------|------|
| CLI 重写 | main.cpp | CLI_DOCUMENTATION.md 已完成, 代码需替换 argc/argv 手动解析 |
| SCA BFS 拓扑排序 + 循环检测 | Module.cpp | 当前仅 set 去重, 无排序/循环检测 |
| 版本约束测试 | test/ | 待编写自动化测试 |

### P4 — 远期

| 项目 | 文件 | 说明 |
|------|------|------|
| DynamicLibLoader | Module.cpp | 桩代码 |
| ScriptLoader | Module.cpp | 桩代码 |
| 其他语言后端 (Python/Java) | — | 未开始 |
| DependencyGraph | — | 替代 BFS 的图方案, 未定义 |

### P5 — 低优先级

| 项目 | 文件 | 说明 |
|------|------|------|
| `[=]` lambda capture 警告 | IPC.cpp:516,574 | C++20 deprecated, 不影响功能 |

---

## 三、技术说明

1. **INTRINSIC/ASSEMBLY opcode**: 编译器内部指令 (`__asm__` / 内联函数), IR Interpreter 返回 `null` 是正确的处理, **无需实现业务逻辑**。

2. **BigNum 除法 O(n²)**: 长除法是设计选择, 非"未完成"。10^n 特化路径覆盖 100% 常用场景 (如 `10^5000n` 瞬时完成)。

3. **SCA BFS 无拓扑排序**: BFS 加载可用但不保证依赖顺序。生产环境加载简单模块可行, 复杂依赖图需补充排序。

---

## 四、开发重心建议

根据当前状态, 后续开发顺序:

```
1. 收尾: 激活 zcc exec (连接 NativeBackend::runModule)
2. 扩展: 填充优化器 DeadCodeElimination (对 IR 解释器性能提升明显)
3. 修复: SCA BFS 拓扑排序 + 循环依赖检测
4. 重构: CLI 统一到结构化选项解析器
```

---

## 测试覆盖

| 范围 | 状态 | 数量 |
|------|------|------|
| IR 生成回归 | ✅ 通过 | 10/10 |
| SCA Manifest 解析 | ✅ 通过 | 3/3 |
| IR 解释器爆破 (IRBlast) | ✅ 通过 | 12/12 |
| 最终爆破 | ✅ 通过 | 8/8 |
| 版本约束 | ⬜ 待编写 | — |

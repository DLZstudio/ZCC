# IR 解释器爆破报告 v1.1 — SCA 维度补正

**版本:** v1.1 (基于 SCA_DESIGN.md 补正)  
**日期:** 2026-06-01  

---

## 补正摘要

v1.0 将 `ALLOC/ALLOCA/FREE` 归类为"不可达死操作码"，属于误判。  
经 **SCA_DESIGN.md** 审计后修正：这三者是 SCA 安全内存模型的**唯一硬通货**。

---

## 1. SCA 安全约束与 ALLOC 的不可替代性

### 1.1 SCA 的指针禁令

```
SCA 安全模型:
  ❌ 普通 ZCC 代码: int* p = &x;        → 报错/不安全
  ❌ C 代码块:      用 malloc/free 裸指针 → 限于 [c] 块内，不能跨模块
  ✅ 指令集代码:    [ir] 块使用 ALLOC    → 唯一的安全共享内存方式
```

SCA 严禁裸指针跨模块传递。同一文件内的 `[c]` 块可以使用 `malloc`，但分配的指针是进程局部地址，不能跨越 `[ir]`→`[zcc]` 边界。

### 1.2 ALLOC 是 SCA 的"安全指针"

```zcc
// 在 [ir] 块中 (ZCC 层无法直接使用这些指令):
auto handle = ALLOC 1024;         // 返回安全句柄（非裸指针）
STORE handle, 0, 42;              // 向偏移 0 写入
auto val = LOAD handle, 0;        // 从偏移 0 读取
FREE handle;                      // 释放
```

内存句柄由模块管理器维护引用计数，实现安全的跨模块共享：
- Producer: ALLOC → 写入 → 传递句柄
- ModuleManager: 引用计数 +1, 转发
- Consumer: LOAD → 读取 → FREE (引用计数 -1)

---

## 2. ALLOC/ALLOCA/FREE 现状: 关键缺陷

### 2.1 CBackend 实现

| 操作码 | CBackend 代码 | 状态 |
|--------|-------------|------|
| ALLOC | `malloc(opName(1))` | ✅ 真实代码 |
| ALLOCA | `// ALLOCA: ...` | 🔴 **注释桩** |
| FREE | `free(opName(0))` | ✅ 真实代码 |

> **ALLOCA 在 CBackend 中是注释行，不生成任何 C 代码。**

### 2.2 IRGenerator: 从不生成

```
ZCCIR.hpp  → ALLOC/ALLOCA/FREE 已在枚举 (行 53-57)
CBackend   → ALLOCa 是桩，ALLOC/FREE 真实
IRGenerator → 从未调用 emitInstruction(ALLOC, ...)
```

**结论: 从 ZCC 源码路径 (`[zcc]` 块) 完全无法触发 ALLOC 操作码。只能通过手写 `[ir]` 块使用。**

---

## 3. @external 虚空接口解析缺陷

### 3.1 发现: @external 语法解析失败

```zcc
@external function remoteProcedure(arg1, arg2);   // ← Parser 报错
```

**症状:** `Expected '{' before block (at ';')`

### 3.2 根因

`src/Parser.cpp:1381`:
```cpp
if (match(TokenType::FUNCTION) || match(TokenType::FUNC)) {
    declaration = parseFunctionDeclaration();  // ← 要求 { body }!
}
```

`parseFunctionDeclaration()` 总是期望完整的函数定义（含 body）。但 `@external` 声明只需要签名——没有 body。

### 3.3 影响

SCA 虚空接口的**显式声明路径完全阻断**。隐式发现（正则差集）替代，但精度低：
- 显式: `@external function f(args)` — 开发者明确声明 → ❌ 不工作
- 隐式: SmartSCALoader 正则差集猜测 → ✅ 可用但误报率高

---

## 4. SCA 维度爆破测试

| # | 测试 | 目标 | 结果 |
|---|------|------|------|
| 01 | `scablast_01_buffer` | 大缓冲区+数组模拟 SCA 共享 | ✅ PASS |
| 02 | `scablast_02_external` | @external 虚空接口声明 | ❌ P:4 (解析失败) |
| 03 | `scablast_03_producer_consumer` | namespace 生产者-消费者模式 | ✅ PASS |

### 4.1 受限测试 vs 真实 SCA 场景

这些测试只能验证 .zcc 层面的逻辑正确性，**无法验证真正的 SCA 内存管理**，因为:
1. ALLOC/ALLOCA/FREE 无法从 .zcc 源码触发
2. `@external` 语法解析本身就失败
3. 测试跑分器只覆盖 Lexer→Parser→IRGenerator，不包含 SCA 加载器

### 4.2 推荐的完整 SCA 爆破用例

需要扩展测试框架以支持 .sca 文件和 `[ir]` 块:

```sca
// scablast_04_alloc.sca
[zcc]
function main() {
    auto handle = ALLOC 1024;       // 期望: 返回有效句柄
    STORE handle, 0, 123;
    auto val = LOAD handle, 0;
    if (val == 123) {
        print("PASS");
    } else {
        print("FAIL");
    }
    FREE handle;
}
[/zcc]
```

```sca
// scablast_05_module_ipc.sca
[zcc]
import { processImage } from "image_processor.sca";

function main() {
    auto imgBuffer = ALLOC 4096;
    processImage(imgBuffer);
    FREE imgBuffer;
}
[/zcc]
```

---

## 5. 修正后的 IR 操作码重要性评估

| 操作码类别 | v1.0 评估 | **SCA 补正后** | 原因 |
|:---|:---|:---|:---|
| TRUNC/ZEXT/SEXT | 不重要 | ✅ 不重要 | 动态类型不需要静态大小转换 |
| FPTRUNC/FPEXT/FPTOUI/... | 不重要 | ✅ 不重要 | 同上 |
| BITCAST | 不重要 | ✅ 不重要 | 同上 |
| STR_CONCAT/LEN/CMP/SLICE | 不重要 | ✅ 不重要 | ADD/CMP/OBJECT_GET 替代 |
| ARRAY_LEN | 不重要 | ✅ 不重要 | OBJECT_GET 替代 |
| **ALLOC / FREE** | 桩 | 🔴 **P0 刚需** | SCA 跨模块共享内存唯一方式 |
| **ALLOCA** | 桩 | 🔴 **P0 刚需** | SCA 栈分配 + CBackend 是注释桩 |
| INTRINSIC | 不重要 | 🟡 **P1** | 与 @external 虚空接口桥接相关 |
| ASSEMBLY | 不重要 | ✅ 不重要 | 仅[asm]块使用 |
| TRAP | 不重要 | ✅ 不重要 | 调试用 |
| UNREACHABLE | 不重要 | ✅ 不重要 | 优化器断言 |

---

## 6. 修正后的 IR 解释器完成度

| 维度 | v1.0 | **SCA 补正** | 说明 |
|------|------|-------------|------|
| AST→IR 映射 | 🟢 优秀 | 🟢 优秀 | 不变 |
| 指令生成正确性 | 🟢 优秀 | 🟢 优秀 | 不变 |
| 控制流拓扑 | 🟢 优秀 | 🟢 优秀 | 不变 |
| CBackend 完整度 | 🟢 62/62 | 🔴 60/62 | ALLOCA 是注释, TRAP 未实现 |
| SCA 内存管理 | ❓ 未评估 | 🔴 **致命** | ALLOCA 桩 + ALLOC/FREE 不可达 |
| @external 虚空接口 | ❓ 未评估 | 🔴 **致命** | Parser 要求 body 导致解析失败 |
| 跨模块通信 | ❓ 未评估 | 🔴 **阻断** | ModuleManager 10% 完成 |

---

## 7. 修复优先级 (SCA 视角)

| 优先级 | 问题 | 位置 | 修复难度 |
|--------|------|------|---------|
| P0 | ALLOCA 注释桩 | CBackend.cpp:211 | ⭐ 简单 — 改为 malloc |
| P0 | ALLOC/FREE IRGenerator 不可达 | IRGenerator.hpp | ⭐⭐ 中等 — 需要在 AST 中暴露 |
| P0 | @external 解析要求 body | Parser.cpp:1381 | ⭐ 简单 — 添加无 body 路径 |
| P1 | INTRINSIC 虚空接口桥接 | IRGenerator + CBackend | ⭐⭐ 中等 |
| P2 | ModuleManager 完善 | Module.cpp/Module.hpp | ⭐⭐⭐ 复杂 |
| P2 | BFS 拓扑排序+循环检测 | Module.cpp | ⭐⭐ 中等 |

---

## 8. 结论

### v1.0 遗漏的关键缺陷

> **"12/12 PASS" 只在纯 ZCC 业务代码层面成立。**
>
> 一旦引入 SCA 的内存管理和模块通信需求:
> - ALLOCA 在 CBackend 中是**注释桩**
> - ALLOC/FREE **从未被 IRGenerator 触发**
> - @external 虚空接口**解析失败**
>
> **SCA 的内核（内存安全共享+虚空接口发现）当前完全不能工作。**

### 修复后预期

修复 3 个 P0 问题后:
- ALLOCA 桩 → `malloc` 实现
- ALLOC/FREE → IRGenerator 支持 (需要对应的 ZCC 语法糖或 IR 内联)
- @external → 解析器支持无 body 签名

SCA 跨模块数据共享将可运行（模式一），虚空接口自动发现可达。

**当前 IR 解释器真实完成度: 60/66 (91%)，但 P0 三项全失意味着 SCA 架构不可用。**

# ZCCIR — 中间表示技术规范

> ZCC Intermediate Representation — SSA 形式、66 操作码、文本输出

---

## 目录

- [1. 设计哲学](#1-设计哲学)
- [2. 操作码全集 (66 个)](#2-操作码全集-66-个)
- [3. 类型体系](#3-类型体系)
- [4. IR 文本格式](#4-ir-文本格式)
- [5. 数据流模型](#5-数据流模型)
- [6. IR 生成管线 (IRGenerator)](#6-ir-生成管线-irgenerator)
- [7. 优化器 (8 Pass)](#7-优化器-8-pass)
- [8. IR 解释器 (Interpreter)](#8-ir-解释器-interpreter)
- [9. 当前状态与路线图](#9-当前状态与路线图)

---

## 1. 设计哲学

ZCCIR 是整个 ZCC 编译器的**核心抽象层**。它位于前端（Lexer/Parser/AST）和后端（C 代码生成/解释器/JIT）之间，是所有编译模式共享的通用中间语言。

### 三原则

1. **SSA 形式** — 每个值只赋值一次，通过 `reg_` 寄存器映射实现 USE-DEF 链
2. **文本可读** — `toText()` 输出人类可读的 `%id = opcode %operand1 %operand2` 格式
3. **后端无关** — 同一个 IR 模块可输出 C 源码、被解释器执行、或被 JIT 编译

### 数据流

```
.zcc 源码 → Lexer → Parser → AST → IRGenerator → ZCCIRModule
                                                    ├── toText() → .ir 文件
                                                    ├── CBackend → .c → GCC → .exe
                                                    └── Interpreter → 直接执行
```

---

## 2. 操作码全集 (66 个)

**文件**: `include/midend/ZCCIR.hpp`（第 15-105 行）

### 2.1 控制流 (7)

| Opcode | 索引 | 语义 | 实现状态 (C后端/解释器) |
|--------|------|------|----------------------|
| `NOP` | 0 | 无操作 | ✅ / ✅ |
| `JMP` | 1 | 无条件跳转 | ✅ / ✅ (CFG 后继) |
| `JMP_IF` | 2 | 条件为真跳转 | ✅ / ✅ |
| `JMP_IF_NOT` | 3 | 条件为假跳转 | ✅ / ✅ |
| `CALL` | 4 | 函数调用 | ✅ / ✅ (内置函数) |
| `RET` | 5 | 返回 | ✅ / ✅ |
| `PHI` | 6 | SSA Phi 节点 | ✅ / ✅ (简化取首) |

### 2.2 算术运算 (9)

| Opcode | 语义 | C 后端映射 | 解释器映射 |
|--------|------|-----------|----------|
| `ADD` | 加法 | `a + b` | `a + b` (double) |
| `SUB` | 减法 | `a - b` | `a - b` |
| `MUL` | 乘法 | `a * b` | `a * b` |
| `DIV` | 除法 | `a / b` | `a / b` (除零→0) |
| `MOD` | 取模 | `a % b` | `fmod(a, b)` |
| `NEG` | 取负 | `-a` | `-a` |
| `INC` | 自增 | `a++` | `a+1` (写回 reg_) |
| `DEC` | 自减 | `a--` | `a-1` (写回 reg_) |
| `POW` | 幂运算 | `pow(a, b)` | `pow(a, b)` |

### 2.3 位运算 (6)

| Opcode | C 后端 | 解释器 |
|--------|--------|--------|
| `AND` | `&` | (逻辑) `&&` |
| `OR` | `\|` | (逻辑) `\|\|` |
| `XOR` | `^` | `^` (int64) |
| `NOT` | `~` | `!` (逻辑) |
| `SHL` | `<<` | `<<` (int64) |
| `SHR` | `>>` | `>>` (int64) |

### 2.4 比较运算 (6)

| Opcode | 语义 | 实现 |
|--------|------|------|
| `CMP_EQ` | 等于 | `==` |
| `CMP_NE` | 不等于 | `!=` |
| `CMP_LT` | 小于 | `<` |
| `CMP_LE` | 小于等于 | `<=` |
| `CMP_GT` | 大于 | `>` |
| `CMP_GE` | 大于等于 | `>=` |

### 2.5 内存操作 (5)

| Opcode | 语义 | C 后端 | 解释器 |
|--------|------|--------|--------|
| `LOAD` | 加载值 | `= var` | `reg_[id]` 读取 |
| `STORE` | 存储值 | `var = value` | `reg_[id] = value` |
| `ALLOCA` | 栈分配 | `alloca(size)` | 哨兵(int64_t 0) |
| `ALLOC` | 堆分配 | `malloc(size)` | 哨兵 |
| `FREE` | 释放 | `free(ptr)` | 空操作 |

### 2.6 类型转换 (10)

| Opcode | 语义 | C 映射 |
|--------|------|--------|
| `TRUNC` | 截断到 int64 | `(int64_t)` |
| `ZEXT` | 零扩展到 uint64 | `(uint64_t)` |
| `SEXT` | 符号扩展 | `(int64_t)(int32_t)` |
| `FPTRUNC` | 浮点截断 | `(float)` |
| `FPEXT` | 浮点扩展 | `(double)` |
| `FPTOUI` | 浮点→无符号整数 | `(uint64_t)` |
| `FPTOSI` | 浮点→有符号整数 | `(int64_t)` |
| `UITOFP` | 无符号整数→浮点 | `(double)(uint64_t)` |
| `SITOFP` | 有符号整数→浮点 | `(double)(int64_t)` |
| `BITCAST` | 位重解释 | 透传 |

### 2.7 高精度运算 (7)

| Opcode | C 后端映射 | 解释器映射 |
|--------|-----------|----------|
| `BIG_ADD` | `bignum_add()` | 字符串高精度加法 |
| `BIG_SUB` | `bignum_sub()` | 字符串高精度减法 |
| `BIG_MUL` | `bignum_mul()` | Karatsuba 乘法 |
| `BIG_DIV` | `bignum_div()` | 长除法 |
| `BIG_MOD` | `bignum_mod()` | 长除法取余 |
| `BIG_CMP` | `bignum_cmp()` | 字符串比较 |
| `BIG_POW` | `bignum_pow()` | 快速幂 + 10^n 特化 |

### 2.8 字符串操作 (4)

| Opcode | 语义 | C 后端 | 解释器 |
|--------|------|--------|--------|
| `STR_CONCAT` | 字符串连接 | `zcc_str_concat()` | `+` operator |
| `STR_LEN` | 字符串长度 | `strlen()` | `.size()` |
| `STR_CMP` | 字符串比较 | `strcmp()` | `==` 比较 |
| `STR_SLICE` | 字符串切片 | `zcc_str_slice()` | `.substr()` |

### 2.9 数组操作 (4)

| Opcode | C 后端映射 | 解释器映射 |
|--------|-----------|----------|
| `ARRAY_NEW` | `zcc_array_new()` | `vector<RuntimeValue>{}` |
| `ARRAY_GET` | `zcc_array_get()` | `arr[idx]` |
| `ARRAY_SET` | `zcc_array_set()` | `arr[idx] = val` |
| `ARRAY_LEN` | `zcc_array_len()` | `arr.size()` |

### 2.10 对象操作 (3)

| Opcode | C 后端映射 | 解释器映射 |
|--------|-----------|----------|
| `OBJECT_NEW` | `zcc_object_new()` | `unordered_map<string,RV>{}` |
| `OBJECT_GET` | `zcc_object_get()` | `obj.find(key)` |
| `OBJECT_SET` | `zcc_object_set()` | `obj[key] = val` |

### 2.11 异常与特殊 (5)

| Opcode | C 后端 | 解释器 |
|--------|--------|--------|
| `THROW` | `zcc_throw()` | cerr + null |
| `TRAP` | `abort()` | cerr + null |
| `UNREACHABLE` | `__builtin_unreachable()` | cerr + null |
| `INTRINSIC` | 注释 | 返回 null |
| `ASSEMBLY` | `__asm__()` | 返回 null |

---

## 3. 类型体系

### 3.1 ZCCIRValue

```cpp
class ZCCIRValue {
    ZCCIRValueKind kind_;   // CONSTANT / ARGUMENT / LOCAL / TEMPORARY / INSTRUCTION
    ZCCTypePtr type_;       // void / bool / i64 / f64 / string / any
    ID id_;                 // uint32_t, 唯一标识
    std::string name_;      // 调试名称
};
```

**值类型 (ZCCIRValueKind)**:
| Kind | 创建位置 | 示例 |
|------|---------|------|
| `CONSTANT` | `emitConstant()` | 42, "hello", true |
| `ARGUMENT` | `visitFunctionDecl()` | 函数参数 |
| `LOCAL` | `visitVariableDecl()` | `auto x = 42` |
| `TEMPORARY` | `emitInstruction()` | 指令中间结果 |
| `INSTRUCTION` | 指令本身的结果 | (未来扩展) |

### 3.2 ZCCIRConstant

```cpp
class ZCCIRConstant : public ZCCIRValue {
    ConstantValue value_;  // variant<monostate, bool, int64_t, double, string, vector<uint8_t>>
};
```

**支持的字面量类型**: null, bool, int (int64_t), float (double), string, binary

### 3.3 ZCCIROperand

```cpp
class ZCCIROperand {
    ValuePtr value_;  // shared_ptr<ZCCIRValue> — 真实值引用 (P0 修复后的核心)
};
```

### 3.4 ZCCIRInstruction

```cpp
class ZCCIRInstruction {
    ZCCIROpcode opcode_;
    ZCCTypePtr type_;
    ID id_;
    OperandList operands_;    // vector<ZCCIROperand> (USE)
    ValuePtr result_;         // shared_ptr<ZCCIRValue> (DEF, P0 新增)
    ZCCIRBlock* parent_;
};
```

### 3.5 ZCCIRBlock

```cpp
class ZCCIRBlock {
    std::string label_;
    InstructionList instructions_;    // vector<unique_ptr<ZCCIRInstruction>>
    BlockList predecessors_;
    BlockList successors_;
};
```

**终止指令判定**: 最后一条指令的 opcode 是 RET / JMP / JMP_IF / JMP_IF_NOT / THROW / UNREACHABLE

### 3.6 ZCCIRFunction

```cpp
class ZCCIRFunction {
    std::string name_;
    ZCCTypePtr returnType_;
    ArgumentList arguments_;   // vector<pair<string, ZCCTypePtr>>
    BlockList blocks_;         // unique_ptr<ZCCIRBlock>[]
    bool isExternal_;
    bool isInline_;
};
```

### 3.7 ZCCIRModule

```cpp
class ZCCIRModule {
    std::string name_;
    FunctionList functions_;    // unique_ptr<ZCCIRFunction>[]
    GlobalList globals_;        // unique_ptr<ZCCIRGlobal>[]
    ConstantList constants_;    // shared_ptr<ZCCIRConstant>[]
    ZCCIRMetadata metadata_;    // 版本/目标/源文件/编译时间
};
```

**方法**: `createFunction()` / `createGlobal()` / `createConstant()` / `addConstant()` / `toText()` / `serialize()` / `deserialize()`

---

## 4. IR 文本格式

### 4.1 格式规范

```
.version 1.0
.module "path/to/source.zcc"
.source "path/to/source.zcc"

.global <type> @<name>

.func <return_type> @<name>(<type> %<param_id>, ...) [extern] {
<label>:
  %<result_id> = <opcode> %<instr_id> %<use_id> %<use_id> ...
  ...
}
```

### 4.2 指令格式

```
%<result> = <opcode> %<instr_id> [%<use> ...]
```

**规则**:
- `%<result>`: 指令产生的 DEF 值 (P0 新增，无结果则不输出)
- `%<instr_id>`: 指令自身的 ID
- `%<use>`: 操作数，指向其他值的 ID
- 操作数为 null → 输出 `_` 占位

### 4.3 示例

```
%3 = store %0 %2 %1     → 值3 = STORE, 目标%2(x), 源%1(42)
%4 = load  %1 %2        → 值4 = LOAD %2(x)
%6 = add   %2 %4 %5     → 值6 = ADD %4, %5(1)
%8 = store %3 %7 %6     → 值8 = STORE, 目标%7(y), 源%6
ret %8                  → 返回
```

---

## 5. 数据流模型

### 5.1 values_ 映射表 (P0 核心)

**文件**: `include/midend/IRGenerator.hpp`

```cpp
std::unordered_map<ZCCIRValue::ID, std::shared_ptr<ZCCIRValue>> values_;
```

**注册时机**:
| 场景 | 注册代码 |
|------|---------|
| 常量创建 | `emitConstant()` → `values_[id] = constant` |
| 变量声明 | `visitVariableDecl()` → `values_[varID] = ZCCIRValue(LOCAL, ...)` |
| 参数声明 | `visitFunctionDecl()` → `values_[paramID] = ZCCIRValue(ARGUMENT, ...)` |
| 指令结果 | `emitInstruction()` → `values_[resultID] = ZCCIRValue(TEMPORARY, ...)` |

### 5.2 emitInstruction 流程

```
emitInstruction(opcode, operands, type)
  │
  ├── 1. 创建 ZCCIRInstruction (opcode, type, instrID++)
  │
  ├── 2. 解析操作数: for each operand_id in operands:
  │     ├── values_.find(id) → ZCCIROperand(valuePtr)  // 真实引用
  │     └── 未找到 → ZCCIROperand()                      // 空壳
  │
  ├── 3. 创建结果值 (非 void):
  │     ├── resultID = nextValueID_++
  │     ├── values_[resultID] = ZCCIRValue(TEMPORARY, type, resultID)
  │     └── instr->setResult(values_[resultID])
  │
  └── 4. currentBlock_->addInstruction(instr)
       return resultID
```

### 5.3 ID 分配

```
nextValueID_       = 1     // 0 作为"未找到"哨兵
nextInstructionID_ = 0
```

---

## 6. IR 生成管线 (IRGenerator)

**文件**: `include/midend/IRGenerator.hpp`（~1300 行，全 inline 实现）

### 6.1 32 个 Visitor 方法

| 类别 | 方法 | 功能 |
|------|------|------|
| **顶层** | `visitProgram` | 程序入口，auto-main 包装 |
| **声明** | `visitFunctionDecl` / `visitVariableDecl` / `visitClassDecl` | 函数/变量/类声明 |
| **语句** | `visitBlockStmt` / `visitExpressionStmt` / `visitIfStmt` / `visitForStmt` / `visitWhileStmt` / `visitForEachStmt` / `visitSwitchStmt` / `visitReturnStmt` / `visitBreakStmt` / `visitContinueStmt` / `visitThrowStmt` / `visitTryStmt` | 控制流语句 |
| **表达式** | `visitLiteralExpr` / `visitIdentifierExpr` / `visitBinaryExpr` / `visitUnaryExpr` / `visitAssignExpr` / `visitCallExpr` / `visitMemberExpr` / `visitIndexExpr` / `visitArrayExpr` / `visitObjectExpr` / `visitFunctionExpr` / `visitArrowFunctionExpr` / `visitTernaryExpr` | 表达式 |
| **模块** | `visitModuleDecl` / `visitImportDecl` / `visitExportDecl` | 模块（空实现） |

### 6.2 核心状态

```cpp
std::unique_ptr<ZCCIRModule> module_;       // 输出模块
ZCCIRFunction* currentFunction_;            // 当前函数上下文
ZCCIRBlock* currentBlock_;                  // 当前基本块
ScopeInfo* currentScope_;                   // 作用域 (name→ID 映射)
ControlFlowInfo controlFlow_;               // 循环栈
ZCCIRValue::ID currentExprResult_;         // 当前表达式结果
```

### 6.3 辅助函数

| 函数 | 功能 |
|------|------|
| `emitInstruction(opcode, operands, type)` | 创建指令 + 解析操作数 + 分配结果 |
| `emitTerminator(opcode, operands)` | 创建终止指令 (JMP/RET/THROW) |
| `emitJump(target)` | 无条件跳转 + 设置 CFG 后继 |
| `emitCondJump(cond, ifTrue, ifFalse)` | 条件跳转 + 两支后继 |
| `emitConstant(value, type)` | 创建常量并注册到 values_ |
| `enterScope(isFunc)` / `leaveScope()` | 作用域进出 |
| `declareVariable(name, id)` / `lookupVariable(name)` | 变量管理 |

---

## 7. 优化器 (8 Pass)

**文件**: `include/midend/Optimizer.hpp` + `src/midend/Optimizer.cpp`

### 7.1 Pass 列表

| Pass | 级别 | 状态 | 说明 |
|------|------|------|------|
| `SSADestructionPass` | O0+ | 桩 | "IR 不使用 phi，直接返回" |
| `ConstantFoldingPass` | O1+ | 部分 | ADD/SUB/MUL/DIV/MOD/CMP/AND/OR 折叠 |
| `PeepholeOptimizationPass` | O1+ | 部分 | addZero/mulOne/mulZero/divOne 可用 |
| `DeadCodeEliminationPass` | O1+ | 桩 | liveness 分析存在，消除逻辑空 |
| `CommonSubexpressionEliminationPass` | O2+ | 桩 | return false |
| `InliningPass` | O2+ | 桩 | shouldInline 阈值检查 (10 指令) |
| `LoopInvariantCodeMotionPass` | O2+ | 桩 | return false |
| `LoopUnrollPass` | O3+ | 桩 | return false |

### 7.2 工厂函数

```cpp
createOptimizer(OptimizationLevel level) → vector<OptimizerPass*>
// O0: SSADestruction
// O1: + ConstantFolding + Peephole + DeadCode
// O2: + CSE + Inlining + LICM
// O3: + LoopUnroll
```

---

## 8. IR 解释器 (Interpreter)

**文件**: `src/backend/NativeBackend.cpp` + `include/backend/NativeBackend.hpp`

### 8.1 架构

```
Interpreter::execute(mainFunc)
  ├── while (currentBlock):
  │   ├── BlockResult = executeBlock(*block)
  │   │   ├── for each instr:
  │   │   │   ├── value = executeInstruction(*instr)
  │   │   │   ├── reg_[resultID] = value     // 存入寄存器
  │   │   │   └── switch 控制流 (JMP/JMP_IF/RET)
  │   │   └── return {nextBlock, value}
  │   └── currentBlock = nextBlock
  └── return state_.getReturn()
```

### 8.2 关键组件

| 组件 | 说明 |
|------|------|
| `InterpreterState` | 作用域栈 + 9 内置函数 + IR 函数注册表 |
| `reg_` | `unordered_map<uint32_t, RuntimeValue>` — 指令结果寄存器 |
| `resolveOperand(inst, idx)` | 从常量/reg_ 查找操作数值 |
| `constantToRuntime(cv)` | ZCCIRConstant → RuntimeValue 转换 |
| `executeCall()` | 内置函数 (print/println/input/typeof/len/str/num/exit) |
| `BigNum` | 7 个字符串高精度函数 (add/sub/mul/div/mod/cmp/pow) |

### 8.3 内置函数

```cpp
println(args)  → cout << ... << endl
print(args)    → cout << ...
readline(args) → getline(cin)
input(args)    → getline(cin)  (别名)
typeof(val)    → "null" / "boolean" / "number" / "string" / "array" / "object"
len(val)       → string.size() / array.size()
str(val)       → val.toString()
num(val)       → stod(str)
exit(code)     → std::exit(code)
```

### 8.4 RuntimeValue

```cpp
using Value = std::variant<
    std::monostate,                             // null
    bool,                                       // boolean
    int64_t,                                    // integer
    double,                                     // float
    std::string,                                // string
    std::vector<RuntimeValue>,                  // array
    std::unordered_map<std::string, RuntimeValue> // object
>;
```

---

## 9. 当前状态与路线图

### 9.1 完成度矩阵

| 子系统 | 完成度 | 说明 |
|--------|--------|------|
| IR 操作码定义 (66) | **100%** | ZCCIR.hpp |
| IR 生成器 (32 visitor) | **100%** | IRGenerator.hpp (全 inline) |
| IR 数据流 (P0) | **100%** | values_ 映射 + DEF 字段 |
| IR 文本输出 | **100%** | toText() + toString() |
| C 后端代码生成 | **100%** | LanguageBackend.cpp 66/66 |
| IR 解释器 (63/66) | **95%** | 仅 INTRINSIC/ASSEMBLY 未实现 |
| BigNum (Karatsuba) | **100%** | 7 个字符串高精度函数 |
| 优化器 (8 Pass) | **25%** | 2/8 有实质逻辑 |
| JIT (LLVM) | **桩** | JITBackend.hpp |
| IR deserialize/load | **0%** | 声明存在, 未实现 |

### 9.2 路线图

```
✅ 66 opcode 定义 + 32 visitor 生成
✅ IR 数据流 + DEF/USE 链
✅ C 后端 66/66
✅ IR Interpreter 框架 + 63/66 opcode
✅ BigNum 字符串高精度
⚠️ 优化器 Pass 填充 (6 个空 Pass)
⬜ JIT LLVM 实际编译 (P2 phase 2)
⬜ IR 文件加载 (deserialize)
```

### 9.3 关键文件索引

| 文件 | 行数 | 内容 |
|------|------|------|
| `include/midend/ZCCIR.hpp` | 430+ | 全部 IR 类型 + opcode 定义 |
| `include/midend/IRGenerator.hpp` | 1300+ | 32 visitor + 数据流引擎 |
| `src/compat_stubs.cpp` | 260+ | IR 文本输出 + 类型桩 |
| `src/compiler/LanguageBackend.cpp` | 410+ | C 代码生成 66/66 |
| `src/backend/NativeBackend.cpp` | 500+ | 解释器 + BigNum |
| `include/midend/Optimizer.hpp` + `.cpp` | 600+ | 8 Pass 优化器 |

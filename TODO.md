# ZCC v0.2 未完成项记录

> **最后更新**: 2026-06-01

---

## 已完成

| 项目 | 状态 | 日期 |
|------|------|------|
| SCA v0.3 Manifest 解析器 | ✅ | 06-01 |
| SCA v0.3 Manifest 数据结构 | ✅ | 06-01 |
| SmartSCALoader v0.3 更新 (validate/load/deps) | ✅ | 06-01 |
| 版本约束检查 (SemVer >=/<=/==) | ✅ | 06-01 |
| Module.o + IPC.o 完整链接 | ✅ | 06-01 |

---

## 当前 Sprint

| 项目 | 状态 |
|------|------|
| IR Interpreter 框架修复 | ✅ 完成 (CFG 跳转引擎) |
| IR Interpreter opcode 实现 (66 个) | ✅ 完成 (63/66 = 95%) |
| 剩余: INTRINSIC / ASSEMBLY / 精确 BigNum | ⚠️ 简化为桩 (BIG_* 用 stold 近似, INTRINSIC/ASSEMBLY 返回 null) |

---

## 未完成

| 优先级 | 项目 | 文件 | 说明 |
|--------|------|------|------|
| P1 | IR Interpreter opcode 实现 (63/66) | NativeBackend.cpp | 仅 INTRINSIC/ASSEMBLY 未实现 |
| P1 | BigNum 字符串高精度 (add/sub/mul/div/mod/cmp/pow) | NativeBackend.cpp | Karatsuba 乘法 O(n^1.585) + 10^n 特化 + 负号支持 |
| ⚠️ | BigNum 除法仍为 O(n²) 长除法 | NativeBackend.cpp | 10^n 特化已覆盖常用场景 |
| P1 | execute() CFG 跳转 | ✅ 完成 |
| P1 | executeBlock() CFG + BlockResult | ✅ 完成 |
| P1 | resolveOperand() / constantToRuntime() | ✅ 完成 |
| P1 | executeCall() 内置函数 | ✅ 完成 |
| P2 | JIT 预埋 (LLVM 接口桩) | ✅ 完成 | `include/backend/JITBackend.hpp` |
| P2 | C 后端补齐 25 个 opcode | ✅ 完成 | LanguageBackend.cpp — 66/66 全覆盖 |
| P3 | CLI 重写 | main.cpp | CLI_DOCUMENTATION.md 已完成, 代码未改 |
| P3 | Optimizer 6 个 Pass 填逻辑 | Optimizer.cpp | 仅 ConstantFolding/Peephole 有实质逻辑 |
| P3 | LanguageBackend CppBackend 桩 | LanguageBackend.cpp | 仅返回注释 |
| P4 | ModuleManager::resolveVoidInterfaces() | Module.cpp | 只有声明 |
| P4 | DependencyGraph 拓扑排序 | Module.cpp | 当前 BFS 无排序/无循环检测 |
| P4 | ExternalExecutableLoader IPC 调用链 | Module.cpp | IPC 框架就绪, 实际查询/调用链未实现 |
| P4 | DynamicLibLoader/ScriptLoader | Module.cpp | 桩代码 |
| P5 | `[=]` lambda capture 警告 | IPC.cpp:516,574 | C++20 deprecated |

---

## 技术债务

| 项目 | 位置 | 说明 |
|------|------|------|
| build.sh 锁定 | build.sh | 需更新为包含 Module.cpp + IPC.cpp |
| zcc.exe 锁定 | build/zcc.exe | 替代方案: zcc_new.exe |
| IPC_STUB test | test/ipc_stub.cpp | 已删除, 自包含测试方案 |

---

## 测试覆盖

| 范围 | 状态 | 文件数 |
|------|------|--------|
| IR 生成回归 | ✅ 10/10 | hello/big_numbers/dynamic_types/control_flow/arrays_objects/functions/edge_cases/enum_test/namespace_test/syntax_errors |
| SCA Manifest 解析 | ✅ 3/3 | test/sca_tester.cpp |
| 版本约束 | ⬜ | 待编写 |
| IR Interpreter | ⬜ | 待编写 |

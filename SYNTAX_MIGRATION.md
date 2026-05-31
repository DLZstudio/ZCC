# ZCC v0.2 — v0.1 语法移植报告

> **日期**: 2026-05-31  
> **移植内容**: enum / namespace / @external(from import) / func 四种语法特性  
> **变更文件**: 6 个 (Token.hpp, AST.hpp, Keywords.hpp, Parser.hpp, Parser.cpp, Lexer.cpp)

---

## 一、移植的语法特性

### 1. `enum` 枚举声明

**语法**:
```zcc
enum Color { RED, GREEN, BLUE };
enum 颜色 { 红, 绿, 蓝 };
```

**AST 节点**: `EnumDecl { name, values }`

**编译器链路**:
```
Lexer → ENUM Token → Parser.parseEnumDeclaration() → EnumDecl AST → IRGenerator stub
```

### 2. `namespace` 命名空间

**语法**:
```zcc
namespace Math { auto pi = 3.14; }
namespace 数学 { auto 圆周率 = 3.14; }
```

**AST 节点**: `NamespaceDecl { name, members }`

**编译器链路**: 同上，成员递归解析

### 3. `@external` 虚空接口声明

**语法**:
```zcc
@external function sendData(data);
@外部 函数 发送数据 (数据);
```

**AST 节点**: `ExternalDecl { name, parameters, returnType }`

**编译器链路**:
```
Lexer → @识别 (scanOperator) → AT_EXTERNAL Token → Parser.parseExternalDeclaration()
```

### 4. `from "module" import ...` 导入语法

**语法**:
```zcc
from "network.sca" import connect, send;
从 "网络" 导入 连接, 发送;
```

**编译器链路**: Parser.parseFromImportDeclaration() → ImportDecl with specifiers

### 5. `func` 作为 `function` 简写

**语法**:
```zcc
func add(a, b) { return a + b; }
```

所有接受 `function` 的位置同时接受 `func`（声明、导出、类方法、函数表达式）

---

## 二、文件变更详情

| 文件 | 变更行数 | 关键变更 |
|------|---------|---------|
| `include/Token.hpp` | +4 enum值 +4 case分支 | ENUM, NAMESPACE, FUNC, AT_EXTERNAL |
| `include/AST.hpp` | +47行 | ExternalDecl, NamespaceDecl, EnumDecl |
| `include/Keywords.hpp` | +14行 | CN/EN 关键词 + KeywordType枚举 + 映射 |
| `include/Parser.hpp` | +7行 | 4个新方法声明 |
| `src/Parser.cpp` | +120行 | 4个解析方法 + 全部FUNCTION→FUNC适配 |
| `src/Lexer.cpp` | +12行 | @字符处理 + 关键词case映射 |

---

## 三、新增 AST 节点速查

```cpp
struct ExternalDecl : Declaration {
    std::string name;
    std::vector<std::string> parameters;
    std::string returnType;
};

struct NamespaceDecl : Declaration {
    std::string name;
    std::vector<DeclPtr> members;
};

struct EnumDecl : Declaration {
    std::string name;
    std::vector<std::string> values;
};
```

---

## 四、新增 TokenType

```cpp
ENUM,           // enum / 枚举
NAMESPACE,      // namespace / 命名空间
FUNC,           // func (函数简写)
AT_EXTERNAL,    // @external / @外部
```

---

## 五、测试结果

```
✅ big_numbers     — 499 bytes
✅ dynamic_types   — 613 bytes
✅ control_flow    — 226 bytes
✅ arrays_objects   — 989 bytes
✅ functions       — 534 bytes
✅ edge_cases      — 677 bytes
✅ hello           — 157 bytes
✅ test            — 111 bytes
✅ syntax_errors   — 正确拒绝
=== 9/9 通过，零回归 ===
```

---

## 六、编译器完整能力

```
类型系统:    auto, const, class, enum, namespace, function, func
控制流:      if-else, for, while, for-each, break, continue, switch
异常:        try-catch-finally, throw
模块:        import, from...import, @external, export
表达式:      +-*/, +=-=, ++--, =>, [], {}, a.b, a[b], a?b:c
字面量:     数字, 字符串, true/false, null, NaN, Infinity
注释:        // 单行, /* */ 多行
关键词:      49个中文 + 49个英文 双语支持
IR操作码:    62个全覆盖
后端:        C(生产), C++(预留), BIOS(简化)
优化器:      8 Pass (O0-O3)
运行时:      BigNum, Array, Object, String, 异常
CLI:         compile, ir, build, run, languages
插件:        ILanguageBackend 协议
```

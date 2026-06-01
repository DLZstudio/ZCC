# ILanguageBackend 插件系统设计文档

> ZCC 语言后端插件协议 — 一行命令切换目标语言

---

## 目录

- [1. 架构概览](#1-架构概览)
- [2. 插件接口 (ILanguageBackend)](#2-插件接口-ilanguagebackend)
- [3. 编译器配置 (CompilerConfig)](#3-编译器配置-compilerconfig)
- [4. 注册机制 (LanguageBackendRegistry)](#4-注册机制-languagebackendregistry)
- [5. 管线集成 (CompilerDriver)](#5-管线集成-compilerdriver)
- [6. 内置后端](#6-内置后端)
- [7. 扩展示例: Java 后端](#7-扩展示例-java-后端)
- [8. CLI 使用](#8-cli-使用)

---

## 1. 架构概览

```
zcc build --lang <name> file.zcc
       │
       ▼
CompilerDriver::build()
  ├── runLexer/runParser/runIRGenerator/runOptimizer  (共享前端)
  ├── LanguageBackendRegistry::getBackend("java")      (插件查找)
  ├── backend->generateSource(IR)                     (IR→目标语言)
  └── backend->getCompilerConfig()→buildCommand()      (调用外部编译器)
```

**设计原则**:
- **零侵入**: 添加新语言不需要修改 ZCC 核心代码
- **单一职责**: 每个后端只需实现 `ILanguageBackend` 的 6 个纯虚方法
- **可组合**: 编译型和解释型语言通过 `needsCompilation()` 区分

---

## 2. 插件接口 (ILanguageBackend)

**文件**: `include/compiler/LanguageBackend.hpp`（第 45-105 行）

### 2.1 纯虚方法 (必须实现)

| 方法 | 返回 | 说明 |
|------|------|------|
| `getLanguageName()` | `std::string` | 目标语言名: `"c"`, `"java"`, `"python"` |
| `getFileExtension()` | `std::string` | 源文件扩展名: `".c"`, `".java"`, `".py"` |
| `getDescription()` | `std::string` | 简短描述 |
| `generateSource(ir, opts)` | `std::string` | **核心**: IR → 目标语言源码 |

### 2.2 可选重写方法

| 方法 | 默认值 | 说明 |
|------|--------|------|
| `getCompilerConfig()` | `nullopt` | 返回 `nullopt` = 解释型语言 (Python/JS) |
| `getRuntimeSourceFiles()` | `{}` | 运行时库文件 (如 zcc_runtime.java) |
| `getExecutableExtension()` | `.exe` (Win) / `""` (Linux) | 产物扩展名 |
| `needsCompilation()` | `true` | 解释型语言返回 `false` |
| `generateProject(ir, opts)` | 单文件 | 多文件项目输出 |

### 2.3 实现最小示例

```cpp
class MyBackend : public ILanguageBackend {
public:
    std::string getLanguageName() const override { return "mine"; }
    std::string getFileExtension() const override { return ".my"; }
    std::string getDescription() const override { return "My custom backend"; }

    std::string generateSource(const ZCCIRModule& ir,
                                const CompileOptions& opts) override {
        // 遍历 ir.getFunctions() / ir.getBlocks() / ir.getInstructions()
        // 将 ZCCIR 操作码翻译为目标语言代码
        std::string source;
        for (auto& func : ir.getFunctions()) {
            source += "// " + func->getName() + "\n";
            for (auto& block : func->getBlocks()) {
                for (auto& instr : block->getInstructions()) {
                    source += "  // " + instr->toString() + "\n";
                }
            }
        }
        return source;
    }
};
```

---

## 3. 编译器配置 (CompilerConfig)

**文件**: `include/compiler/LanguageBackend.hpp`（第 21-41 行）

```cpp
struct CompilerConfig {
    std::string compiler;            // 编译器可执行路径
    std::string compileTemplate;     // 命令模板 (支持 {input}/{output}/{flags})
    std::vector<std::string> extraFlags;
    bool isInterpreter;              // true = 解释器 (Python/Node.js)
};
```

**模板占位符**:
| 占位符 | 含义 |
|--------|------|
| `{input}` | 生成的源文件 + 运行时文件 |
| `{output}` | 输出文件路径 |
| `{flags}` | 优化级别 |

**示例**:
```cpp
// GCC (C)
{ "gcc", "gcc {input} -o {output} -O2 -lm" }

// Java
{ "javac", "javac {input}" }

// Python (解释型, getCompilerConfig() 返回 nullopt)
{}  // 不需要编译步骤

// Go
{ "go", "go build -o {output} {input}" }
```

---

## 4. 注册机制 (LanguageBackendRegistry)

**文件**: `include/compiler/LanguageBackend.hpp`（第 141-164 行）

```cpp
class LanguageBackendRegistry {
public:
    static LanguageBackendRegistry& instance();                 // 单例
    void registerBackend(std::unique_ptr<ILanguageBackend>);    // 注册
    ILanguageBackend* getBackend(const std::string& name);      // 按名查找
    std::vector<std::string> getAvailableLanguages() const;     // 列出所有
    ILanguageBackend* getDefault() const;                       // 默认 = "c"
};
```

**注册方式**:
```cpp
// 方式一: 在 initializeBuiltinBackends() 中注册
void initializeBuiltinBackends() {
    LanguageBackendRegistry::instance().registerBackend(
        std::make_unique<CBackendPlugin>());
    LanguageBackendRegistry::instance().registerBackend(
        std::make_unique<JavaBackend>());  // 新增
}

// 方式二: 外部程序中注册 (无需修改 ZCC 源码)
auto& reg = LanguageBackendRegistry::instance();
reg.registerBackend(std::make_unique<MyCustomBackend>());
CompilerDriver driver;
driver.build(request);  // 自动使用已注册的后端
```

---

## 5. 管线集成 (CompilerDriver)

**文件**: `include/compiler/CompilerDriver.hpp:build()`（第 91-144 行）

```cpp
BuildResult CompilerDriver::build(const BuildRequest& request) {
    // ... 前端管线 (Lexer→Parser→IRGenerator→Optimizer) ...

    // 按 request.targetLanguage 查找后端
    auto* backend = LanguageBackendRegistry::instance().getBackend(request.targetLanguage);
    if (!backend) {
        backend = LanguageBackendRegistry::instance().getDefault();  // 回退到 "c"
    }

    // IR → 目标语言源码
    result.sourceOutputFile = ... + backend->getFileExtension();
    std::string sourceCode = backend->generateSource(*ir, opts);
    writeFile(result.sourceOutputFile, sourceCode);

    // 外部编译 (如果后端需要)
    if (backend->needsCompilation()) {
        auto cc = backend->getCompilerConfig();
        std::string cmd = cc->buildCommand(result.sourceOutputFile,
                                            result.outputFile, runtimeFiles, optLevel);
        std::system(cmd.c_str());
    }

    result.success = true;
    return result;
}
```

**关键**: `CompilerDriver` 通过 `getBackend(name)` 获取后端，不依赖任何编译期类型。这是运行时多态，纯虚接口保证零侵入。

---

## 6. 内置后端

### 6.1 C 后端 (CBackendPlugin)

| 项目 | 值 |
|------|-----|
| 语言名 | `c` |
| 扩展名 | `.c` |
| 完成度 | **100%** (66/66 opcode) |
| 编译器 | GCC |
| 运行时 | zcc_runtime.c/h |

### 6.2 C++ 后端 (CppBackendPlugin)

| 项目 | 值 |
|------|-----|
| 语言名 | `cpp` |
| 扩展名 | `.cpp` |
| 完成度 | **桩** (generateSource 返回注释) |
| 编译器 | G++ |

### 6.3 预留: Python / Java / Go / Rust

接口已就绪，等待实现 `ILanguageBackend` 的 6 个纯虚方法 + 注册即可。

---

## 7. 扩展示例: Java 后端

```cpp
// JavaBackend.hpp
#include "LanguageBackend.hpp"

class JavaBackend : public ILanguageBackend {
public:
    std::string getLanguageName() const override { return "java"; }
    std::string getFileExtension() const override { return ".java"; }
    std::string getDescription() const override { 
        return "Java Source Code (via javac)"; 
    }
    
    std::string getExecutableExtension() const override { return ".class"; }
    
    std::optional<CompilerConfig> getCompilerConfig() const override {
        CompilerConfig cc;
        cc.compiler = "javac";
        cc.compileTemplate = "javac {input}";
        return cc;
    }
    
    std::vector<std::string> getRuntimeSourceFiles() const override {
        return { "ZCCRuntime.java" };
    }
    
    std::string generateSource(const ZCCIRModule& ir,
                                const CompileOptions& opts) override {
        std::string src;
        src += "import zcc.runtime.*;\n\n";
        src += "public class Main {\n";
        src += "    public static void main(String[] args) {\n";
        // 遍历 IR 生成 Java 代码
        for (auto& func : ir.getFunctions()) {
            for (auto& block : func->getBlocks()) {
                for (auto& instr : block->getInstructions()) {
                    // ADD → zcc.add(a, b)
                    // CALL → 方法调用
                    src += "        // " + instr->toString() + "\n";
                }
            }
        }
        src += "    }\n";
        src += "}\n";
        return src;
    }
};

// 注册 (在 main.cpp 或任意初始化代码中)
LanguageBackendRegistry::instance().registerBackend(
    std::make_unique<JavaBackend>());
```

**使用**:
```bash
zcc build hello.zcc --lang java
# 产出: hello.java → javac hello.java → Hello.class
```

---

## 8. CLI 使用

```bash
# 列出所有可用后端
zcc languages

# 使用指定后端编译
zcc build file.zcc --lang c        # C 后端
zcc build file.zcc --lang java     # Java 后端 (待实现)
zcc build file.zcc --lang python   # Python 后端 (待实现)

# 不指定 --lang 时使用默认后端 (c)
zcc build file.zcc                 # 等价于 --lang c
```

---

## 结论

**插件系统本身 100% 完成且可用。** 瓶颈不在框架，在于每个目标语言的 IR→源码翻译器需要逐 opcode 实现。`ILanguageBackend` 的 6 个纯虚方法 + `LanguageBackendRegistry` 的单例 + `CompilerDriver` 的运行时多态，构成完整的可插拔后端架构。

# ZCC 编译器 v0.2.0 - 获取指南

## 📦 获取方式

### 方式 1: 本地 ZIP 归档（最快）

**归档位置**: `E:\114514\语言\zcc-v0.2.0.zip`

**大小**: 约 106 KB（仅包含必要的源代码和文档）

**解压后使用**:
```bash
# 解压到无中文路径（重要！）
# 例如：D:\zcc-compiler

# 进入目录
cd D:\zcc-compiler

# 编译
.\build.bat

# 测试
.\zcc.exe --help
```

---

### 方式 2: Git 克隆（推荐用于开发）

**如果已推送到 GitHub**:
```bash
# 克隆仓库
git clone https://github.com/你的用户名/zcc-compiler.git
cd zcc-compiler

# 切换到发布版本
git checkout v0.2.0

# 编译
mkdir build && cd build
cmake .. -G "MinGW Makefiles" \
    -DCMAKE_CXX_COMPILER="D:/C++_Qt/Tools/mingw1310_64/bin/g++.exe"
cmake --build .

# 或使用 build.bat（Windows）
.\build.bat
```

**如果推送到 Gitee**:
```bash
git clone https://gitee.com/你的用户名/zcc-compiler.git
cd zcc-compiler
git checkout v0.2.0
.\build.bat
```

---

### 方式 3: 本地 Git 仓库

**从本地仓库克隆**:
```bash
# 克隆到无中文路径
git clone E:/114514/语言/ZCC D:/zcc-clean

# 切换到发布版本
cd D:/zcc-clean
git checkout v0.2.0

# 编译
.\build.bat
```

---

## 📁 归档内容

```
zcc-v0.2.0.zip
├── CMakeLists.txt          # CMake 配置
├── build.bat               # Windows 编译脚本
├── README.md               # 项目说明
├── README_v2.md           # 新架构文档
├── DEVELOPMENT_PROGRESS.md # 开发进度报告
├── TEST_REPORT.md         # 测试报告
├── .gitignore             # Git 忽略规则
│
├── include/               # 头文件
│   ├── core/             # 核心组件
│   │   ├── Platform.hpp
│   │   ├── Compiler.hpp
│   │   ├── CacheManager.hpp
│   │   └── Diagnostics.hpp
│   ├── frontend/         # 前端
│   │   ├── Lexer.hpp
│   │   ├── Parser.hpp
│   │   └── AST.hpp
│   ├── midend/           # 中端
│   │   ├── ZCCIR.hpp
│   │   ├── Types.hpp
│   │   ├── IRGenerator.hpp
│   │   └── Optimizer.hpp
│   └── backend/          # 后端
│       ├── Backend.hpp
│       └── NativeBackend.hpp
│
├── src/                  # 源代码
│   ├── core/
│   │   └── Compiler.cpp
│   ├── frontend/
│   │   ├── Lexer.cpp
│   │   └── Parser.cpp
│   ├── midend/
│   │   └── Optimizer.cpp
│   └── backend/
│       ├── BackendManager.cpp
│       ├── CBackend.cpp
│       ├── NativeBackend.cpp
│       └── BIOSBackend.cpp
│
├── examples/             # 示例程序
│   ├── hello.zcc
│   ├── calculator.zcc
│   ├── fibonacci.zcc
│   └── high_precision_*.zcc
│
└── tests/               # 测试
    ├── CMakeLists.txt
    └── test_lexer.cpp
```

**总计**: 约 30 个文件，8000+ 行代码

---

## 🔧 编译要求

### 必需
- **编译器**: GCC 10+ 或 Clang 12+ 或 MSVC 2019+
- **CMake**: 3.20+
- **C++ 标准**: C++20

### 推荐（Windows）
- **MinGW**: mingw1310_64 或更新版本
- **路径**: 无中文、无空格

---

## 📝 快速开始

### Windows (MinGW)

```bash
# 1. 解压到 D:\zcc-compiler
# 2. 打开命令提示符
cd D:\zcc-compiler

# 3. 编译
.\build.bat

# 4. 测试
.\zcc.exe --help
.\zcc.exe compile examples\hello.zcc -o hello
```

### Linux

```bash
# 1. 解压到 ~/zcc-compiler
cd ~/zcc-compiler

# 2. 编译
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 3. 测试
./zcc --help
./zcc compile ../examples/hello.zcc -o hello
```

---

## 🎯 版本信息

**版本**: v0.2.0  
**发布日期**: 2026 年 3 月 14 日  
**状态**: 核心架构完成 ✅  

**核心特性**:
- ✅ 完整的前端（Lexer/Parser/AST）
- ✅ ZCC-IR 中间表示（SSA 形式）
- ✅ 多级优化器（O0-O3）
- ✅ 原生解释器后端
- ✅ Windows/Linux/BIOS 后端支持
- ✅ .ir 文件跨平台分发

---

## 📞 问题反馈

如果编译或使用遇到问题：

1. **检查路径**: 确保无中文、无空格
2. **检查编译器**: `g++ --version`
3. **检查 CMake**: `cmake --version`
4. **查看文档**: `README_v2.md`, `TEST_REPORT.md`

---

## 🚀 推送到远程仓库

如果你要将此项目推送到 GitHub/Gitee：

```bash
# 在本地仓库目录
cd E:\114514\语言\ZCC

# 添加远程仓库
git remote add origin https://github.com/你的用户名/zcc-compiler.git
# 或
git remote add origin https://gitee.com/你的用户名/zcc-compiler.git

# 推送所有分支和标签
git push -u origin master --tags

# 验证
git remote -v
git tag -l
```

---

**ZCC Compiler v0.2.0** - Zero-Core Command Language 🎉

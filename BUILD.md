# ZCC 编译器构建说明

## 前置要求

### Windows
- **Visual Studio 2017 或更高版本** (包含 C++ 工作负载)
- 或 **MinGW-w64** (GCC for Windows)
- **CMake 3.16+** (可选，如果手动构建则不需要)

### Linux
- **GCC 7+** 或 **Clang 5+**
- **CMake 3.16+**
- 构建工具：`build-essential` (Debian/Ubuntu) 或 `base-devel` (Arch)

### macOS
- **Xcode Command Line Tools**
- **CMake 3.16+** (可通过 Homebrew 安装：`brew install cmake`)

## 安装 CMake

### Windows
1. 访问 https://cmake.org/download/
2. 下载并安装 Windows 版本
3. 安装时选择"Add CMake to system PATH"

### Linux (Debian/Ubuntu)
```bash
sudo apt-get update
sudo apt-get install cmake
```

### Linux (CentOS/RHEL)
```bash
sudo yum install cmake
```

### macOS
```bash
brew install cmake
```

## 构建步骤

### 方法 1: 使用 CMake (推荐)

```bash
# 进入项目目录
cd E:\114514\语言\ZCC

# 创建构建目录
mkdir build
cd build

# 配置项目
cmake ..

# 构建
cmake --build . --config Release

# 运行测试
ctest

# 安装 (可选)
cmake --install . --prefix "C:\Program Files\ZCC"
```

### 方法 2: 使用 Visual Studio (Windows)

1. 打开 Visual Studio 2019/2022
2. 选择"打开本地文件夹"
3. 选择 `E:\114514\语言\ZCC` 目录
4. Visual Studio 会自动检测 CMakeLists.txt
5. 选择配置为"Release"
6. 按 F7 或选择"生成"->"全部生成"

### 方法 3: 使用命令行手动编译 (无 CMake)

#### Windows (MSVC)

```powershell
# 打开"Developer Command Prompt for VS 2022"

cd E:\114514\语言\ZCC

# 编译所有源文件
cl /std:c++17 /O2 /I include ^
   src\main.cpp ^
   src\Compiler.cpp ^
   src\Lexer.cpp ^
   src\Parser.cpp ^
   src\CodeGenerator.cpp ^
   /Fe:zcc.exe ^
   /link

# 运行
zcc.exe --help
```

#### Linux (GCC)

```bash
cd /path/to/ZCC

# 编译所有源文件
g++ -std=c++17 -O2 -I include \
    src/main.cpp \
    src/Compiler.cpp \
    src/Lexer.cpp \
    src/Parser.cpp \
    src/CodeGenerator.cpp \
    -o zcc

# 运行
./zcc --help
```

#### macOS (Clang)

```bash
cd /path/to/ZCC

# 编译所有源文件
clang++ -std=c++17 -O2 -I include \
    src/main.cpp \
    src/Compiler.cpp \
    src/Lexer.cpp \
    src/Parser.cpp \
    src/CodeGenerator.cpp \
    -o zcc

# 运行
./zcc --help
```

## 验证构建

构建成功后，应该能看到以下文件：

```
build/
├── zcc[.exe]           # 编译器可执行文件
├── zcc_test[.exe]      # 测试可执行文件 (如果启用了测试)
└── [其他中间文件]
```

运行以下命令验证：

```bash
# 查看版本
./zcc --version

# 查看帮助
./zcc --help

# 运行测试
./zcc_test
```

## 常见问题

### 问题 1: "找不到 CMake"
**解决**: 确保 CMake 已正确安装并添加到 PATH

### 问题 2: "C++17 不支持"
**解决**: 升级编译器到支持 C++17 的版本
- GCC: 7.0+
- Clang: 5.0+
- MSVC: 2017 15.3+

### 问题 3: "找不到头文件"
**解决**: 确保 include 目录路径正确

### 问题 4: 链接错误
**解决**: 确保所有源文件都被编译并链接

### 问题 5: Windows 编码问题
**解决**: 使用 `/utf-8` 编译选项 (MSVC) 或 `-finput-charset=UTF-8` (GCC/Clang)

## 性能优化

### 编译优化
```bash
# GCC/Clang
cmake -DCMAKE_BUILD_TYPE=Release ..

# MSVC
cmake -DCMAKE_CXX_FLAGS_RELEASE="/O2" ..
```

### 启用 LTO (链接时优化)
```bash
cmake -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON ..
```

## 交叉编译

### 编译为 WebAssembly (需要 Emscripten)

```bash
# 安装 Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# 编译
mkdir build-wasm && cd build-wasm
emcmake cmake ..
emmake make
```

## 清理构建

```bash
# 删除构建目录
rm -rf build

# 或清理 CMake 缓存
cd build
cmake --build . --target clean
```

## 下一步

构建成功后，您可以：

1. 阅读 [README.md](README.md) 了解使用方法
2. 查看 [examples/](examples/) 目录中的示例代码
3. 运行测试验证功能
4. 开始编写您的第一个 ZCC 程序！

---

如有问题，请访问项目 GitHub 或社区论坛获取帮助。

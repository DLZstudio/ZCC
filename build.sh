#!/bin/bash
# ZCC v0.2 Build Script
# Uses MinGW GCC 13.1.0 at C:/mingw1310

set -e

export PATH="/c/mingw1310/bin:$PATH"
mkdir -p build

SRCDIR="e:/114514/语言/ZCC/v0.2"
cd "$SRCDIR"

INCLUDES="-I include -I include/midend -I include/compiler -I include/backend -I include/core"
CXXFLAGS="-std=c++20 $INCLUDES"
PREFIX="C:/mingw1310"

echo "=== Compiling source files ==="

g++ $CXXFLAGS -c src/Lexer.cpp -o build/Lexer.o
g++ $CXXFLAGS -c src/Parser.cpp -o build/Parser.o
g++ $CXXFLAGS -c src/main.cpp -o build/main.o
g++ $CXXFLAGS -c src/compat_stubs.cpp -o build/compat_stubs.o
g++ $CXXFLAGS -c src/Compiler.cpp -o build/Compiler.o
g++ $CXXFLAGS -c src/CodeGenerator.cpp -o build/CodeGenerator.o
g++ $CXXFLAGS -c src/compiler/LanguageBackend.cpp -o build/LanguageBackend.o
g++ $CXXFLAGS -c src/midend/Optimizer.cpp -o build/Optimizer.o
g++ $CXXFLAGS -c src/backend/BackendManager.cpp -o build/BackendManager.o
g++ $CXXFLAGS -c src/backend/CBackend.cpp -o build/CBackend.o
g++ $CXXFLAGS -c src/backend/NativeBackend.cpp -o build/NativeBackend.o
g++ $CXXFLAGS -c src/backend/BIOSBackend.cpp -o build/BIOSBackend.o

echo "=== Linking ==="

g++ -std=c++20 -fno-use-linker-plugin -nostartfiles \
  "${PREFIX}/x86_64-w64-mingw32/lib/crt2.o" \
  "${PREFIX}/lib/gcc/x86_64-w64-mingw32/13.1.0/crtbegin.o" \
  build/Lexer.o build/Parser.o build/main.o build/compat_stubs.o build/Compiler.o \
  build/CodeGenerator.o build/LanguageBackend.o build/Optimizer.o \
  build/BackendManager.o build/CBackend.o build/NativeBackend.o build/BIOSBackend.o \
  -L${PREFIX}/x86_64-w64-mingw32/lib \
  -L${PREFIX}/lib/gcc/x86_64-w64-mingw32/13.1.0 \
  -lstdc++ -lmingw32 -lgcc_s -lgcc -lmoldname -lmingwex -lmsvcrt \
  -lkernel32 -lpthread -ladvapi32 -lshell32 -luser32 -liconv \
  "${PREFIX}/lib/gcc/x86_64-w64-mingw32/13.1.0/crtend.o" \
  -o build/zcc.exe

echo "=== Build complete: build/zcc.exe ==="
ls -la build/zcc.exe

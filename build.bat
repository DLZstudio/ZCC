@echo off
setlocal
set "PATH=D:\C++_Qt\Tools\mingw1310_64\bin;%PATH%"
cd /d D:\ZCC

echo Compiling ZCC Compiler...

echo [1/5] Compiling Lexer.cpp...
g++.exe -std=c++17 -I D:/ZCC/include D:/ZCC/src/Lexer.cpp -c -o D:/ZCC/Lexer.o
if not exist D:/ZCC/Lexer.o goto error

echo [2/5] Compiling Compiler.cpp...
g++.exe -std=c++17 -I D:/ZCC/include D:/ZCC/src/Compiler.cpp -c -o D:/ZCC/Compiler.o
if not exist D:/ZCC/Compiler.o goto error

echo [3/5] Compiling CodeGenerator.cpp...
g++.exe -std=c++17 -I D:/ZCC/include D:/ZCC/src/CodeGenerator.cpp -c -o D:/ZCC/CodeGenerator.o
if not exist D:/ZCC/CodeGenerator.o goto error

echo [4/5] Compiling Parser.cpp...
g++.exe -std=c++17 -I D:/ZCC/include D:/ZCC/src/Parser.cpp -c -o D:/ZCC/Parser.o
if not exist D:/ZCC/Parser.o goto error

echo [5/5] Compiling main.cpp and linking...
g++.exe -std=c++17 -static-libgcc -static-libstdc++ -I D:/ZCC/include D:/ZCC/src/main.cpp D:/ZCC/Lexer.o D:/ZCC/Compiler.o D:/ZCC/CodeGenerator.o D:/ZCC/Parser.o -o D:/ZCC/zcc.exe
if not exist D:/ZCC/zcc.exe goto error

echo.
echo ================================
echo ZCC Compiler built successfully!
echo ================================
echo.

goto end

:error
echo.
echo ================================
echo Build FAILED!
echo ================================
echo.
exit /b 1

:end
endlocal

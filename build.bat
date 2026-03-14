@echo off
setlocal
set "PATH=D:\C++_Qt\Tools\mingw1310_64\bin;%PATH%"
cd /d D:\ZCC

echo Compiling ZCC Compiler v0.2.0...
echo.

REM 清理旧文件
del *.o 2>nul
del zcc.exe 2>nul

echo [1/8] Compiling Lexer.cpp...
g++.exe -std=c++17 -I D:/ZCC/include D:/ZCC/src/Lexer.cpp -c -o D:/ZCC/Lexer.o
if not exist D:/ZCC/Lexer.o goto error

echo [2/8] Compiling Parser.cpp...
g++.exe -std=c++17 -I D:/ZCC/include D:/ZCC/src/Parser.cpp -c -o D:/ZCC/Parser.o
if not exist D:/ZCC/Parser.o goto error

echo [3/8] Compiling Compiler.cpp...
g++.exe -std=c++17 -I D:/ZCC/include D:/ZCC/src/Compiler.cpp -c -o D:/ZCC/Compiler.o
if not exist D:/ZCC/Compiler.o goto error

echo [4/8] Compiling Optimizer.cpp...
g++.exe -std=c++17 -I D:/ZCC/include D:/ZCC/src/midend/Optimizer.cpp -c -o D:/ZCC/Optimizer.o
if not exist D:/ZCC/Optimizer.o goto error

echo [5/8] Compiling BackendManager.cpp...
g++.exe -std=c++17 -I D:/ZCC/include D:/ZCC/src/backend/BackendManager.cpp -c -o D:/ZCC/BackendManager.o
if not exist D:/ZCC/BackendManager.o goto error

echo [6/8] Compiling CBackend.cpp...
g++.exe -std=c++17 -I D:/ZCC/include D:/ZCC/src/backend/CBackend.cpp -c -o D:/ZCC/CBackend.o
if not exist D:/ZCC/CBackend.o goto error

echo [7/8] Compiling NativeBackend.cpp...
g++.exe -std=c++17 -I D:/ZCC/include D:/ZCC/src/backend/NativeBackend.cpp -c -o D:/ZCC/NativeBackend.o
if not exist D:/ZCC/NativeBackend.o goto error

echo [8/8] Compiling BIOSBackend.cpp and linking...
g++.exe -std=c++17 -static-libgcc -static-libstdc++ -I D:/ZCC/include ^
    D:/ZCC/src/main.cpp ^
    D:/ZCC/Lexer.o D:/ZCC/Parser.o D:/ZCC/Compiler.o ^
    D:/ZCC/Optimizer.o ^
    D:/ZCC/BackendManager.o D:/ZCC/CBackend.o D:/ZCC/NativeBackend.o D:/ZCC/BIOSBackend.o ^
    -o D:/ZCC/zcc.exe
if not exist D:/ZCC/zcc.exe goto error

REM 清理目标文件
del *.o 2>nul

echo.
echo ================================
echo ZCC Compiler v0.2.0 built successfully!
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

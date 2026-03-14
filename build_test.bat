@echo off
setlocal
set "PATH=D:\C++_Qt\Tools\mingw1310_64\bin;%PATH%"
cd /d D:\ZCC
g++.exe -std=c++17 -I D:/ZCC/include D:/ZCC/src/Lexer.cpp -c -o D:/ZCC/Lexer.o
echo Exit code: %ERRORLEVEL%
if exist D:/ZCC/Lexer.o (
    echo SUCCESS
) else (
    echo FAILED
)
endlocal

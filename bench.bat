@echo off
cd /d "%~dp0"
set PATH=C:\raylib\w64devkit\bin;%PATH%
g++ -o benchmark.exe benchmark.cpp -O2 -Wall -std=c++17 -static
if %errorlevel%==0 benchmark.exe data\cities15000.txt
pause

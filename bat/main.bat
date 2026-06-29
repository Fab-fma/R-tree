@echo off
cd /d "%~dp0.."
set PATH=C:aylib\w64devkitin;%PATH%
g++ -o main.exe main.cpp -O2 -Wall -std=c++17 -static
if %errorlevel%==0 main.exe data\cities15000.txt
pause

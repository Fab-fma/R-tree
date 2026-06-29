@echo off
cd /d "%~dp0.."
set PATH=C:\raylib\w64devkit\bin;%PATH%
g++ -o visual_knn.exe visual_knn.cpp -O2 -Wall -std=c++17 ^
  -I C:\raylib\raylib\src -DPLATFORM_DESKTOP ^
  -lraylib -lopengl32 -lgdi32 -lwinmm
if %errorlevel%==0 visual_knn.exe data\cities1000.txt
pause

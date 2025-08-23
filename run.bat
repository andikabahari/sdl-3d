@echo off
setlocal

cd   .\res\shaders
call .\compile.bat
if %errorlevel% neq 0 (
    exit /b
)
echo.
cd   ..\..

if not exist .\build (
    call cmake -B build
)
call cmake --build build
if %errorlevel% neq 0 (
    exit /b
)
call .\build\Debug\app.exe

endlocal

@echo off
setlocal

echo Compiling GLSL shaders to SPIR-V...

where glslangValidator >nul 2>nul
if %errorlevel% neq 0 (
    echo glslangValidator not found.
    pause
    exit /b 1
)

for %%f in (*.glsl) do (
    glslangValidator -V "%%f" -o "%%~nf.spv"
)

endlocal

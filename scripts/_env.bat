@echo off
REM ============================================================
REM CTFontHook 公共环境设置（由其它脚本用 call 调用，不单独双击运行）
REM 作用：定位项目根目录与 Visual Studio 工具链；把临时目录指向项目内，避免占用 C 盘。
REM
REM 【重要】本目录下所有 .bat 必须保存为 GBK(codepage 936) + CRLF 换行。
REM 原因：cmd.exe 在 codepage 65001 下读取多行中文批处理会出现错位（已知缺陷），
REM       使用系统默认的 936 控制台 + GBK 文件才能真正稳定运行。
REM 修改脚本的正确方式：改完后用 scripts\tools\to_gbk.ps1 重新保存为 GBK。
REM ============================================================

REM --- 项目根目录 = 本脚本所在目录的上一级（scripts 的父目录）---
for %%I in ("%~dp0..") do set "ROOT=%%~fI"

set "VSROOT="
set "MSBUILD="
set "VCVARS="
set "DUMPBIN="
set "MSVCVER="

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT if exist "D:\VSInstaller\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VSROOT=D:\VSInstaller\BuildTools"
if not defined VSROOT if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VSROOT=C:\Program Files\Microsoft Visual Studio\2022\BuildTools"
if not defined VSROOT if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VSROOT=C:\Program Files\Microsoft Visual Studio\2022\Community"

REM 下面的赋值不能写进 ( ) 代码块：块内 %VAR% 会在整块解析时展开，会读到尚未赋值的旧值。
if not defined VSROOT goto :env_paths_done
set "MSBUILD=%VSROOT%\MSBuild\Current\Bin\MSBuild.exe"
set "VCVARS=%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
for /f "usebackq delims=" %%D in (`dir /b /ad "%VSROOT%\VC\Tools\MSVC" 2^>nul`) do set "MSVCVER=%%D"
if defined MSVCVER set "DUMPBIN=%VSROOT%\VC\Tools\MSVC\%MSVCVER%\bin\Hostx64\x64\dumpbin.exe"
:env_paths_done

REM --- 临时目录指向项目内，避免占用 C 盘 ---
set "TMP=%ROOT%\.tmp"
set "TEMP=%ROOT%\.tmp"
if not exist "%ROOT%\.tmp" mkdir "%ROOT%\.tmp" >nul 2>nul

exit /b 0

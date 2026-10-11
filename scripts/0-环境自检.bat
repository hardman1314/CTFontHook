@echo off
setlocal
call "%~dp0_env.bat"

echo ============================================================
echo    CTFontHook 环境自检
echo ============================================================
echo    项目根目录 : %ROOT%
echo.

set "OK=1"

echo [1] 编译工具 MSBuild
if exist "%MSBUILD%" (echo      通过 : %MSBUILD%) else (echo      失败 : 未找到 MSBuild & set "OK=0")

echo [2] C++ 工具链 vcvars64.bat
if exist "%VCVARS%" (echo      通过 : %VCVARS%) else (echo      失败 : 未找到 vcvars64.bat & set "OK=0")

echo [3] 导出表工具 dumpbin
if exist "%DUMPBIN%" (echo      通过 : %DUMPBIN%) else (echo      警告 : 未找到 dumpbin（仅影响导出校验）)

echo [4] Detours 依赖
if exist "%ROOT%\detours.h" (echo      通过 : detours.h) else (echo      失败 : 缺少 detours.h & set "OK=0")
if exist "%ROOT%\detours.lib" (echo      通过 : detours.lib) else (echo      失败 : 缺少 detours.lib & set "OK=0")
if exist "%ROOT%\detours_x64.lib" (echo      通过 : detours_x64.lib) else (echo      失败 : 缺少 detours_x64.lib & set "OK=0")

echo [5] 加壳工具 UPX
if exist "%ROOT%\tools\upx\upx.exe" (echo      通过 : tools\upx\upx.exe) else (echo      失败 : 缺少 upx.exe & set "OK=0")

echo [6] 工程文件
if exist "%ROOT%\CTFontHook.sln" (echo      通过 : CTFontHook.sln) else (echo      失败 : 缺少 CTFontHook.sln & set "OK=0")
if exist "%ROOT%\src\CTFontHook.vcxproj" (echo      通过 : src\CTFontHook.vcxproj) else (echo      失败 : 缺少 src\CTFontHook.vcxproj & set "OK=0")

echo [7] 版本管理 Git
where git >nul 2>nul && (echo      通过 : 已安装 Git) || (echo      失败 : 未找到 Git & set "OK=0")
if exist "%ROOT%\.git" (echo      通过 : 已初始化版本库) else (echo      提示 : 尚未初始化版本库)

echo.
if "%OK%"=="1" (echo 结论：环境完好，可以开始开发。) else (echo 结论：存在缺失项，请把上面「失败」的内容告诉开发方。)
echo ============================================================
pause

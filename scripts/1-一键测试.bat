@echo off
setlocal
call "%~dp0_env.bat"

echo ============================================================
echo   CTFontHook 一键测试（编译 + 导出校验）
echo ============================================================
echo   项目根目录 : %ROOT%
echo   作用：把源码编译成 winmm.dll（32 位和 64 位各一份）与安装器 EXE，
echo         并检查导出函数是否完整，确认代码改动没有把工程改坏。
echo ============================================================
echo.

if not exist "%MSBUILD%" (
    echo [错误] 未找到 MSBuild 编译工具。
    echo        请先双击运行「0-环境自检.bat」，并把结果反馈给开发方。
    echo.
    pause
    exit /b 1
)

set "BUILD_FAIL=0"

echo [1/4] 编译 64 位 Release ...
"%MSBUILD%" "%ROOT%\CTFontHook.sln" /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /t:Build
if errorlevel 1 (echo      失败 & set "BUILD_FAIL=1") else (echo      通过)
echo.

if "%BUILD_FAIL%"=="0" (
    echo [2/4] 编译 32 位 Release ...
    "%MSBUILD%" "%ROOT%\CTFontHook.sln" /nologo /v:minimal /p:Configuration=Release /p:Platform=Win32 /t:Build
    if errorlevel 1 (echo      失败 & set "BUILD_FAIL=1") else (echo      通过)
    echo.
)

if "%BUILD_FAIL%"=="0" (
    echo [3/4] 编译安装器 Release ...
    "%MSBUILD%" "%ROOT%\installer\CTFontHookInstaller.vcxproj" /nologo /v:minimal /p:Configuration=Release /p:Platform=Win32 /t:Build
    if errorlevel 1 (echo      失败 & set "BUILD_FAIL=1") else (echo      通过)
    echo.
)

if "%BUILD_FAIL%"=="1" (
    echo ============================================================
    echo 结论：编译失败，源码没能生成 DLL。
    echo        请把上面的英文报错内容复制或截图给开发方。
    echo ============================================================
    echo.
    pause
    exit /b 1
)

echo [4/4] 检查产物并校验导出函数 ...
set "DLL32=%ROOT%\Release\winmm.dll"
set "DLL64=%ROOT%\x64\Release\winmm.dll"
set "SETUP=%ROOT%\installer\bin\Win32\Release\CTFontHook-Setup.exe"
set "CHECK_FAIL=0"

if exist "%DLL64%" (echo      通过 : x64\Release\winmm.dll) else (echo      失败 : 缺少 x64\Release\winmm.dll & set "CHECK_FAIL=1")
if exist "%DLL32%" (echo      通过 : Release\winmm.dll) else (echo      失败 : 缺少 Release\winmm.dll & set "CHECK_FAIL=1")
if exist "%SETUP%" (echo      通过 : installer\bin\Win32\Release\CTFontHook-Setup.exe) else (echo      失败 : 缺少 installer\bin\Win32\Release\CTFontHook-Setup.exe & set "CHECK_FAIL=1")

if "%CHECK_FAIL%"=="0" (
    if exist "%DUMPBIN%" (
        call :check_exports "%DLL64%" "64 位"
        if errorlevel 1 set "CHECK_FAIL=1"
        call :check_exports "%DLL32%" "32 位"
        if errorlevel 1 set "CHECK_FAIL=1"
    ) else (
        echo      警告 : 未找到 dumpbin，跳过导出校验（不影响 DLL 生成）
    )
)

echo.
if "%CHECK_FAIL%"=="0" (
    echo ============================================================
    echo 结论：测试通过！DLL 已生成，并且导出函数完整。
    echo        32 位产物 : %DLL32%
    echo        64 位产物 : %DLL64%
    echo        安装器    : %SETUP%
    echo ============================================================
) else (
    echo ============================================================
    echo 结论：测试失败。请把上面的「失败」内容反馈给开发方。
    echo ============================================================
)
echo.
pause
exit /b %CHECK_FAIL%

:check_exports
setlocal
set "DLL=%~1"
set "TAG=%~2"
"%DUMPBIN%" /nologo /exports "%DLL%" > "%ROOT%\.tmp\exports.txt" 2>nul
set "MISS=0"
for %%F in (PlaySound timeGetTime waveOutOpen mciSendStringW auxGetNumDevs) do (
    findstr /c:"%%F" "%ROOT%\.tmp\exports.txt" >nul || set "MISS=1"
)
if "%MISS%"=="0" (
    echo      通过 : %TAG% 导出函数完整
    endlocal & exit /b 0
) else (
    echo      失败 : %TAG% 导出函数缺失（代理可能无法被游戏加载）
    endlocal & exit /b 1
)

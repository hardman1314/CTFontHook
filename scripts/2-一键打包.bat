@echo off
setlocal
call "%~dp0_env.bat"

echo ============================================================
echo   CTFontHook 一键打包
echo ============================================================
echo   项目根目录 : %ROOT%
echo   作用：把 32 位和 64 位 DLL 与安装器编译好，连同版本号、
echo         使用说明一起汇总成压缩包，方便发给用游戏的朋友。
echo ============================================================
echo.

if not exist "%MSBUILD%" (
    echo [错误] 未找到 MSBuild 编译工具。
    echo        请先双击运行「0-环境自检.bat」。
    echo.
    pause
    exit /b 1
)

echo [1/5] 编译 64 位 Release ...
"%MSBUILD%" "%ROOT%\CTFontHook.sln" /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /t:Build
if errorlevel 1 goto :build_fail
echo.

echo [2/5] 编译 32 位 Release ...
"%MSBUILD%" "%ROOT%\CTFontHook.sln" /nologo /v:minimal /p:Configuration=Release /p:Platform=Win32 /t:Build
if errorlevel 1 goto :build_fail
echo.

echo [3/5] 编译安装器 Release ...
"%MSBUILD%" "%ROOT%\installer\CTFontHookInstaller.vcxproj" /nologo /v:minimal /p:Configuration=Release /p:Platform=Win32 /t:Build
if errorlevel 1 goto :build_fail
echo.

set "DLL32=%ROOT%\Release\winmm.dll"
set "DLL64=%ROOT%\x64\Release\winmm.dll"
set "SETUP=%ROOT%\installer\bin\Win32\Release\CTFontHook-Setup.exe"
if not exist "%DLL32%" (echo [错误] 缺少 32 位产物：%DLL32% & echo. & pause & exit /b 1)
if not exist "%DLL64%" (echo [错误] 缺少 64 位产物：%DLL64% & echo. & pause & exit /b 1)
if not exist "%SETUP%" (echo [错误] 缺少安装器产物：%SETUP% & echo. & pause & exit /b 1)

echo [4/5] 汇总打包内容 ...
set "VER="
for /f "usebackq delims=" %%V in ("%ROOT%\VERSION") do set "VER=%%V"
if not defined VER set "VER=0.0.0"
for /f "usebackq delims=" %%T in (`powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmm"`) do set "STAMP=%%T"
if not defined STAMP set "STAMP=unknown"

set "PKGNAME=CTFontHook-%VER%-%STAMP%"
set "PKGDIR=%ROOT%\dist\%PKGNAME%"

if exist "%PKGDIR%" rmdir /s /q "%PKGDIR%"
mkdir "%PKGDIR%\32bit" >nul 2>nul
mkdir "%PKGDIR%\64bit" >nul 2>nul
copy /y "%DLL32%" "%PKGDIR%\32bit\winmm.dll" >nul
copy /y "%DLL64%" "%PKGDIR%\64bit\winmm.dll" >nul
copy /y "%SETUP%" "%PKGDIR%\CTFontHook-Setup.exe" >nul
echo      已放入 CTFontHook-Setup.exe 与 32bit\ 64bit\ 的 winmm.dll

> "%PKGDIR%\版本.txt" echo CTFontHook 版本 %VER%
>> "%PKGDIR%\版本.txt" echo 打包时间 %STAMP%

> "%PKGDIR%\使用说明.txt" echo CTFontHook 使用说明
>> "%PKGDIR%\使用说明.txt" echo ============================================
>> "%PKGDIR%\使用说明.txt" echo 本工具用于给游戏替换字体，让游戏里显示的字更好看。
>> "%PKGDIR%\使用说明.txt" echo ============================================
>> "%PKGDIR%\使用说明.txt" echo.
>> "%PKGDIR%\使用说明.txt" echo 【第一步】安装
>> "%PKGDIR%\使用说明.txt" echo     1. 双击运行本目录下的 CTFontHook-Setup.exe。
>> "%PKGDIR%\使用说明.txt" echo        如果弹出「用户账户控制」提示，请点「是」。
>> "%PKGDIR%\使用说明.txt" echo     2. 在窗口里点「浏览」，选中游戏的主程序（.exe 文件）。
>> "%PKGDIR%\使用说明.txt" echo        安装器会自动判断游戏是 32 位还是 64 位，并自动挑选对应的 winmm.dll。
>> "%PKGDIR%\使用说明.txt" echo     3. 点「安装」。安装器会自动完成下面这些事：
>> "%PKGDIR%\使用说明.txt" echo        - 备份游戏目录里原有的 winmm.dll / FontHook.ini；
>> "%PKGDIR%\使用说明.txt" echo        - 把对应位数的 winmm.dll 复制到游戏主程序同级目录；
>> "%PKGDIR%\使用说明.txt" echo        - 写入安装记录，方便以后一键还原。
>> "%PKGDIR%\使用说明.txt" echo.
>> "%PKGDIR%\使用说明.txt" echo 【第二步】使用
>> "%PKGDIR%\使用说明.txt" echo     1. 启动游戏，按 Insert 键打开字体选择器。
>> "%PKGDIR%\使用说明.txt" echo        如果 Insert 无反应，依次尝试 Pause 键、Scroll Lock 键。
>> "%PKGDIR%\使用说明.txt" echo     2. 按游戏文本的语言选择 中文 / 日文 / 繁体 / 韩文 字符集。
>> "%PKGDIR%\使用说明.txt" echo     3. 选择并应用自己喜欢的字体。
>> "%PKGDIR%\使用说明.txt" echo.
>> "%PKGDIR%\使用说明.txt" echo 【第三步】卸载 / 还原
>> "%PKGDIR%\使用说明.txt" echo     再次运行 CTFontHook-Setup.exe，点「卸载」即可：
>> "%PKGDIR%\使用说明.txt" echo        - 删除安装进去的 winmm.dll；
>> "%PKGDIR%\使用说明.txt" echo        - 如果之前有备份，自动还原成原来的 winmm.dll / FontHook.ini。
>> "%PKGDIR%\使用说明.txt" echo.
>> "%PKGDIR%\使用说明.txt" echo --------------------------------------------
>> "%PKGDIR%\使用说明.txt" echo 附录：手动安装（仅在安装器无法运行时使用）
>> "%PKGDIR%\使用说明.txt" echo --------------------------------------------
>> "%PKGDIR%\使用说明.txt" echo     1. 先确认游戏是 32 位还是 64 位：
>> "%PKGDIR%\使用说明.txt" echo        32 位游戏用 32bit\winmm.dll，64 位游戏用 64bit\winmm.dll。
>> "%PKGDIR%\使用说明.txt" echo        不确定时：先试 32bit，游戏打不开再换 64bit。
>> "%PKGDIR%\使用说明.txt" echo     2. 找到游戏主程序（.exe）所在文件夹。
>> "%PKGDIR%\使用说明.txt" echo     3. 如果该文件夹里已有 winmm.dll，先把它改名备份（例如 winmm_备份.dll）。
>> "%PKGDIR%\使用说明.txt" echo     4. 把对应位数的 winmm.dll 复制到这个文件夹（和 .exe 同级）。
>> "%PKGDIR%\使用说明.txt" echo     5. 卸载时：删除游戏目录里的 winmm.dll，并把之前备份的原 winmm.dll 改回原名。
>> "%PKGDIR%\使用说明.txt" echo.
>> "%PKGDIR%\使用说明.txt" echo 说明：配置保存在游戏目录的 FontHook.ini，删除它即可恢复默认。
echo      已生成 版本.txt 与 使用说明.txt

echo.
echo [5/5] 生成压缩包 ...
if exist "%ROOT%\dist\%PKGNAME%.zip" del /q "%ROOT%\dist\%PKGNAME%.zip"
powershell -NoProfile -Command "Compress-Archive -Path '%PKGDIR%\*' -DestinationPath '%ROOT%\dist\%PKGNAME%.zip' -Force"
if errorlevel 1 (echo [错误] 压缩失败。 & echo. & pause & exit /b 1)
echo      完成 : %ROOT%\dist\%PKGNAME%.zip

echo.
echo ============================================================
echo 结论：打包完成！
echo        压缩包位置：%ROOT%\dist\%PKGNAME%.zip
echo        （已为你打开 dist 文件夹）
echo ============================================================
start "" "%ROOT%\dist"
echo.
pause
exit /b 0

:build_fail
echo.
echo ============================================================
echo 结论：编译失败，没有生成压缩包。
echo        请把上面的英文报错内容复制或截图给开发方。
echo ============================================================
echo.
pause
exit /b 1

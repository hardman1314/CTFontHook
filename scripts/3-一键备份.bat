@echo off
setlocal
call "%~dp0_env.bat"

echo ============================================================
echo   CTFontHook 一键备份（Git 提交 + 打标签）
echo ============================================================
echo   项目根目录 : %ROOT%
echo   作用：把当前所有改动记录进版本库，并打一个可以随时回退的标签。
echo         每做一次备份，就相当于给项目拍一张「快照」。
echo ============================================================
echo.

where git >nul 2>nul
if errorlevel 1 (
    echo [错误] 未找到 Git。请先运行「0-环境自检.bat」。
    echo.
    pause
    exit /b 1
)

if not exist "%ROOT%\.git" (
    echo [提示] 版本库尚未初始化，正在自动初始化 ...
    git -C "%ROOT%" init >nul
    git -C "%ROOT%" branch -M master >nul 2>nul
    echo      完成。
    echo.
)

REM 若系统未配置 Git 身份，则本次提交临时使用占位身份（不修改全局配置）
set "GITID="
for /f "usebackq delims=" %%U in (`git config user.name 2^>nul`) do set "GITNAME=%%U"
if not defined GITNAME set "GITID=-c user.name=CTFontHook-dev -c user.email=dev@ctfonthook.local"

echo [1/4] 扫描改动 ...
git -C "%ROOT%" add -A
git -C "%ROOT%" diff --cached --quiet
if not errorlevel 1 (
    echo      没有检测到任何改动，不需要备份。
    echo.
    pause
    exit /b 0
)
echo      检测到以下改动：
git -C "%ROOT%" diff --cached --stat

echo.
echo [2/4] 填写本次备份说明
echo      例如：修复字体列表滚动条 / 新增某引擎适配 / 调整选择器界面
set "MSG="
set /p "MSG=说明(直接回车则用默认说明) > "
if "%MSG%"=="" set "MSG=自动备份"

for /f "usebackq delims=" %%T in (`powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"`) do set "STAMP=%%T"
if not defined STAMP set "STAMP=unknown"

echo.
echo [3/4] 提交改动 ...
git -C "%ROOT%" %GITID% commit -m "%MSG%" >nul
if errorlevel 1 (
    echo [错误] 提交失败，请把上面的报错内容反馈给开发方。
    echo.
    pause
    exit /b 1
)
echo      已提交 : %MSG%

echo.
echo [4/4] 打备份标签 ...
git -C "%ROOT%" tag "backup-%STAMP%"
if errorlevel 1 (
    echo [警告] 标签 backup-%STAMP% 已存在，本次未重复打标签。
) else (
    echo      已打标签 : backup-%STAMP%
)

echo.
echo ============================================================
echo 结论：备份完成。当前状态已保存，可随时用「4-一键回退.bat」还原。
echo        备份标签：backup-%STAMP%
echo ============================================================
echo.
pause
exit /b 0

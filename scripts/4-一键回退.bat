@echo off
setlocal
call "%~dp0_env.bat"

echo ============================================================
echo   CTFontHook 一键回退
echo ============================================================
echo   项目根目录 : %ROOT%
echo   作用：把项目内容还原到之前某个备份点。
echo         回退前会自动把「当前状态」也存一份，回退错了还能救回来。
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
    echo [错误] 版本库尚未初始化，还没有可回退的历史。
    echo        请先运行「3-一键备份.bat」建立第一个备份点。
    echo.
    pause
    exit /b 1
)

echo [可回退的备份点]（越靠上越新）
git -C "%ROOT%" tag -l "backup-*" --sort=-creatordate
echo.
echo [最近的提交记录]
git -C "%ROOT%" log --oneline -15
echo.

set "TAG="
set /p "TAG=请输入要回退到的备份点名称（直接回车取消）> "
if "%TAG%"=="" (
    echo 已取消，未做任何改动。
    echo.
    pause
    exit /b 0
)

git -C "%ROOT%" rev-parse -q --verify "refs/tags/%TAG%" >nul
if errorlevel 1 (
    echo.
    echo [错误] 找不到备份点：%TAG%
    echo        请从上面列表里复制一个完整名称（含 backup- 前缀）。
    echo.
    pause
    exit /b 1
)

echo.
echo [确认] 将把项目内容还原到 %TAG% 时的状态。
echo        注意：回退后，比它更新的改动会从项目里移走（但仍保留在安全标签里）。
set "ANS="
set /p "ANS=确定请输入 Y 再回车，其它键取消 > "
if /i not "%ANS%"=="Y" (
    echo 已取消，未做任何改动。
    echo.
    pause
    exit /b 0
)

set "GITID="
for /f "usebackq delims=" %%U in (`git config user.name 2^>nul`) do set "GITNAME=%%U"
if not defined GITNAME set "GITID=-c user.name=CTFontHook-dev -c user.email=dev@ctfonthook.local"

for /f "usebackq delims=" %%T in (`powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"`) do set "STAMP=%%T"
if not defined STAMP set "STAMP=unknown"

echo.
echo [1/2] 把当前状态存为安全标签 ...
git -C "%ROOT%" add -A
git -C "%ROOT%" %GITID% commit -m "回退前自动保存 %STAMP%" >nul 2>nul
git -C "%ROOT%" tag "safety-%STAMP%"
echo      安全标签 : safety-%STAMP%

echo.
echo [2/2] 回退到 %TAG% ...
git -C "%ROOT%" reset --hard "%TAG%" >nul
if errorlevel 1 (
    echo [错误] 回退失败，请把上面的报错内容反馈给开发方。
    echo.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo 结论：已回退到 %TAG%。
echo        如果发现回退错了，可以用安全标签还原回退前的状态：
echo        safety-%STAMP%
echo ============================================================
echo.
pause
exit /b 0

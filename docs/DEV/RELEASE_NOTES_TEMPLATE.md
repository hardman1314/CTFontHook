# Release 说明模板（时汐字体器 · CTFontHook）

> 用法：复制到 GitHub Release 的「Describe this release」框，或作为
> `gh release create ... --notes-file docs\DEV\RELEASE_NOTES_TEMPLATE.md` 的输入。
> 发布前把版本号与更新条目改成本次真实内容。步骤见 [RELEASE_GUIDE.md](RELEASE_GUIDE.md)。

---

## 时汐字体器 vX.Y.Z

给 Windows 游戏替换字体的工具：双击安装 → 选择游戏主程序 → 自动识别 32/64 位。

### 下载
- **安装包**：`CTFontHook-Setup-X.Y.Z.exe`（单文件，已内嵌 32/64 位 DLL，双击即可安装）

### 本次更新
- 修复：……
- 新增：……
- 优化：……

### 使用方法
1. 双击 `CTFontHook-Setup-X.Y.Z.exe`，若弹出「用户账户控制」请点「是」。
2. 点「浏览」选中游戏主程序（.exe），点「安装」。
3. 启动游戏，按 `Insert` 呼出字体选择器（无反应时依次试 `Pause`、`Scroll Lock`）。

### 卸载 / 还原
再次运行安装器，点「卸载 / 还原」，会自动恢复游戏原有的 `winmm.dll`。

### 说明
- 产物文件名固定为 `winmm.dll`（代理劫持机制要求）。
- 安装器会备份游戏目录原有 `winmm.dll` 到游戏目录下的 `.CTFontHook\backup\`。
- 本软件以 MIT 许可发布；第三方组件见仓库 `THIRD-PARTY-NOTICES.md`。
- 请在使用前阅读并遵守 `README.md` 中的免责声明。

// CTFontHook 安装器：控件 ID 定义（无对话资源，控件在代码中程序化创建）。
#pragma once

#define IDC_STATIC_TITLE      1001
#define IDC_STATIC_PATHLABEL  1002
#define IDC_EDIT_PATH         1003
#define IDC_BTN_BROWSE        1004
#define IDC_STATIC_ARCH       1005
#define IDC_BTN_INSTALL       1006
#define IDC_BTN_UNINSTALL     1007
#define IDC_BTN_EXIT          1008
#define IDC_STATIC_LOGLABEL   1009
#define IDC_EDIT_LOG          1010
#define IDC_STATIC_HINT       1011
#define IDC_STATIC_LISTLABEL  1012
#define IDC_LIST_GAMES        1013
#define IDC_STATIC_LISTEMPTY  1014
#define IDC_BTN_OPEN_DIR      1015
#define IDC_BTN_REMOVE        1016
#define IDC_BTN_REFRESH       1017
#define IDC_BTN_ADD_GAME      1018

// 应用图标（由 app.ico 提供多尺寸位图，EXE 文件图标与窗口图标共用）。
#define IDI_APPICON           2001

// 单文件安装包内嵌的 winmm.dll 载荷（RCDATA；取值 > 2048，与控件 ID 区间隔离）。
#define IDR_PAYLOAD_DLL32     3001
#define IDR_PAYLOAD_DLL64     3002

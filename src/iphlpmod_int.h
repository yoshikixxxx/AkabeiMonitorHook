/* 内部実装用ヘッダー */
#ifndef IPHLPMOD_INT_H
#define IPHLPMOD_INT_H

#include "iphlpmod.h"

/* 動的解決用関数型 */
typedef NTSTATUS (*LdrLoadDll_t)(WCHAR* PathToFile, ULONG Flags, UNICODE_STRING* ModuleFileName, HANDLE* ModuleHandle);
typedef NTSTATUS (*LdrGetProcedureAddress_t)(HANDLE ModuleHandle, ANSI_STRING* ProcName, ULONG Ordinal, PVOID* FunctionAddress);
typedef NTSTATUS (*NtProtectVirtualMemory_t)(HANDLE ProcessHandle, PVOID* BaseAddress, ULONG64* RegionSize, ULONG NewProtect, ULONG* OldProtect);

/* user32メニュー操作用の関数型 */
typedef int (*GetMenuItemCount_t)(PVOID hMenu);
typedef int (*GetMenuStringW_t)(PVOID hMenu, UINT uIDItem, WCHAR* lpString, int cchMax, UINT flags);
typedef PVOID (*GetSubMenu_t)(PVOID hMenu, int nPos);
typedef BOOL (*GetMenuItemInfoW_t)(PVOID hMenu, UINT item, BOOL byPos, PVOID mii);
typedef BOOL (*InsertMenuItemW_t)(PVOID hMenu, UINT item, BOOL byPos, PVOID mii);
typedef BOOL (*DeleteMenu_t)(PVOID hMenu, UINT item, UINT flags);
typedef BOOL (*TrackPopupMenuEx_t)(PVOID hMenu, UINT uFlags, int x, int y, PVOID hWnd, PVOID params);
typedef BOOL (*TrackPopupMenu_t)(PVOID hMenu, UINT uFlags, int x, int y, int reserved, PVOID hWnd, PVOID prc);

/* メニュー項目構造体 */
typedef uintptr_t ULONG_PTR;
typedef struct _MENUITEMINFOW {
    UINT cbSize;
    UINT fMask;
    UINT fType;
    UINT fState;
    UINT wID;
    PVOID hSubMenu;
    PVOID hbmpChecked;
    PVOID hbmpUnchecked;
    ULONG_PTR dwItemData;
    WCHAR* dwTypeData;
    UINT cch;
    PVOID hbmpItem;
} MENUITEMINFOW;

typedef struct _IF_MENU_ITEM {
    MENUITEMINFOW mii;
    WCHAR text[260];
    UINT textLen;
} IF_MENU_ITEM;

/* メニュー操作用定数 */
#define MF_BYPOSITION    0x00000400
#define MIIM_STATE       0x00000001
#define MIIM_ID          0x00000002
#define MIIM_SUBMENU     0x00000004
#define MIIM_CHECKMARKS  0x00000008
#define MIIM_TYPE        0x00000010
#define MIIM_DATA        0x00000020
#define MIIM_STRING      0x00000040
#define MIIM_BITMAP      0x00000080
#define MIIM_FTYPE       0x00000100
#define MFT_STRING       0x00000000
#define MFT_SEPARATOR    0x00000800

/* メモリ保護/DLLイベント定数 */
#define PAGE_READWRITE     0x04
#define DLL_PROCESS_ATTACH 1

#endif

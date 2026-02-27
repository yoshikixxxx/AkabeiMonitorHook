/*
 * 概要
 * この DLL は iphlpapi.dll の一部 API を中継して結果を整形する
 * GetIfTable2 は不要な行を除外して表示用の属性と名称を補正する
 * GetIpAddrTable は戻り値の並び順を安定化する
 * さらに user32 のメニュー表示 API をフックしてインターフェース一覧を名前順に並べ替える
 */

#include "iphlpmod.h"
#include "iphlpmod_int.h"

/* 内部宣言/共有状態 */
#ifdef _MSC_VER
unsigned long long __readgsqword(unsigned long Offset);
#pragma intrinsic(__readgsqword)
#endif

/* 内部関数宣言 */
static USHORT ascii_strlen(const char* s);
static char ascii_tolower(char c);
static int ascii_equals_nocase(const char* a, const char* b);
static int ascii_endswith_nocase(const char* s, const char* suf);
static uint16_t wtolower_ascii(uint16_t ch);
static int wcs_contains_ascii_nocase(const WCHAR* hay, const char* needle_ascii);
static int wcs_contains_char(const WCHAR* s, WCHAR ch);
static int wcs_contains_wstr(const WCHAR* hay, const WCHAR* needle);
static void wcs_copy(WCHAR* dst, const WCHAR* src, uint32_t max_chars);
static int wcs_equals_ascii_nocase(const WCHAR* s, const char* ascii);
static int wcs_cmp_ascii_nocase(const WCHAR* a, const WCHAR* b);
static int row_less(const MIB_IF_ROW2* a, const MIB_IF_ROW2* b);
static void sort_table_rows(MIB_IF_TABLE2* t);
static int iprow_less(const MIB_IPADDRROW* a, const MIB_IPADDRROW* b);
static void sort_ipaddr_rows(MIB_IPADDRTABLE* t);
static PEB* get_peb(void);
static void* get_export_by_name(void* module_base, const char* name);
static void* find_module_base(const char* base_name_ascii);
static int init_real(void);
static int is_req1(const WCHAR* desc);
static void normalize_for_visibility(MIB_IF_ROW2* row);
static int init_bootstrap(void);
static int iat_patch_one(void* imageBase, const char* dllName, const char* funcName, void* newFn, void** oldOut);
static void install_menu_hooks(void);
static int wcs_cmp_menu(const WCHAR* a, const WCHAR* b);
static void sort_if_submenu(PVOID hSub);
static void sort_interface_menu(PVOID hMenu);

/* フック関数宣言 */
BOOL TrackPopupMenuEx(PVOID hMenu, UINT uFlags, int x, int y, PVOID hWnd, PVOID params);
BOOL TrackPopupMenu(PVOID hMenu, UINT uFlags, int x, int y, int reserved, PVOID hWnd, PVOID prc);

/* 内部共有状態 */
static LdrLoadDll_t pLdrLoadDll = (LdrLoadDll_t)0;
static LdrGetProcedureAddress_t pLdrGetProc = (LdrGetProcedureAddress_t)0;
static NtProtectVirtualMemory_t pNtProtectVM = (NtProtectVirtualMemory_t)0;

static GetMenuItemCount_t pGetMenuItemCount = (GetMenuItemCount_t)0;
static GetMenuStringW_t pGetMenuStringW = (GetMenuStringW_t)0;
static GetSubMenu_t pGetSubMenu = (GetSubMenu_t)0;
static GetMenuItemInfoW_t pGetMenuItemInfoW = (GetMenuItemInfoW_t)0;
static InsertMenuItemW_t pInsertMenuItemW = (InsertMenuItemW_t)0;
static DeleteMenu_t pDeleteMenu = (DeleteMenu_t)0;

static TrackPopupMenuEx_t pTrackPopupMenuEx_real = (TrackPopupMenuEx_t)0;
static TrackPopupMenu_t pTrackPopupMenu_real = (TrackPopupMenu_t)0;

static HANDLE hIphlpapi = (HANDLE)0;
static ULONG (*pGetIfTable2_real)(MIB_IF_TABLE2** Table) = (ULONG (*)(MIB_IF_TABLE2**))0;
static ULONG (*pGetIpAddrTable_real)(PVOID Table, ULONG* Size, BOOL Order) = (ULONG (*)(PVOID, ULONG*, BOOL))0;
static void (*pFreeMibTable_real)(PVOID Memory) = (void (*)(PVOID))0;

static IF_MENU_ITEM g_if_items[128];
static int g_in_menu_hook = 0;

/* 固定文字列は冒頭に集約 */
static WCHAR g_iphlpapi_name[] = { 'i','p','h','l','p','a','p','i','.','d','l','l',0 };
static char g_proc_name_getiftable2[] = "GetIfTable2";
static char g_proc_name_getipaddrtable[] = "GetIpAddrTable";
static char g_proc_name_freemibtable[] = "FreeMibTable";
static WCHAR g_menu_title_iface_jp[] = { 0x30A4,0x30F3,0x30BF,0x30FC,0x30D5,0x30A7,0x30FC,0x30B9,0 };


/* メモリ領域をバイト単位でコピー */
void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

/* メモリ領域を指定値で埋める */
void* memset(void* dst, int c, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    uint8_t v = (uint8_t)c;
    for (size_t i = 0; i < n; i++) d[i] = v;
    return dst;
}

/* ASCII 文字列の長さを取得 */
static USHORT ascii_strlen(const char* s) {
    USHORT n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

/* ASCII 英字を小文字に変換 */
static char ascii_tolower(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c + 32);
    return c;
}

/* ASCII 文字列を大文字小文字を無視して比較 */
static int ascii_equals_nocase(const char* a, const char* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    for (;;) {
        char ac = *a++;
        char bc = *b++;
        if (ascii_tolower(ac) != ascii_tolower(bc)) return 0;
        if (ac == 0) return 1;
    }
}

/* ASCII 文字列の末尾一致を大文字小文字を無視して判定 */
static int ascii_endswith_nocase(const char* s, const char* suf) {
    USHORT sl = ascii_strlen(s);
    USHORT tl = ascii_strlen(suf);
    if (tl > sl) return 0;
    const char* p = s + (sl - tl);
    for (USHORT i = 0; i < tl; i++) {
        if (ascii_tolower(p[i]) != ascii_tolower(suf[i])) return 0;
    }
    return 1;
}

/* ASCII 範囲の UTF-16 文字を小文字に変換 */
static uint16_t wtolower_ascii(uint16_t ch) {
    if (ch >= 'A' && ch <= 'Z') return (uint16_t)(ch + 32);
    return ch;
}

/* ワイド文字列に ASCII 部分文字列が含まれるか判定 */
static int wcs_contains_ascii_nocase(const WCHAR* hay, const char* needle_ascii) {

    if (!hay || !needle_ascii || !needle_ascii[0]) return 0;

    uint32_t nlen = 0;
    while (needle_ascii[nlen]) nlen++;

    for (uint32_t i = 0; i <= IF_MAX_STRING_SIZE; i++) {
        if (hay[i] == 0) break;

        uint32_t j = 0;
        for (; j < nlen; j++) {
            WCHAR hc = hay[i + j];
            if (hc == 0) break;
            uint16_t hcl = wtolower_ascii((uint16_t)hc);
            uint16_t ncl = wtolower_ascii((uint16_t)needle_ascii[j]);
            if (hcl != ncl) break;
        }
        if (j == nlen) return 1;
    }
    return 0;
}

/* ワイド文字列に指定文字が含まれるか判定 */
static int wcs_contains_char(const WCHAR* s, WCHAR ch) {
    if (!s) return 0;
    for (uint32_t i = 0; i <= IF_MAX_STRING_SIZE; i++) {
        if (s[i] == 0) break;
        if (s[i] == ch) return 1;
    }
    return 0;
}

/* ワイド文字列にワイド文字列が含まれるか判定 */
static int wcs_contains_wstr(const WCHAR* hay, const WCHAR* needle) {
    if (!hay || !needle || needle[0] == 0) return 0;

    uint32_t nlen = 0;
    while (needle[nlen] && nlen < 64) nlen++;
    for (uint32_t i = 0; i <= IF_MAX_STRING_SIZE; i++) {
        if (hay[i] == 0) break;
        uint32_t j = 0;
        for (; j < nlen; j++) {
            WCHAR hc = hay[i + j];
            if (hc == 0) break;
            if (hc != needle[j]) break;
        }
        if (j == nlen) return 1;
    }
    return 0;
}

/* ワイド文字列を上限付きでコピー */
static void wcs_copy(WCHAR* dst, const WCHAR* src, uint32_t max_chars) {
    if (!dst) return;
    uint32_t i = 0;
    if (src) {
        for (; i < max_chars; i++) {
            WCHAR c = src[i];
            dst[i] = c;
            if (c == 0) return;
        }
    }

    dst[max_chars] = 0;
}

/* ワイド文字列と ASCII 文字列の完全一致を判定 */
static int wcs_equals_ascii_nocase(const WCHAR* s, const char* ascii) {
    if (!s || !ascii) return 0;
    uint32_t i = 0;
    for (;; i++) {
        WCHAR wc = s[i];
        char ac = ascii[i];
        if (ac == 0 && wc == 0) return 1;
        if (ac == 0 || wc == 0) return 0;
        if (wtolower_ascii((uint16_t)wc) != wtolower_ascii((uint16_t)ac)) return 0;
    }
}

/* ワイド文字列同士を大文字小文字を無視して比較 */
static int wcs_cmp_ascii_nocase(const WCHAR* a, const WCHAR* b) {

    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    for (uint32_t i = 0; i <= IF_MAX_STRING_SIZE; i++) {
        uint16_t ac = (uint16_t)a[i];
        uint16_t bc = (uint16_t)b[i];
        if (ac == 0 && bc == 0) return 0;
        if (ac == 0) return -1;
        if (bc == 0) return 1;
        ac = wtolower_ascii(ac);
        bc = wtolower_ascii(bc);
        if (ac < bc) return -1;
        if (ac > bc) return 1;
    }
    return 0;
}

/* インターフェース行の並び順を判定 */
static int row_less(const MIB_IF_ROW2* a, const MIB_IF_ROW2* b) {

    int c = wcs_cmp_ascii_nocase(a->Alias, b->Alias);
    if (c != 0) return (c < 0);

    if (a->InterfaceIndex != b->InterfaceIndex) return (a->InterfaceIndex < b->InterfaceIndex);

    return (a->InterfaceLuid.Value < b->InterfaceLuid.Value);
}

/* インターフェース行を挿入ソートで並べ替える */
static void sort_table_rows(MIB_IF_TABLE2* t) {

    ULONG n = t->NumEntries;
    for (ULONG i = 1; i < n; i++) {
        MIB_IF_ROW2 key;
        memcpy(&key, &t->Table[i], sizeof(MIB_IF_ROW2));
        ULONG j = i;
        while (j > 0 && row_less(&key, &t->Table[j - 1])) {
            memcpy(&t->Table[j], &t->Table[j - 1], sizeof(MIB_IF_ROW2));
            j--;
        }
        if (j != i) {
            memcpy(&t->Table[j], &key, sizeof(MIB_IF_ROW2));
        }
    }
}

/* IP アドレス行の並び順を判定 */
static int iprow_less(const MIB_IPADDRROW* a, const MIB_IPADDRROW* b) {

    if (a->dwIndex != b->dwIndex) return (a->dwIndex < b->dwIndex);

    if (a->dwAddr != b->dwAddr) return (a->dwAddr < b->dwAddr);

    return (a->dwMask < b->dwMask);
}

/* IP アドレス行を挿入ソートで並べ替える */
static void sort_ipaddr_rows(MIB_IPADDRTABLE* t) {
    uint32_t n = t->dwNumEntries;
    for (uint32_t i = 1; i < n; i++) {
        MIB_IPADDRROW key;
        memcpy(&key, &t->table[i], sizeof(MIB_IPADDRROW));
        uint32_t j = i;
        while (j > 0 && iprow_less(&key, &t->table[j - 1])) {
            memcpy(&t->table[j], &t->table[j - 1], sizeof(MIB_IPADDRROW));
            j--;
        }
        if (j != i) {
            memcpy(&t->table[j], &key, sizeof(MIB_IPADDRROW));
        }
    }
}

/* 現在プロセスの PEB を取得 */
static PEB* get_peb(void) {
    return (PEB*)(uintptr_t)__readgsqword(0x60);
}

/* モジュールのエクスポート関数を名前で解決 */
static void* get_export_by_name(void* module_base, const char* name) {
    if (!module_base || !name) return NULL;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)module_base;
    if (dos->e_magic != 0x5A4D) return NULL;
    IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)((uint8_t*)module_base + dos->e_lfanew);
    if (nt->Signature != 0x00004550) return NULL;

    IMAGE_DATA_DIRECTORY expdir = nt->OptionalHeader.DataDirectory[0];
    if (expdir.VirtualAddress == 0 || expdir.Size == 0) return NULL;

    IMAGE_EXPORT_DIRECTORY* exp = (IMAGE_EXPORT_DIRECTORY*)((uint8_t*)module_base + expdir.VirtualAddress);
    uint32_t* names = (uint32_t*)((uint8_t*)module_base + exp->AddressOfNames);
    uint16_t* ords  = (uint16_t*)((uint8_t*)module_base + exp->AddressOfNameOrdinals);
    uint32_t* funcs = (uint32_t*)((uint8_t*)module_base + exp->AddressOfFunctions);

    for (uint32_t i = 0; i < exp->NumberOfNames; i++) {
        char* n = (char*)((uint8_t*)module_base + names[i]);

        uint32_t j = 0;
        for (;; j++) {
            char a = n[j];
            char b = name[j];
            if (a != b) break;
            if (a == 0) {
                uint16_t ord = ords[i];
                uint32_t rva = funcs[ord];
                return (uint8_t*)module_base + rva;
            }
        }
    }
    return NULL;
}

/* ロード済みモジュールのベースアドレスを取得 */
static void* find_module_base(const char* base_name_ascii) {
    PEB* peb = get_peb();
    if (!peb || !peb->Ldr) return NULL;

    LIST_ENTRY* head = &peb->Ldr->InMemoryOrderModuleList;
    for (LIST_ENTRY* e = head->Flink; e && e != head; e = e->Flink) {
        LDR_DATA_TABLE_ENTRY* ent = (LDR_DATA_TABLE_ENTRY*)((uint8_t*)e - (uintptr_t)(&((LDR_DATA_TABLE_ENTRY*)0)->InMemoryOrderLinks));
        if (!ent->BaseDllName.Buffer) continue;
        if (wcs_equals_ascii_nocase(ent->BaseDllName.Buffer, base_name_ascii)) {
            return ent->DllBase;
        }
    }
    return NULL;
}

/* 実体の iphlpapi 関数を動的に初期化 */
static int init_real(void) {
    if (pGetIfTable2_real && pFreeMibTable_real && pGetIpAddrTable_real) return 1;

    void* ntdll = find_module_base("ntdll.dll");
    if (!ntdll) return 0;

    if (!pLdrLoadDll) {
        pLdrLoadDll = (LdrLoadDll_t)get_export_by_name(ntdll, "LdrLoadDll");
        pLdrGetProc = (LdrGetProcedureAddress_t)get_export_by_name(ntdll, "LdrGetProcedureAddress");
    }
    if (!pLdrLoadDll || !pLdrGetProc) return 0;

    if (!hIphlpapi) {
        UNICODE_STRING us;
        us.Buffer = g_iphlpapi_name;

        us.Length = (USHORT)(12 * 2);
        us.MaximumLength = (USHORT)(13 * 2);
        HANDLE mod = NULL;
        if (pLdrLoadDll(NULL, 0, &us, &mod) != 0) {
            return 0;
        }
        hIphlpapi = mod;
    }

    ANSI_STRING as;
    PVOID addr;

    as.Buffer = g_proc_name_getiftable2; as.Length = ascii_strlen(g_proc_name_getiftable2); as.MaximumLength = (USHORT)(as.Length + 1);
    addr = NULL;
    if (pLdrGetProc(hIphlpapi, &as, 0, &addr) != 0 || !addr) return 0;
    pGetIfTable2_real = (ULONG (*)(MIB_IF_TABLE2**))addr;

    as.Buffer = g_proc_name_getipaddrtable; as.Length = ascii_strlen(g_proc_name_getipaddrtable); as.MaximumLength = (USHORT)(as.Length + 1);
    addr = NULL;
    if (pLdrGetProc(hIphlpapi, &as, 0, &addr) != 0 || !addr) return 0;
    pGetIpAddrTable_real = (ULONG (*)(PVOID, ULONG*, BOOL))addr;

    as.Buffer = g_proc_name_freemibtable; as.Length = ascii_strlen(g_proc_name_freemibtable); as.MaximumLength = (USHORT)(as.Length + 1);
    addr = NULL;
    if (pLdrGetProc(hIphlpapi, &as, 0, &addr) != 0 || !addr) return 0;
    pFreeMibTable_real = (void (*)(PVOID))addr;

    return 1;
}

/* 表示補正対象の説明文字列か判定 */
static int is_req1(const WCHAR* desc) {
    return wcs_contains_ascii_nocase(desc, "vpn") ||
           wcs_contains_ascii_nocase(desc, "tap") ||
           wcs_contains_ascii_nocase(desc, "microsoft network adapter multiplexor");
}

/* 表示用にインターフェース属性を補正 */
static void normalize_for_visibility(MIB_IF_ROW2* row) {

    row->Type = IF_TYPE_ETHERNET_CSMACD;
    row->MediaType = NdisMedium802_3;

    row->PhysicalMediumType = 0;
    row->InterfaceAndOperStatusFlags.HardwareInterface = 1;
    row->InterfaceAndOperStatusFlags.FilterInterface = 0;
}

/* メニューフックに必要な関数群を初期化 */
static int init_bootstrap(void) {

    void* ntdll = find_module_base("ntdll.dll");
    if (!ntdll) return 0;
    if (!pLdrLoadDll) {
        pLdrLoadDll = (LdrLoadDll_t)get_export_by_name(ntdll, "LdrLoadDll");
        pLdrGetProc = (LdrGetProcedureAddress_t)get_export_by_name(ntdll, "LdrGetProcedureAddress");
    }
    if (!pNtProtectVM) {
        pNtProtectVM = (NtProtectVirtualMemory_t)get_export_by_name(ntdll, "NtProtectVirtualMemory");
    }
    if (!pLdrLoadDll || !pLdrGetProc || !pNtProtectVM) return 0;

    void* user32 = find_module_base("user32.dll");
    if (!user32) return 0;
    if (!pGetMenuItemCount) {
        pGetMenuItemCount = (GetMenuItemCount_t)get_export_by_name(user32, "GetMenuItemCount");
        pGetMenuStringW = (GetMenuStringW_t)get_export_by_name(user32, "GetMenuStringW");
        pGetSubMenu = (GetSubMenu_t)get_export_by_name(user32, "GetSubMenu");
        pGetMenuItemInfoW = (GetMenuItemInfoW_t)get_export_by_name(user32, "GetMenuItemInfoW");
        pInsertMenuItemW = (InsertMenuItemW_t)get_export_by_name(user32, "InsertMenuItemW");
        pDeleteMenu = (DeleteMenu_t)get_export_by_name(user32, "DeleteMenu");

        if (!pTrackPopupMenuEx_real) pTrackPopupMenuEx_real = (TrackPopupMenuEx_t)get_export_by_name(user32, "TrackPopupMenuEx");
        if (!pTrackPopupMenu_real) pTrackPopupMenu_real = (TrackPopupMenu_t)get_export_by_name(user32, "TrackPopupMenu");
    }
    return (pGetMenuItemCount && pGetMenuStringW && pGetSubMenu && pGetMenuItemInfoW && pInsertMenuItemW && pDeleteMenu);
}

/* IAT の 1 エントリを書き換えてフックを差し込む */
static int iat_patch_one(void* imageBase, const char* dllName, const char* funcName, void* newFn, void** oldOut) {
    if (!imageBase) return 0;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)imageBase;
    if (dos->e_magic != 0x5A4D) return 0;
    IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)((uint8_t*)imageBase + dos->e_lfanew);
    if (nt->Signature != 0x00004550) return 0;

    IMAGE_DATA_DIRECTORY impdir = nt->OptionalHeader.DataDirectory[1];
    if (impdir.VirtualAddress == 0 || impdir.Size == 0) return 0;

    IMAGE_IMPORT_DESCRIPTOR* imp = (IMAGE_IMPORT_DESCRIPTOR*)((uint8_t*)imageBase + impdir.VirtualAddress);
    for (; imp->Name; imp++) {
        const char* name = (const char*)((uint8_t*)imageBase + imp->Name);
        if (!ascii_endswith_nocase(name, dllName)) continue;

        IMAGE_THUNK_DATA64* oft = (IMAGE_THUNK_DATA64*)((uint8_t*)imageBase + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        IMAGE_THUNK_DATA64* ft  = (IMAGE_THUNK_DATA64*)((uint8_t*)imageBase + imp->FirstThunk);

        for (; oft->AddressOfData; oft++, ft++) {
            if (oft->Ordinal & IMAGE_ORDINAL_FLAG64) continue;
            IMAGE_IMPORT_BY_NAME* ibn = (IMAGE_IMPORT_BY_NAME*)((uint8_t*)imageBase + (uint32_t)oft->AddressOfData);
            const char* iname = (const char*)ibn->Name;

            uint32_t j = 0;
            for (;; j++) {
                char a = iname[j];
                char b = funcName[j];
                if (a != b) break;
                if (a == 0) {

                    if (oldOut) *oldOut = (void*)(uintptr_t)ft->Function;
                    if (pNtProtectVM) {
                        PVOID base = (PVOID)&ft->Function;
                        ULONG64 size = sizeof(uint64_t);
                        ULONG oldProt = 0;

                        HANDLE cur = (HANDLE)(uintptr_t)(~(uintptr_t)0);
                        if (pNtProtectVM(cur, &base, &size, PAGE_READWRITE, &oldProt) == 0) {
                            ft->Function = (uint64_t)(uintptr_t)newFn;

                            pNtProtectVM(cur, &base, &size, oldProt, &oldProt);
                            return 1;
                        }
                    }

                    return 0;
                }
            }
        }
    }
    return 0;
}

/* 実行ファイルの IAT にメニューフックを組み込む */
static void install_menu_hooks(void) {
    if (!init_bootstrap()) return;
    void* exeBase = get_peb()->ImageBaseAddress;

    (void)iat_patch_one(exeBase, "user32.dll", "TrackPopupMenuEx", (void*)&TrackPopupMenuEx, (void**)&pTrackPopupMenuEx_real);
    (void)iat_patch_one(exeBase, "user32.dll", "TrackPopupMenu", (void*)&TrackPopupMenu, (void**)&pTrackPopupMenu_real);
}

/* メニュー文字列の比較を行う */
static int wcs_cmp_menu(const WCHAR* a, const WCHAR* b) {
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    for (uint32_t i = 0; i < 512; i++) {
        uint16_t ac = (uint16_t)a[i];
        uint16_t bc = (uint16_t)b[i];
        if (ac == 0 && bc == 0) return 0;
        if (ac == 0) return -1;
        if (bc == 0) return 1;
        ac = wtolower_ascii(ac);
        bc = wtolower_ascii(bc);
        if (ac < bc) return -1;
        if (ac > bc) return 1;
    }
    return 0;
}

/* インターフェースサブメニューの項目を並べ替える */
static void sort_if_submenu(PVOID hSub) {
    if (!hSub || !pGetMenuItemCount || !pGetMenuStringW || !pGetMenuItemInfoW || !pInsertMenuItemW || !pDeleteMenu) return;

    int n = pGetMenuItemCount(hSub);
    if (n <= 1) return;
    if (n > 128) n = 128;

    int keep = 0;

    for (int i = 0; i < n; i++) {
        MENUITEMINFOW m;

        for (uint32_t z = 0; z < sizeof(MENUITEMINFOW); z++) ((uint8_t*)&m)[z] = 0;
        m.cbSize = sizeof(MENUITEMINFOW);
        m.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU | MIIM_DATA;
        if (!pGetMenuItemInfoW(hSub, (UINT)i, 1, &m)) {
            continue;
        }
        if ((m.fType & MFT_SEPARATOR) == MFT_SEPARATOR) {
            continue;
        }

        WCHAR buf[260];
        for (int k = 0; k < 260; k++) buf[k] = 0;
        int len = 0;
        if (pGetMenuStringW) {
            len = pGetMenuStringW(hSub, (UINT)i, buf, 259, MF_BYPOSITION);
        }
        if (len <= 0) {
            continue;
        }

        g_if_items[keep].mii = m;
        g_if_items[keep].textLen = (UINT)len;
        for (int k = 0; k < 259; k++) {
            g_if_items[keep].text[k] = buf[k];
            if (buf[k] == 0) break;
        }
        g_if_items[keep].text[259] = 0;
        keep++;
    }
    if (keep <= 1) return;

    for (int i = 1; i < keep; i++) {
        IF_MENU_ITEM key = g_if_items[i];
        int j = i;
        while (j > 0) {
            int c = wcs_cmp_menu(key.text, g_if_items[j - 1].text);
            if (c == 0) {
                if (key.mii.wID >= g_if_items[j - 1].mii.wID) break;
            } else if (c > 0) {
                break;
            }
            g_if_items[j] = g_if_items[j - 1];
            j--;
        }
        g_if_items[j] = key;
    }

    int total = pGetMenuItemCount(hSub);
    for (int i = total - 1; i >= 0; i--) {
        pDeleteMenu(hSub, (UINT)i, MF_BYPOSITION);
    }

    for (int i = 0; i < keep; i++) {
        MENUITEMINFOW m = g_if_items[i].mii;
        m.fMask |= MIIM_STRING;
        m.dwTypeData = g_if_items[i].text;
        m.cch = g_if_items[i].textLen;
        (void)pInsertMenuItemW(hSub, (UINT)i, 1, &m);
    }
}

/* 対象メニュー内からインターフェース項目を見つけて整列 */
static void sort_interface_menu(PVOID hMenu) {
    if (!hMenu || !pGetMenuItemCount || !pGetMenuStringW || !pGetSubMenu) return;

    int n = pGetMenuItemCount(hMenu);
    if (n <= 0) return;
    for (int i = 0; i < n; i++) {
        WCHAR title[128];
        for (int k = 0; k < 128; k++) title[k] = 0;
        int len = pGetMenuStringW(hMenu, (UINT)i, title, 127, MF_BYPOSITION);
        if (len <= 0) continue;
        if (wcs_contains_wstr(title, g_menu_title_iface_jp) || wcs_contains_ascii_nocase(title, "interface")) {
            PVOID sub = pGetSubMenu(hMenu, i);
            if (sub) sort_if_submenu(sub);
            return;
        }
    }
}

/* TrackPopupMenuEx を横取りして表示前にメニューを整列 */
BOOL TrackPopupMenuEx(PVOID hMenu, UINT uFlags, int x, int y, PVOID hWnd, PVOID params) {
    if (!pTrackPopupMenuEx_real) {

        init_bootstrap();
    }
    if (!g_in_menu_hook) {
        g_in_menu_hook = 1;
        sort_interface_menu(hMenu);
        g_in_menu_hook = 0;
    }
    return pTrackPopupMenuEx_real ? pTrackPopupMenuEx_real(hMenu, uFlags, x, y, hWnd, params) : 0;
}

/* TrackPopupMenu を横取りして表示前にメニューを整列 */
BOOL TrackPopupMenu(PVOID hMenu, UINT uFlags, int x, int y, int reserved, PVOID hWnd, PVOID prc) {
    if (!pTrackPopupMenu_real) {
        init_bootstrap();
    }
    if (!g_in_menu_hook) {
        g_in_menu_hook = 1;
        sort_interface_menu(hMenu);
        g_in_menu_hook = 0;
    }
    return pTrackPopupMenu_real ? pTrackPopupMenu_real(hMenu, uFlags, x, y, reserved, hWnd, prc) : 0;
}

/* GetIpAddrTable を中継して必要時だけ並び順を補正 */
ULONG GetIpAddrTable(PVOID Table, ULONG* Size, BOOL Order) {
    if (!init_real()) return (ULONG)111;

    ULONG st = pGetIpAddrTable_real(Table, Size, Order);
    if (st != 0) return st;
    if (!Table || !Size) return st;

    if (!Order) {
        MIB_IPADDRTABLE* t = (MIB_IPADDRTABLE*)Table;
        if (t->dwNumEntries > 1) {
            /* 並び順を安定化して呼び出し側の表示差分を減らす */
            sort_ipaddr_rows(t);
        }
    }
    return st;
}

/* iphlpapi の FreeMibTable をそのまま中継 */
void FreeMibTable(PVOID Memory) {
    if (!init_real() || !pFreeMibTable_real) return;
    pFreeMibTable_real(Memory);
}

/* GetIfTable2 を中継して一覧を整形して返す */
ULONG GetIfTable2(MIB_IF_TABLE2** Table) {
    if (!init_real()) return (ULONG)87;

    ULONG st = pGetIfTable2_real(Table);
    if (st != 0 || !Table || !*Table) return st;

    MIB_IF_TABLE2* t = *Table;

    ULONG write = 0;
    ULONG n = t->NumEntries;

    for (ULONG i = 0; i < n; i++) {
        MIB_IF_ROW2* row = &t->Table[i];
        const WCHAR* desc = row->Description;

        if (wcs_contains_ascii_nocase(desc, "0000")) {
            /* 説明名に 0000 を含む行は除外 */
            continue;
        }

        if (row->Type == IF_TYPE_WWANPP || row->Type == IF_TYPE_WWANPP2) {
            if (wcs_contains_char(desc, (WCHAR)'#')) {
                /* 連番付きの派生行は除外 */
                continue;
            }

            normalize_for_visibility(row);
        }

        if (is_req1(desc)) {
            normalize_for_visibility(row);
        }

        /* Alias は Description と同じ値にそろえる */
        wcs_copy(row->Alias, row->Description, IF_MAX_STRING_SIZE);

        if (write != i) {
            memcpy(&t->Table[write], row, sizeof(MIB_IF_ROW2));
        }
        write++;
    }

    /* 有効行数に更新して最後に名前順で整列 */
    t->NumEntries = write;
    if (write > 1) {
        sort_table_rows(t);
    }
    return st;
}

/* DLL 読み込み時にメニューフックを導入 */
BOOL DllMain(HMODULE hModule, ULONG ul_reason_for_call, PVOID lpReserved) {
    (void)hModule;
    (void)lpReserved;
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        /* 初回ロード時だけフックを導入 */
        install_menu_hooks();
    }
    return 1;
}

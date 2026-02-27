#ifndef IPHLPMOD_H
#define IPHLPMOD_H

/* DLL公開用マクロ */
#ifndef IPHLPMOD_API
#if defined(_WIN32) || defined(_WIN64)
#if defined(IPHLPMOD_EXPORTS)
#define IPHLPMOD_API __declspec(dllexport)
#else
#define IPHLPMOD_API __declspec(dllimport)
#endif
#else
#define IPHLPMOD_API
#endif
#endif

/* 基本型定義 */
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef uint64_t           uintptr_t;

typedef unsigned long long size_t;

typedef unsigned long      ULONG;
typedef unsigned long long ULONG64;
typedef unsigned short     USHORT;
typedef unsigned short     WCHAR;
typedef unsigned char      UCHAR;
typedef int                BOOL;
typedef unsigned int       UINT;

typedef void*              PVOID;
typedef PVOID              HANDLE;
typedef PVOID              HMODULE;

typedef uint32_t           NTSTATUS;

#define NULL ((void*)0)

typedef struct _LIST_ENTRY {
    struct _LIST_ENTRY* Flink;
    struct _LIST_ENTRY* Blink;
} LIST_ENTRY;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    WCHAR* Buffer;
} UNICODE_STRING;

typedef struct _ANSI_STRING {
    USHORT Length;
    USHORT MaximumLength;
    char*  Buffer;
} ANSI_STRING;

typedef struct _PEB_LDR_DATA {
    ULONG      Length;
    UCHAR      Initialized;
    PVOID      SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA;

typedef struct _PEB {
    UCHAR      InheritedAddressSpace;
    UCHAR      ReadImageFileExecOptions;
    UCHAR      BeingDebugged;
    UCHAR      BitField;
    PVOID      Mutant;
    PVOID      ImageBaseAddress;
    PEB_LDR_DATA* Ldr;
} PEB;

typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID      DllBase;
    PVOID      EntryPoint;
    ULONG      SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
} LDR_DATA_TABLE_ENTRY;

typedef struct _IMAGE_DOS_HEADER {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    int32_t  e_lfanew;
} IMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} IMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY {
    uint32_t VirtualAddress;
    uint32_t Size;
} IMAGE_DATA_DIRECTORY;

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16

typedef struct _IMAGE_OPTIONAL_HEADER64 {
    uint16_t Magic;
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} IMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_NT_HEADERS64 {
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64;

typedef struct _IMAGE_EXPORT_DIRECTORY {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;
    uint32_t Base;
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;
    uint32_t AddressOfNames;
    uint32_t AddressOfNameOrdinals;
} IMAGE_EXPORT_DIRECTORY;

typedef struct _IMAGE_IMPORT_DESCRIPTOR {
    uint32_t OriginalFirstThunk;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;
    uint32_t FirstThunk;
} IMAGE_IMPORT_DESCRIPTOR;

typedef union _IMAGE_THUNK_DATA64 {
    uint64_t ForwarderString;
    uint64_t Function;
    uint64_t Ordinal;
    uint64_t AddressOfData;
} IMAGE_THUNK_DATA64;

typedef struct _IMAGE_IMPORT_BY_NAME {
    uint16_t Hint;
    char Name[1];
} IMAGE_IMPORT_BY_NAME;

#define IMAGE_ORDINAL_FLAG64 0x8000000000000000ULL

#define IF_MAX_STRING_SIZE 256
#define IF_MAX_PHYS_ADDRESS_LENGTH 32

#define IF_TYPE_ETHERNET_CSMACD 6
#define IF_TYPE_WWANPP 243
#define IF_TYPE_WWANPP2 244

#define NdisMedium802_3 0

typedef struct _IF_OPER_STATUS_FLAGS {
    UCHAR HardwareInterface : 1;
    UCHAR FilterInterface : 1;
    UCHAR ConnectorPresent : 1;
    UCHAR NotAuthenticated : 1;
    UCHAR NotMediaConnected : 1;
    UCHAR Paused : 1;
    UCHAR LowPower : 1;
    UCHAR EndPointInterface : 1;
} IF_OPER_STATUS_FLAGS;

typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID;

typedef union _NET_LUID {
    uint64_t Value;
    struct {
        uint64_t Reserved : 24;
        uint64_t NetLuidIndex : 24;
        uint64_t IfType : 16;
    } Info;
} NET_LUID;

typedef GUID NET_IF_NETWORK_GUID;

typedef struct _MIB_IF_ROW2 {
    NET_LUID  InterfaceLuid;
    uint32_t  InterfaceIndex;
    GUID      InterfaceGuid;
    WCHAR     Alias[IF_MAX_STRING_SIZE + 1];
    WCHAR     Description[IF_MAX_STRING_SIZE + 1];
    ULONG     PhysicalAddressLength;
    UCHAR     PhysicalAddress[IF_MAX_PHYS_ADDRESS_LENGTH];
    UCHAR     PermanentPhysicalAddress[IF_MAX_PHYS_ADDRESS_LENGTH];
    ULONG     Mtu;
    uint32_t  Type;
    uint32_t  TunnelType;
    uint32_t  MediaType;
    uint32_t  PhysicalMediumType;
    uint32_t  AccessType;
    uint32_t  DirectionType;
    IF_OPER_STATUS_FLAGS InterfaceAndOperStatusFlags;
    uint32_t  OperStatus;
    uint32_t  AdminStatus;
    uint32_t  MediaConnectState;
    NET_IF_NETWORK_GUID NetworkGuid;
    uint32_t  ConnectionType;
    ULONG64   TransmitLinkSpeed;
    ULONG64   ReceiveLinkSpeed;
    ULONG64   InOctets;
    ULONG64   InUcastPkts;
    ULONG64   InNUcastPkts;
    ULONG64   InDiscards;
    ULONG64   InErrors;
    ULONG64   InUnknownProtos;
    ULONG64   InUcastOctets;
    ULONG64   InMulticastOctets;
    ULONG64   InBroadcastOctets;
    ULONG64   OutOctets;
    ULONG64   OutUcastPkts;
    ULONG64   OutNUcastPkts;
    ULONG64   OutDiscards;
    ULONG64   OutErrors;
    ULONG64   OutUcastOctets;
    ULONG64   OutMulticastOctets;
    ULONG64   OutBroadcastOctets;
    ULONG64   OutQLen;
} MIB_IF_ROW2;

typedef struct _MIB_IF_TABLE2 {
    ULONG NumEntries;
    MIB_IF_ROW2 Table[1];
} MIB_IF_TABLE2;

typedef struct _MIB_IPADDRROW {
    uint32_t dwAddr;
    uint32_t dwIndex;
    uint32_t dwMask;
    uint32_t dwBCastAddr;
    uint32_t dwReasmSize;
    uint16_t unused1;
    uint16_t wType;
} MIB_IPADDRROW;

typedef struct _MIB_IPADDRTABLE {
    uint32_t dwNumEntries;
    MIB_IPADDRROW table[1];
} MIB_IPADDRTABLE;


/* 公開API宣言 */
#ifdef __cplusplus
extern "C" {
#endif

IPHLPMOD_API ULONG GetIfTable2(MIB_IF_TABLE2** Table);
IPHLPMOD_API ULONG GetIpAddrTable(PVOID Table, ULONG* Size, BOOL Order);
IPHLPMOD_API void FreeMibTable(PVOID Memory);
BOOL DllMain(HMODULE hModule, ULONG ul_reason_for_call, PVOID lpReserved);

#ifdef __cplusplus
}
#endif

#endif

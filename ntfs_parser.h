#ifndef NTFS_PARSER_H
#define NTFS_PARSER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Helper macros
#define NTFS_STATEMENT(statement) \
    do {                          \
        statement                 \
    } while(0)

#define NTFS_UNUSED(name)           (void) (name)
#define NTFS_CAST(type, value)      ((type) (value))
#define NTFS_RETURN(name, value)    \
    NTFS_STATEMENT((name) = (value); goto skip;)

#define NTFS_WSTRINGIFY_(s) L ## s
#define NTFS_WSTRINGIFY(s)  NTFS_WSTRINGIFY_(s)

#ifndef NTFS_MEM_COPY
    #define NTFS_MEM_COPY(dest, dest_size, src, src_size) \
        memcpy_s(dest, dest_size, src, src_size)
#endif

#ifndef NTFS_ASSERT
    #define NTFS_ASSERT(cond, msg)                                      \
        NTFS_STATEMENT(                                                 \
            if (!(cond)) {                                              \
                NTFS__Win32Log(NTFS_WSTRINGIFY(msg),                    \
                               NTFS_WSTRINGIFY(__FILE__), __LINE__);    \
                __debugbreak();                                         \
            }                                                           \
        )
#endif

#ifndef NTFS_API
    #define NTFS_API
#endif

// Arena APIs
typedef struct {
    void  *Buffer;
    size_t ReservedSize;
    size_t CommittedSize;
    size_t Offset;
} ntfs_arena;

typedef struct {
    size_t Size;
} ntfs_arena_header;

#define NTFS__ARENA_KILOBYTE(value) (value * 1024)
#define NTFS__ARENA_MEGABYTE(value) (value * 1024 * 1024)
#define NTFS__ARENA_DEFAULT_COMMIT   NTFS__ARENA_MEGABYTE(1)
#define NTFS__ARENA_DEFAULT_RESERVED NTFS__ARENA_MEGABYTE(16)

NTFS_API ntfs_arena NTFS__ArenaDefault(void);
NTFS_API void       NTFS__ArenaDestroy(ntfs_arena *Arena);
NTFS_API void      *NTFS__ArenaAlloc(ntfs_arena *Arena, size_t Size);
NTFS_API void      *NTFS__PushCopyWStringZ(ntfs_arena *Arena, uint16_t *String, size_t Length);
NTFS_API void      *NTFS__ArenaResizeAlloc(ntfs_arena *Arena, void *Address, size_t Size);
NTFS_API void       NTFS__ArenaReset(ntfs_arena *Arena);

// Dynamic list API
typedef struct {
    size_t Capacity;
    size_t Length;
} ntfs_list_header;

#define NTFS__LIST_DEFAULT_SIZE  64
#define NTFS__ListHeader(list)   (&NTFS_CAST(ntfs_list_header *, (list))[-1])
#define NTFS__ListLen(list)      ((list) ? (NTFS__ListHeader(list)->Length) : 0)
#define NTFS__ListCap(list)      ((list) ? (NTFS__ListHeader(list)->Capacity) : 0)
#define NTFS__ListPush(arena, list, item) \
    ((list) = NTFS__ListGrow(arena, list, sizeof(item)), \
     (list)[NTFS__ListHeader(list)->Length++] = (item))

NTFS_API void *NTFS__ListGrow(ntfs_arena *Arena, void *List, size_t ItemSize);


typedef enum {
    NTFS_Error_Success,
    NTFS_Error_MemoryError,

    // Volume related errors
    NTFS_Error_VolumeOpen,
    NTFS_Error_VolumeReadBootRecord,
    NTFS_Error_VolumeUnknownSignature,
    NTFS_Error_VolumePartitionNotFound,
    NTFS_Error_VolumeFailedValidation,
    NTFS_Error_VolumeFailedLoadInfoFile,
    NTFS_Error_VolumeUnsupportedVersion,
    NTFS_Error_VolumeFailedLoadCaseTable,

    // File related errors
    NTFS_Error_RecordFailedRead,
    NTFS_Error_RecordFailedValidation,
    NTFS_Error_FileFailedInfoValidation,
    NTFS_Error_FileReadDataAttrNotFound,
    NTFS_Error_FileReadFailed,
} ntfs_error;

static inline char *NTFS_ErrorToString(ntfs_error Error)
{
    switch (Error) {
    case NTFS_Error_Success:                   return "ntfs success";
    case NTFS_Error_MemoryError:               return "ntfs failed to allocate memory";
    case NTFS_Error_VolumeOpen:                return "ntfs failed opening handle to volume";
    case NTFS_Error_VolumeReadBootRecord:      return "ntfs failed reading volume boot record";
    case NTFS_Error_VolumeUnknownSignature:    return "ntfs failed unknown volume signature";
    case NTFS_Error_VolumePartitionNotFound:   return "ntfs failed partition was not found";
    case NTFS_Error_VolumeFailedValidation:    return "ntfs failed volume fields validation";
    case NTFS_Error_VolumeFailedLoadInfoFile:  return "ntfs failed volume load information file";
    case NTFS_Error_VolumeUnsupportedVersion:  return "ntfs failed volume unsupported version";
    case NTFS_Error_VolumeFailedLoadCaseTable: return "ntfs failed volume load case table";
    case NTFS_Error_RecordFailedRead:          return "ntfs failed reading mft file record";
    case NTFS_Error_RecordFailedValidation:    return "ntfs failed file record validation";
    case NTFS_Error_FileFailedInfoValidation:  return "ntfs failed file validation extra info";
    case NTFS_Error_FileReadDataAttrNotFound:  return "ntfs failed file unnamed data attribute was not found";
    case NTFS_Error_FileReadFailed:            return "ntfs failed file read";
    }

    return "";
}

// Volume API
typedef struct {
    ntfs_error Error;
    void      *Handle;

    uint64_t StartOffset;
    uint64_t SectorsPerCluster;
    uint64_t MftCluster;
    uint64_t BytesPerSector;
    uint64_t BytesPerCluster;
    uint64_t BytesPerMftEntry;
    uint64_t SerialNumber;

    uint16_t  Name[128];
    uint16_t *CaseTable;
} ntfs_volume;

#define NTFS_BOOT_RECORD_SIZE                512
#define NTFS_BOOT_RECORD_SIGNATURE           0xAA55
#define NTFS_BOOT_RECORD_PARTITION_OFFSET    0x01BE
#define NTFS_BOOT_RECORD_PARITION_ENTRY_SIZE 0x10

NTFS_API ntfs_volume NTFS_VolumeOpen(wchar_t DriveLetter);
NTFS_API ntfs_volume NTFS_VolumeOpenFromFile(wchar_t *Path);
NTFS_API void        NTFS_VolumeClose(ntfs_volume *Volume);
NTFS_API bool        NTFS_VolumeRead(ntfs_volume *Volume, uint64_t From,
                                     void *Buffer, size_t Size);

NTFS_API ntfs_volume NTFS__VolumeLoad(void *VolumeHandle, size_t VbrOffset);
NTFS_API void        NTFS__VolumeLoadInformation(ntfs_volume *Volume);

enum {
    NTFS_SystemFile_Mft        =  0,
    NTFS_SystemFile_MftMirror  =  1,
    NTFS_SystemFile_LogFile    =  2,
    NTFS_SystemFile_Volume     =  3,
    NTFS_SystemFile_AttrDef    =  4,
    NTFS_SystemFile_RootFolder =  5,
    NTFS_SystemFile_Bitmap     =  6,
    NTFS_SystemFile_Boot       =  7,
    NTFS_SystemFile_BadClus    =  8,
    NTFS_SystemFile_Secure     =  9,
    NTFS_SystemFile_UpCase     = 10,
    NTFS_SystemFile_Extend     = 11,
};

enum {
    NTFS_AttributeFlag_Compressed = 0x0001,
    NTFS_AttributeFlag_Encrypted  = 0x4000,
    NTFS_AttributeFlag_Sparse     = 0x8000,
};

typedef enum {
    NTFS_AttributeType_StandardInformation =  0x10,
    NTFS_AttributeType_AttributeList       =  0x20,
    NTFS_AttributeType_FileName            =  0x30,
    NTFS_AttributeType_VolumeVersion       =  0x40,
    NTFS_AttributeType_SecurityDescriptor  =  0x50,
    NTFS_AttributeType_VolumeName          =  0x60,
    NTFS_AttributeType_VolumeInformation   =  0x70,
    NTFS_AttributeType_Data                =  0x80,
    NTFS_AttributeType_IndexRoot           =  0x90,
    NTFS_AttributeType_IndexAllocation     =  0xA0,
    NTFS_AttributeType_Bitmap              =  0xB0,
    NTFS_AttributeType_SymbolicLink        =  0xC0,
    NTFS_AttributeType_EaInformation       =  0xD0,
    NTFS_AttributeType_Ea                  =  0xE0,
    NTFS_AttributeType_PropertySet         =  0xF0,
    NTFS_AttributeType_LoggedUtilityStream = 0x100,
} ntfs_attr_type;

static inline char *NTFS_AttrTypeToString(ntfs_attr_type Type)
{
    switch (Type) {
        case NTFS_AttributeType_StandardInformation: return "Standard Information";
        case NTFS_AttributeType_AttributeList:       return "Attribute List";
        case NTFS_AttributeType_FileName:            return "File Name";
        case NTFS_AttributeType_VolumeVersion:       return "Volume Version";
        case NTFS_AttributeType_SecurityDescriptor:  return "Security Descriptor";
        case NTFS_AttributeType_VolumeName:          return "Volume Name";
        case NTFS_AttributeType_VolumeInformation:   return "Volume Information";
        case NTFS_AttributeType_Data:                return "Data";
        case NTFS_AttributeType_IndexRoot:           return "Index Root";
        case NTFS_AttributeType_IndexAllocation:     return "Index Allocation";
        case NTFS_AttributeType_Bitmap:              return "Bitmap";
        case NTFS_AttributeType_SymbolicLink:        return "Symbolic Link";
        case NTFS_AttributeType_EaInformation:       return "Ea Information";
        case NTFS_AttributeType_Ea:                  return "Ea";
        case NTFS_AttributeType_PropertySet:         return "Property Set";
        case NTFS_AttributeType_LoggedUtilityStream: return "Logged Utility Stream";
    }

    return "(unknown)";
}

enum {
    NTFS_FileFlags_ReadOnly          = 0x0001,
    NTFS_FileFlags_Hidden            = 0x0002,
    NTFS_FileFlags_System            = 0x0004,
    NTFS_FileFlags_Archive           = 0x0020,
    NTFS_FileFlags_Device            = 0x0040,
    NTFS_FileFlags_Normal            = 0x0080,
    NTFS_FileFlags_Temporary         = 0x0100,
    NTFS_FileFlags_SparseFile        = 0x0200,
    NTFS_FileFlags_ReparsePoint      = 0x0400,
    NTFS_FileFlags_Compressed        = 0x0800,
    NTFS_FileFlags_Offline           = 0x1000,
    NTFS_FileFlags_NotContentIndexed = 0x2000,
    NTFS_FileFlags_Encrypted         = 0x4000,
};

typedef struct {
    uint64_t StartVCN;
    uint64_t Count;
} ntfs_data_run;

typedef struct {
    ntfs_attr_type Type;
    bool           NonResFlag;
    uint8_t        NameLength;
    uint16_t      *Name;
    uint16_t       Flags;
    uint16_t       Id;

    struct {
        uint8_t *Data;
        uint32_t Size;
    } Resident;

    struct {
        uint64_t Size;
        uint64_t AlignedSize;
        ntfs_data_run *RunList;
    } NonResident;
} ntfs_attr;

typedef struct {
    ntfs_error Error;

    uint64_t  Index;
    uint8_t   *Buffer;
    ntfs_attr *AttrList;
    bool      IsDir;
} ntfs_record;

typedef struct {
    ntfs_error   Error;
    ntfs_arena   Arena;
    ntfs_volume *Volume;

    ntfs_record Record;

    uint64_t CreationTime;
    uint64_t ModifiedTime;
    uint64_t ChangedTime;
    uint64_t ReadTime;
    union {
        struct {
            uint32_t ReadOnly          : 1;
            uint32_t Hidden            : 1;
            uint32_t System            : 1;
            uint32_t _                 : 2;
            uint32_t Archive           : 1;
            uint32_t Device            : 1;
            uint32_t Normal            : 1;
            uint32_t Temporary         : 1;
            uint32_t SparseFile        : 1;
            uint32_t ReparsePoint      : 1;
            uint32_t Compressed        : 1;
            uint32_t Offline           : 1;
            uint32_t NotContentIndexed : 1;
            uint32_t Encrypted         : 1;
        } F;

        uint32_t Value;
    } Flags;
    uint64_t  ParentIndex;
    uint64_t  AlignedSize;
    uint64_t  Size;
    uint16_t *Name;
} ntfs_file;

#define NTFS_FILE_RECORD_MAGIC           0x454C4946
#define NTFS_FILE_RECORD_ATTR_END_MARKER 0xFFFFFFFF

NTFS_API ntfs_file NTFS_FileOpenFromIndex(ntfs_volume *Volume, size_t Index);
NTFS_API void      NTFS_FileClose(ntfs_file *File);
NTFS_API size_t    NTFS_FileRead(ntfs_file *File, uint64_t Offset,
                                 uint8_t *Buffer, size_t Size);

NTFS_API ntfs_record    NTFS__RecordLoadFromIndex(ntfs_volume *Volume,
                                                  ntfs_arena *Arena,
                                                  size_t Index);
NTFS_API ntfs_data_run *NTFS__DataRunsLoad(ntfs_arena *Arena,
                                           void *Buffer, size_t Size);

#endif   // NTFS_PARSER_H


#ifdef NTFS_PARSER_IMPLEMENTATION

// Helper functions
static inline bool NTFS__IsPowerOf2(uint64_t Value)
{
    bool Result = (Value != 0) && (Value & (Value - 1)) == 0;
    return Result;
}

static inline bool NTFS__IsPageAligned(size_t Value)
{
    const size_t PageSize = 4096;

    bool Result = (Value & (PageSize - 1)) == 0;
    return Result;
}

static inline size_t NTFS__Align(size_t Value, size_t Alignment)
{
    size_t Result = Value;
    if (Result % Alignment) {
        Result += (Alignment - (Result % Alignment));
    }

    return Result;
}

static inline bool NTFS__IsAligned(size_t Value, size_t Alignment)
{
    bool Result = false;

    if (NTFS__IsPowerOf2(Alignment)) {
        Result = (Value & (Alignment - 1)) == 0;
    } else {
        Result = (Value % Alignment) == 0;
    }

    return Result;
}

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Platform specific APIs
static void  NTFS__Win32Log(wchar_t *Message, wchar_t *FileName, size_t Line);
static void *NTFS__Win32FileOpen(wchar_t *FilePath);
static bool  NTFS__Win32FileRead(void *Handle, uint64_t Offset, void *Buffer, size_t Size);
static void *NTFS__Win32MemoryAllocate(size_t Size, size_t CommittedSize);
static void *NTFS__Win32MemoryCommit(void *Address, size_t Size);
static bool  NTFS__Win32MemoryFree(void *Address);

static void NTFS__Win32Log(wchar_t *Message, wchar_t *FileName, size_t Line)
{
    DWORD_PTR pArgs[] = {
        NTFS_CAST(DWORD_PTR, FileName),
        NTFS_CAST(DWORD_PTR, Line),
        NTFS_CAST(DWORD_PTR, Message),
    };
    LPWSTR Buffer = 0;
    DWORD Flags   = FORMAT_MESSAGE_FROM_STRING
                  | FORMAT_MESSAGE_ARGUMENT_ARRAY
                  | FORMAT_MESSAGE_ALLOCATE_BUFFER;

    DWORD  Length = FormatMessageW(Flags, L"NTFS_ASSERT! %1:%2!d! - %3\n", 0, 0,
                                   NTFS_CAST(LPWSTR, &Buffer), 0, (va_list*)pArgs);
    HANDLE Stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    WriteConsoleW(Stdout, Buffer, Length, 0, 0);

    LocalFree(Buffer);
}

static void *NTFS__Win32FileOpen(wchar_t *FilePath)
{
    HANDLE Result =
        CreateFileW(FilePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    0, OPEN_EXISTING, 0, 0);
    if (Result == INVALID_HANDLE_VALUE) {
        Result = 0;  // Normalize
    }

    return Result;
}

static bool NTFS__Win32FileRead(void *Handle, uint64_t Offset, void *Buffer, size_t Size)
{
    NTFS_ASSERT(Size == NTFS_CAST(uint32_t, Size), "Not supporting reading 64bit size");

    OVERLAPPED Overlapped = {
        .Offset     = Offset  & 0xFFFFFFFF,
        .OffsetHigh = Offset >> 32,
    };
    DWORD BytesRead = 0;
    BOOL  Result    =
        ReadFile(Handle, Buffer, NTFS_CAST(DWORD, Size), &BytesRead, &Overlapped);
    return Result && BytesRead == Size;
}

static void *NTFS__Win32MemoryAllocate(size_t Size, size_t CommittedSize)
{
    NTFS_ASSERT(CommittedSize <= Size, "Committed size cannot be larger from total allocation size");
    NTFS_ASSERT(NTFS__IsPageAligned(Size), "Reserved size needs to be page aligned");

    void *Result = VirtualAlloc(0, Size, MEM_RESERVE, PAGE_READWRITE);
    if (Result) {
        CommittedSize = (CommittedSize == 0) ? Size : CommittedSize;
        Result        = NTFS__Win32MemoryCommit(Result, CommittedSize);
    }

    return Result;
}

static void *NTFS__Win32MemoryCommit(void *Address, size_t Size)
{
    NTFS_ASSERT(NTFS__IsPageAligned(Size), "Committed size needs to be page aligned");

    void *Result = VirtualAlloc(Address, Size, MEM_COMMIT, PAGE_READWRITE);
    return Result;
}

static bool NTFS__Win32MemoryFree(void *Address)
{
    bool Result = VirtualFree(Address, 0, MEM_FREE) != 0;
    return Result;
}


// Arena APIs
ntfs_arena NTFS__ArenaDefault(void)
{
    ntfs_arena Result    = { 0 };
    Result.ReservedSize  = NTFS__ARENA_DEFAULT_RESERVED;
    Result.CommittedSize = NTFS__ARENA_DEFAULT_COMMIT;
    Result.Buffer        =
        NTFS__Win32MemoryAllocate(Result.ReservedSize, Result.CommittedSize);

    return Result;
}

void NTFS__ArenaDestroy(ntfs_arena *Arena)
{
    NTFS__Win32MemoryFree(Arena->Buffer);
    *Arena = (ntfs_arena) { 0 };
}

void *NTFS__ArenaAlloc(ntfs_arena *Arena, size_t Size)
{
    size_t SizeAligned = NTFS__Align(Size + sizeof(ntfs_arena_header), sizeof(void *));

    // TODO: Change to support growing arena when needed instead of crashing
    NTFS_ASSERT(Arena->Offset + SizeAligned <= Arena->ReservedSize,
                "growing arenas are not supported");
    while (Arena->CommittedSize < Arena->ReservedSize
           && (Arena->Offset + SizeAligned) >= Arena->CommittedSize) {

        Arena->CommittedSize *= 2;
        if (Arena->CommittedSize > Arena->ReservedSize) {
            Arena->CommittedSize = Arena->ReservedSize;
        }

        NTFS__Win32MemoryCommit(Arena->Buffer, Arena->CommittedSize);
    }

    uint8_t *Result = &NTFS_CAST(uint8_t *, Arena->Buffer)[Arena->Offset];
    Arena->Offset  += SizeAligned;

    NTFS_CAST(ntfs_arena_header *, Result)->Size = SizeAligned;
    Result += sizeof(ntfs_arena_header);

    return Result;
}

void *NTFS__PushCopyWStringZ(ntfs_arena *Arena, uint16_t *String, size_t Length)
{
    size_t SrcSize  = Length * sizeof(String[0]);
    size_t DestSize = SrcSize + sizeof(String[0]);
    void  *Result = NTFS__ArenaAlloc(Arena, DestSize);

    NTFS_MEM_COPY(Result, DestSize, String, SrcSize);
    NTFS_CAST(uint16_t *, Result)[Length] = 0x00;
    return Result;
}

void *NTFS__ArenaResizeAlloc(ntfs_arena *Arena, void *Address, size_t Size)
{
    ntfs_arena_header *Header =
        NTFS_CAST(ntfs_arena_header *,
                  &NTFS_CAST(uint8_t *, Address)[0-sizeof(*Header)]);

    void *Result = &NTFS_CAST(uint8_t *, Arena->Buffer)[(Arena->Offset - Header->Size)];
    if (Result == Header) {  // Provided allocation is last allocation
        size_t SizeAligned = NTFS__Align(Size + sizeof(*Header), sizeof(void *));
        Arena->Offset = Arena->Offset - Header->Size + SizeAligned;
        Header->Size  = SizeAligned;
        Result        = NTFS_CAST(uint8_t *, Result) + sizeof(*Header);

    } else {
        Result = NTFS__ArenaAlloc(Arena, Size);
        NTFS_MEM_COPY(Result, Header->Size, Address, Size);
    }

    return Result;
}

void NTFS__ArenaReset(ntfs_arena *Arena)
{
    Arena->Offset = 0;
}

void *NTFS__ListGrow(ntfs_arena *Arena, void *List, size_t ItemSize)
{
    ntfs_list_header *Result = NTFS__ListHeader(List);
    if (List == 0) {
        size_t Size      = sizeof(*Result) + NTFS__LIST_DEFAULT_SIZE * ItemSize;
        Result           = NTFS__ArenaAlloc(Arena, Size);
        Result->Capacity = NTFS__LIST_DEFAULT_SIZE;
        Result->Length   = 0;

    } else if (Result->Length == Result->Capacity) {
        Result->Capacity *= 2;
        size_t NewSize    = sizeof(*Result) + Result->Capacity * ItemSize;
        Result = NTFS__ArenaResizeAlloc(Arena, Result, NewSize);
    }

    return NTFS_CAST(uint8_t *, Result) + sizeof(*Result);
}


// Volume API
ntfs_volume NTFS_VolumeOpen(wchar_t DriveLetter)
{
    ntfs_volume Result = { 0 };

    wchar_t DrivePath[] = L"\\\\.\\ :";
    DrivePath[4]        = DriveLetter;
    void *VolumeHandle  = NTFS__Win32FileOpen(DrivePath);
    if (VolumeHandle == 0) {
        NTFS_RETURN(Result.Error, NTFS_Error_VolumeOpen);
    }

    Result = NTFS__VolumeLoad(VolumeHandle, 0);

skip:
    return Result;
}

ntfs_volume NTFS_VolumeOpenFromFile(wchar_t *Path)
{
    ntfs_volume Result = { 0 };

    void *VolumeHandle = NTFS__Win32FileOpen(Path);
    if (VolumeHandle == 0) {
        NTFS_RETURN(Result.Error, NTFS_Error_VolumeOpen);
    }

    uint8_t BootSector[NTFS_BOOT_RECORD_SIZE];
    if (!NTFS__Win32FileRead(VolumeHandle, 0, &BootSector, sizeof(BootSector))) {
        NTFS_RETURN(Result.Error, NTFS_Error_VolumeReadBootRecord);
    }

    uint16_t Signature = *NTFS_CAST(uint16_t *, &BootSector[510]);
    if (Signature != NTFS_BOOT_RECORD_SIGNATURE) {
        NTFS_RETURN(Result.Error, NTFS_Error_VolumeUnknownSignature);
    }

    uint8_t *PartitionTable = &BootSector[NTFS_BOOT_RECORD_PARTITION_OFFSET];
    Result.Error            = NTFS_Error_VolumePartitionNotFound;
    for (int i = 0; i < 4; i++) {
        uint8_t PartitionType = PartitionTable[0x04];
        if (PartitionType != 0) {
            uint32_t FirstSector = *NTFS_CAST(uint32_t *, &PartitionTable[0x08]);
            Result = NTFS__VolumeLoad(VolumeHandle, FirstSector * sizeof(BootSector));
            break;
        }

        PartitionTable += NTFS_BOOT_RECORD_PARITION_ENTRY_SIZE;
    }

skip:
    return Result;
}

void NTFS_VolumeClose(ntfs_volume *Volume)
{
    if (Volume->Handle) {
        CloseHandle(Volume->Handle);
    }

    // TODO: Remove once volume will have better arena handling
    if (Volume->CaseTable) {
        NTFS__Win32MemoryFree(Volume->CaseTable);
    }

    // Dont override the error
    *Volume = (ntfs_volume) { .Error = Volume->Error};
}

bool NTFS_VolumeRead(ntfs_volume *Volume, uint64_t From, void *Buffer, size_t Size)
{
    NTFS_ASSERT(NTFS__IsAligned(From, Volume->BytesPerSector),
                "volume read offset is not aligned to volume sector size");
    NTFS_ASSERT(NTFS__IsAligned(Size, Volume->BytesPerSector),
                "volume read size is not aligned to volume sector size");

    bool Result = NTFS__Win32FileRead(Volume->Handle, From + Volume->StartOffset,
                                      Buffer, Size);
    return Result;
}

ntfs_volume NTFS__VolumeLoad(void *VolumeHandle, size_t VbrOffset)
{
    ntfs_volume Result = {
        .Handle         = VolumeHandle,
        .StartOffset    = VbrOffset,
        .BytesPerSector = NTFS_BOOT_RECORD_SIZE,
    };

    uint8_t BootSector[NTFS_BOOT_RECORD_SIZE];
    if (!NTFS_VolumeRead(&Result, 0, &BootSector, sizeof(BootSector))) {
        NTFS_RETURN(Result.Error, NTFS_Error_VolumeReadBootRecord);
    };

    uint16_t Signature = *NTFS_CAST(uint16_t *, &BootSector[510]);
    if (Signature != NTFS_BOOT_RECORD_SIGNATURE) {
        NTFS_RETURN(Result.Error, NTFS_Error_VolumeUnknownSignature);
    }

    Result.BytesPerSector    = *NTFS_CAST(uint16_t *, &BootSector[0x0B]);
    Result.SectorsPerCluster = *NTFS_CAST(uint8_t  *, &BootSector[0x0D]);
    Result.MftCluster        = *NTFS_CAST(uint64_t *, &BootSector[0x30]);
    Result.BytesPerCluster   = Result.BytesPerSector * Result.SectorsPerCluster;
    Result.SerialNumber      = *NTFS_CAST(uint64_t *, BootSector + 0x48);

    int8_t ClustersPerFileRecord = *NTFS_CAST(int8_t *, &BootSector[0x40]);
    if (ClustersPerFileRecord < 0) {
        Result.BytesPerMftEntry = NTFS_CAST(uint64_t, 1 << (-ClustersPerFileRecord));
    } else {
        Result.BytesPerMftEntry = ClustersPerFileRecord * Result.BytesPerCluster;
    }

    bool IsValid = NTFS__IsPowerOf2(Result.BytesPerSector);
    IsValid     &= NTFS__IsPowerOf2(Result.SectorsPerCluster);
    IsValid     &= Result.BytesPerMftEntry <= Result.BytesPerCluster;
    if (!IsValid) {
        NTFS_RETURN(Result.Error, NTFS_Error_VolumeFailedValidation);
    }

    NTFS__VolumeLoadInformation(&Result);

skip:
    return Result;
}

void NTFS__VolumeLoadInformation(ntfs_volume *Volume)
{
    ntfs_file VolumeFile = NTFS_FileOpenFromIndex(Volume, NTFS_SystemFile_Volume);
    if (VolumeFile.Error) {
        NTFS_RETURN(Volume->Error, NTFS_Error_VolumeFailedLoadInfoFile);
    }

    for (size_t Index = 0; Index < NTFS__ListLen(VolumeFile.Record.AttrList); Index++) {
        ntfs_attr *Attr = VolumeFile.Record.AttrList + Index;

        if (Attr->Type == NTFS_AttributeType_VolumeName) {
            NTFS_MEM_COPY(Volume->Name, sizeof(Volume->Name) - 2,
                          Attr->Resident.Data, Attr->Resident.Size);

        } else if (Attr->Type == NTFS_AttributeType_VolumeInformation) {
            uint8_t Major = Attr->Resident.Data[0x08];
            uint8_t Minor = Attr->Resident.Data[0x09];
            if (!(Major == 3 && Minor == 1)) {
                NTFS_RETURN(Volume->Error, NTFS_Error_VolumeUnsupportedVersion);
            }
        }
    }

    // TODO: maybe change to sparse static table, instead of loading this for every volume
    ntfs_file UpCase = NTFS_FileOpenFromIndex(Volume, NTFS_SystemFile_UpCase);
    if (UpCase.Error) {
        NTFS_RETURN(Volume->Error, NTFS_Error_VolumeFailedLoadCaseTable);
    }

    ntfs_attr *DataAttr = 0;
    for (size_t Index = 0; Index < NTFS__ListLen(UpCase.Record.AttrList); Index++) {
        ntfs_attr *Attr = UpCase.Record.AttrList + Index;
        if (Attr->Type == NTFS_AttributeType_Data && !Attr->Name) {
            DataAttr = Attr;
            break;
        }
    }

    if (!DataAttr || DataAttr->NonResident.AlignedSize != NTFS__ARENA_KILOBYTE(128)) {
        NTFS_RETURN(Volume->Error, NTFS_Error_VolumeFailedLoadCaseTable);

    } else if (NTFS__ListLen(DataAttr->NonResident.RunList) != 1) {
        // TODO: Remove later when have support for reading files
        NTFS_RETURN(Volume->Error, NTFS_Error_VolumeFailedLoadCaseTable);
    }

    uint64_t CaseOffset =
        DataAttr->NonResident.RunList[0].StartVCN * Volume->BytesPerCluster;
    Volume->CaseTable   =
        NTFS__Win32MemoryAllocate(DataAttr->NonResident.AlignedSize, 0);

    // TODO: Remove NTFS__Win32MemoryAllocate once volume have better arena handling
    if (Volume->CaseTable == 0 ||
        !NTFS_VolumeRead(Volume, CaseOffset, Volume->CaseTable,
                         DataAttr->NonResident.AlignedSize)) {
        NTFS_RETURN(Volume->Error, NTFS_Error_VolumeFailedLoadCaseTable);
    }

skip:
    NTFS_FileClose(&VolumeFile);
    NTFS_FileClose(&UpCase);
}

ntfs_file NTFS_FileOpenFromIndex(ntfs_volume *Volume, size_t Index)
{
    ntfs_file Result = {
        .Arena  = NTFS__ArenaDefault(),
        .Volume = Volume,
    };

    if (Result.Arena.Buffer == 0) {
        NTFS_RETURN(Result.Error, NTFS_Error_MemoryError);
    }

    Result.Record = NTFS__RecordLoadFromIndex(Volume, &Result.Arena, Index);
    if (Result.Record.Error) {
        NTFS_RETURN(Result.Error, Result.Record.Error);
    }

    bool HasStdInfo  = false;
    bool HasFileName = false;
    for (size_t i = 0; i < NTFS__ListLen(Result.Record.AttrList); i++) {
        ntfs_attr *Attr = Result.Record.AttrList + i;

        if (Attr->Type == NTFS_AttributeType_StandardInformation) {
            HasStdInfo = true;

            Result.CreationTime = *NTFS_CAST(uint64_t *, Attr->Resident.Data + 0x00);
            Result.ModifiedTime = *NTFS_CAST(uint64_t *, Attr->Resident.Data + 0x08);
            Result.ChangedTime  = *NTFS_CAST(uint64_t *, Attr->Resident.Data + 0x10);
            Result.ReadTime     = *NTFS_CAST(uint64_t *, Attr->Resident.Data + 0x18);
            Result.Flags.Value  = *NTFS_CAST(uint32_t *, Attr->Resident.Data + 0x20);

            bool IsValid = (Result.CreationTime & INT64_MIN) == 0;
            IsValid     &= (Result.ModifiedTime & INT64_MIN) == 0;
            IsValid     &= (Result.ChangedTime  & INT64_MIN) == 0;
            IsValid     &= (Result.ReadTime     & INT64_MIN) == 0;
            if (!IsValid) {
                NTFS_RETURN(Result.Error, NTFS_Error_FileFailedInfoValidation);
            }

        } else if (Attr->Type == NTFS_AttributeType_FileName) {
            HasFileName = true;

            Result.ParentIndex =
                *NTFS_CAST(uint64_t *, Attr->Resident.Data + 0x00) & ~(INT64_MIN >> 16);
            // Result.AlignedSize = *NTFS_CAST(uint64_t *, Attr->Resident.Data + 0x28);
            // Result.Size        = *NTFS_CAST(uint64_t *, Attr->Resident.Data + 0x30);

            if (Result.Size > Result.AlignedSize ||
                !NTFS__IsAligned(Result.AlignedSize, Volume->BytesPerCluster)) {
                NTFS_RETURN(Result.Error, NTFS_Error_FileFailedInfoValidation);
            }

            uint8_t NameLength = Attr->Resident.Data[0x40];
            // uint8_t NameSpace  = Attr->Resident.Data[0x41];
            if (NameLength > (Attr->Resident.Size - 0x42)) {
                NTFS_RETURN(Result.Error, NTFS_Error_FileFailedInfoValidation);
            }

            Result.Name =
                NTFS__PushCopyWStringZ(&Result.Arena,
                                       NTFS_CAST(uint16_t *, Attr->Resident.Data + 0x42),
                                       NameLength);

        } else if (Attr->Type == NTFS_AttributeType_Data && !Attr->Name) {
            if (Attr->NonResFlag) {
                Result.Size        = Attr->NonResident.Size;
                Result.AlignedSize = Attr->NonResident.AlignedSize;
            } else {
                Result.Size        = Attr->Resident.Size;
                Result.AlignedSize = NTFS__Align(Result.Size, Volume->BytesPerCluster);
            }
        }
    }

    if (!HasStdInfo || !HasFileName) {
        NTFS_RETURN(Result.Error, NTFS_Error_FileFailedInfoValidation);
    }

skip:
    return Result;
}

ntfs_record NTFS__RecordLoadFromIndex(ntfs_volume *Volume, ntfs_arena *Arena, size_t Index)
{
    ntfs_record Result = { 0 };

    uint64_t RecordOffset = Volume->MftCluster * Volume->BytesPerCluster
                            + (Index * Volume->BytesPerMftEntry);
    uint8_t *FileRecord   = NTFS__ArenaAlloc(Arena, Volume->BytesPerMftEntry);
    if (!NTFS_VolumeRead(Volume, RecordOffset, FileRecord, Volume->BytesPerMftEntry)) {
        NTFS_RETURN(Result.Error, NTFS_Error_RecordFailedRead);
    }

    uint32_t Magic     = *NTFS_CAST(uint32_t *, FileRecord + 0x00);
    uint16_t Offset    = *NTFS_CAST(uint16_t *, FileRecord + 0x14);
    uint16_t Flags     = *NTFS_CAST(uint16_t *, FileRecord + 0x16);
    uint32_t RealSize  = *NTFS_CAST(uint32_t *, FileRecord + 0x18);
    uint32_t AllocSize = *NTFS_CAST(uint32_t *, FileRecord + 0x1C);
    uint32_t MftIndex  = *NTFS_CAST(uint32_t *, FileRecord + 0x2C);

    bool IsValid = Magic == NTFS_FILE_RECORD_MAGIC;
    IsValid     &= Offset < RealSize;
    IsValid     &= RealSize <= AllocSize && AllocSize == Volume->BytesPerMftEntry;
    IsValid     &= MftIndex == Index;
    IsValid     &= Flags & 0x01;
    if (!IsValid) {
        NTFS_RETURN(Result.Error, NTFS_Error_RecordFailedValidation);
    }
    Result.IsDir = Flags & 0x02;
    Result.Index = MftIndex;

    uint8_t *AttrPtr    = FileRecord + Offset;
    uint8_t *AttrEndPtr = AttrPtr + RealSize;
    while (AttrPtr < AttrEndPtr) {
        uint32_t Marker = *NTFS_CAST(uint32_t *, AttrPtr);
        if (Marker == NTFS_FILE_RECORD_ATTR_END_MARKER) {
            break;
        }

        uint32_t AttrTotalSize  = *NTFS_CAST(uint32_t *, AttrPtr + 0x04);
        uint32_t AttrNameOffset = *NTFS_CAST(uint16_t *, AttrPtr + 0x0A);
        if (AttrPtr + AttrTotalSize > AttrEndPtr) {
            NTFS_RETURN(Result.Error, NTFS_Error_RecordFailedValidation);
        }

        ntfs_attr Attr  = { 0 };
        Attr.Type       = *NTFS_CAST(uint32_t *, AttrPtr + 0x00);
        Attr.NonResFlag = AttrPtr[0x08] == 1;
        Attr.NameLength = *NTFS_CAST(uint8_t  *, AttrPtr + 0x09);
        Attr.Flags      = *NTFS_CAST(uint16_t *, AttrPtr + 0x0C);
        Attr.Id         = *NTFS_CAST(uint16_t *, AttrPtr + 0x0E);
        if (Attr.NameLength) {
            if (AttrNameOffset + Attr.NameLength >= AttrTotalSize) {
                NTFS_RETURN(Result.Error, NTFS_Error_RecordFailedValidation);
            }

            Attr.Name = NTFS_CAST(uint16_t *, AttrPtr + AttrNameOffset);
        }

        if (Attr.NonResFlag) {
            uint16_t AttrOffset    = *NTFS_CAST(uint16_t *, AttrPtr + 0x20);
            uint64_t AttrAllocSize = *NTFS_CAST(uint64_t *, AttrPtr + 0x28);
            uint64_t AttrRealSize  = *NTFS_CAST(uint64_t *, AttrPtr + 0x30);
            if (AttrRealSize > AttrAllocSize ||
                !NTFS__IsAligned(AttrAllocSize, Volume->BytesPerCluster)) {
                NTFS_RETURN(Result.Error, NTFS_Error_RecordFailedValidation);
            }

            Attr.NonResident.Size        = AttrRealSize;
            Attr.NonResident.AlignedSize = AttrAllocSize;
            Attr.NonResident.RunList =
                NTFS__DataRunsLoad(Arena, AttrPtr + AttrOffset,
                                   AttrTotalSize - AttrOffset);

        } else {
            uint32_t AttrSize   = *NTFS_CAST(uint32_t *, AttrPtr + 0x10);
            uint16_t AttrOffset = *NTFS_CAST(uint16_t *, AttrPtr + 0x14);
            if (AttrOffset + AttrSize > AttrTotalSize) {
                NTFS_RETURN(Result.Error, NTFS_Error_RecordFailedValidation);
            }

            Attr.Resident.Size = AttrSize;
            if (Attr.Resident.Size > 0) {
                Attr.Resident.Data = AttrPtr + AttrOffset;
            }
        }

        NTFS__ListPush(Arena, Result.AttrList, Attr);
        AttrPtr += AttrTotalSize;
    }

skip:
    return Result;
}

ntfs_data_run *NTFS__DataRunsLoad(ntfs_arena *Arena, void *Buffer, size_t Size)
{
    uint8_t *DataRunPtr    = NTFS_CAST(uint8_t *, Buffer);
    uint8_t *DataRunEndPtr = DataRunPtr + Size;

    ntfs_data_run *Result  = 0;
    uint64_t       PrevVCN = 0;
    while (DataRunPtr < DataRunEndPtr) {
        if (*DataRunPtr == 0) {
            break;
        }

        uint8_t LenSize =  *DataRunPtr & 0x0F;
        uint8_t OffSize = (*DataRunPtr & 0xF0) >> 4;
        DataRunPtr++;

        uint64_t Length = 0;
        for (int j = 0; j < LenSize; j++) {
            Length |= *DataRunPtr << (8 * j);
            DataRunPtr++;
        }

        int64_t Offset = 0;
        for (int j = 0; j < OffSize; j++) {
            Offset |= *DataRunPtr << (8 * j);
            DataRunPtr++;
        }

        ntfs_data_run Run = { .Count = Length, .StartVCN = (PrevVCN += Offset) };
        NTFS__ListPush(Arena, Result, Run);
    }

    return Result;
}

void NTFS_FileClose(ntfs_file *File)
{
    if (File->Arena.Buffer) {
        NTFS__ArenaDestroy(&File->Arena);
    }

    *File = (ntfs_file) { .Error = File->Error };
}

size_t NTFS_FileRead(ntfs_file *File, uint64_t Offset, uint8_t *Buffer, size_t Size)
{
    // TODO: Add support for unaligned params
    NTFS_ASSERT(NTFS__IsAligned(Offset, File->Volume->BytesPerCluster),
                "Offset must be aligned to cluster size");
    NTFS_ASSERT(NTFS__IsAligned(Size, File->Volume->BytesPerCluster),
                "Size must be aligned to cluster size");

    size_t Result = 0;

    ntfs_attr *DataAttr = 0;
    for (size_t Index = 0; Index < NTFS__ListLen(File->Record.AttrList); Index++) {
        ntfs_attr *Attr = File->Record.AttrList + Index;

        if (Attr->Type == NTFS_AttributeType_Data && !Attr->Name) {
            DataAttr = Attr;
            break;
        }
    }

    if (DataAttr == 0) {
        NTFS_RETURN(File->Error, NTFS_Error_FileReadDataAttrNotFound);
    }

    if (!DataAttr->NonResFlag) {
        size_t SrcSize =
            (DataAttr->Resident.Size > Size) ? Size : DataAttr->Resident.Size;
        NTFS_MEM_COPY(Buffer, Size, DataAttr->Resident.Data, SrcSize);
        NTFS_RETURN(Result, SrcSize);
    }

    uint64_t FileOffset   = 0;
    uint64_t BufferOffset = 0;
    for (size_t Index = 0; Index < NTFS__ListLen(DataAttr->NonResident.RunList) && Size; Index++) {
        ntfs_data_run *Run = DataAttr->NonResident.RunList + Index;

        size_t   ReadSize   = Run->Count * File->Volume->BytesPerCluster;
        uint64_t ReadOffset = Run->StartVCN * File->Volume->BytesPerCluster;

        if (Offset <= FileOffset + ReadSize) {
            if (Size < ReadSize) {
                ReadSize = Size;
            }

            if (FileOffset < Offset) {
                ReadOffset += Offset - FileOffset;
            }

            if (!NTFS_VolumeRead(File->Volume, ReadOffset, Buffer + BufferOffset, ReadSize)) {
                NTFS_RETURN(File->Error, NTFS_Error_FileReadFailed);
            }

            Offset += ReadSize;
            BufferOffset += ReadSize;
            Result += ReadSize;
            Size -= ReadSize;
        }

        FileOffset += ReadSize;
    }

skip:
    return Result;
}

#endif  // NTFS_PARSER_IMPLEMENTATION

#ifndef NTFS_PARSER_H
#define NTFS_PARSER_H

#include <stdbool.h>
#include <stdint.h>

// Helper macros
#define NTFS_STATEMENT(statement) \
    do {                          \
        statement                 \
    } while(0)

#define NTFS_UNUSED(name)           (void) (name)
#define NTFS_CAST(type, value)      ((type) (value))
#define NTFS_RETURN(name, error)    \
    NTFS_STATEMENT((name)->Error = error; goto skip;)

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

#define NTFS__ARENA_MEGABYTE(value) (value * 1024 * 1024)
#define NTFS__ARENA_DEFAULT_COMMIT   NTFS__ARENA_MEGABYTE(1)
#define NTFS__ARENA_DEFAULT_RESERVED NTFS__ARENA_MEGABYTE(16)

NTFS_API ntfs_arena NTFS__ArenaDefault(void);
NTFS_API void       NTFS__ArenaDestroy(ntfs_arena *Arena);
NTFS_API void      *NTFS__ArenaAlloc(ntfs_arena *Arena, size_t Size);
NTFS_API void      *NTFS__ArenaResizeAlloc(ntfs_arena *Arena, void *Address, size_t Size);
NTFS_API void       NTFS__ArenaReset(ntfs_arena *Arena);


typedef enum {
    NTFS_Error_Success,

    // Volume related errors
    NTFS_Error_VolumeOpen,
    NTFS_Error_VolumeReadBootRecord,
    NTFS_Error_VolumeUnknownSignature,
    NTFS_Error_VolumePartitionNotFound,
    NTFS_Error_VolumeFailedValidation,
} ntfs_error;

static inline char *NTFS_ErrorToString(ntfs_error Error)
{
    switch (Error) {
    case NTFS_Error_Success:                 return "ntfs success";
    case NTFS_Error_VolumeOpen:              return "ntfs failed opening handle to volume";
    case NTFS_Error_VolumeReadBootRecord:    return "ntfs failed reading volume boot record";
    case NTFS_Error_VolumeUnknownSignature:  return "ntfs failed unknown volume signature";
    case NTFS_Error_VolumePartitionNotFound: return "ntfs failed partition was not found";
    case NTFS_Error_VolumeFailedValidation:  return "ntfs failed volume fields validation";
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
} ntfs_volume;

#define NTFS_BOOT_RECORD_SIZE                512
#define NTFS_BOOT_RECORD_SIGNATURE           0xAA55
#define NTFS_BOOT_RECORD_PARTITION_OFFSET    0x01BE
#define NTFS_BOOT_RECORD_PARITION_ENTRY_SIZE 0x10

NTFS_API ntfs_volume NTFS_VolumeOpen(char DriveLetter);
NTFS_API ntfs_volume NTFS_VolumeOpenFromFile(wchar_t *Path);
NTFS_API void        NTFS_VolumeClose(ntfs_volume *Volume);
NTFS_API bool        NTFS_VolumeRead(ntfs_volume *Volume, uint64_t From,
                                     void *Buffer, size_t Size);

NTFS_API ntfs_volume NTFS__VolumeLoad(void *VolumeHandle, size_t VbrOffset);

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
        .Offset     =  Offset & 0xFFFFFFFF,
        .OffsetHigh = (Offset & 0xFFFFFFFF) >> 32,
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
        Result = NTFS__Win32MemoryCommit(Result, CommittedSize);
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

void *NTFS__ArenaResizeAlloc(ntfs_arena *Arena, void *Address, size_t Size)
{
    ntfs_arena_header *Header =
        NTFS_CAST(ntfs_arena_header *,
                  &NTFS_CAST(uint8_t *, Address)[-sizeof(*Header)]);

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


// Volume API
ntfs_volume NTFS_VolumeOpen(char DriveLetter)
{
    ntfs_volume Result = { 0 };

    wchar_t DrivePath[] = L"\\\\.\\ :";
    DrivePath[4]        = NTFS_CAST(wchar_t, DriveLetter);
    void *VolumeHandle  = NTFS__Win32FileOpen(DrivePath);
    if (VolumeHandle == 0) {
        NTFS_RETURN(&Result, NTFS_Error_VolumeOpen);
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
        NTFS_RETURN(&Result, NTFS_Error_VolumeOpen);
    }

    uint8_t BootSector[NTFS_BOOT_RECORD_SIZE];
    if (!NTFS__Win32FileRead(VolumeHandle, 0, &BootSector, sizeof(BootSector))) {
        NTFS_RETURN(&Result, NTFS_Error_VolumeReadBootRecord);
    }

    uint16_t Signature = *NTFS_CAST(uint16_t *, &BootSector[510]);
    if (Signature != NTFS_BOOT_RECORD_SIGNATURE) {
        NTFS_RETURN(&Result, NTFS_Error_VolumeUnknownSignature);
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

    // Dont override the error
    *Volume = (ntfs_volume) { .Error = Volume->Error};
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
        NTFS_RETURN(&Result, NTFS_Error_VolumeReadBootRecord);
    };

    uint16_t Signature = *NTFS_CAST(uint16_t *, &BootSector[510]);
    if (Signature != NTFS_BOOT_RECORD_SIGNATURE) {
        NTFS_RETURN(&Result, NTFS_Error_VolumeUnknownSignature);
    }

    Result.BytesPerSector    = *NTFS_CAST(uint16_t *, &BootSector[0x0B]);
    Result.SectorsPerCluster = *NTFS_CAST(uint8_t  *, &BootSector[0x0D]);
    Result.MftCluster        = *NTFS_CAST(uint64_t *, &BootSector[0x30]);
    Result.BytesPerCluster   = Result.BytesPerSector * Result.SectorsPerCluster;

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
        NTFS_RETURN(&Result, NTFS_Error_VolumeFailedValidation);
    }

skip:
    return Result;
}

bool NTFS_VolumeRead(ntfs_volume *Volume, uint64_t From, void *Buffer, size_t Size)
{
    NTFS_ASSERT((From & (Volume->BytesPerSector - 1)) == 0,
                "volume read offset is not aligned to volume sector size");
    NTFS_ASSERT((Size & (Volume->BytesPerSector - 1)) == 0,
                "volume read size is not aligned to volume sector size");

    bool Result = NTFS__Win32FileRead(Volume->Handle, From + Volume->StartOffset,
                                      Buffer, Size);
    return Result;
}

#endif  // NTFS_PARSER_IMPLEMENTATION

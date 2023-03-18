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

typedef enum {
    NTFSError_Success,

    // Volume related errors
    NTFSError_VolumeOpen,
    NTFSError_VolumeReadBootRecord,
    NTFSError_VolumeUnknownSignature,
} ntfs_error;

static inline char *NTFS_ErrorToString(ntfs_error Error)
{
    switch (Error) {
    case NTFSError_Success:                return "ntfs success";
    case NTFSError_VolumeOpen:             return "ntfs failed opening handle to volume";
    case NTFSError_VolumeReadBootRecord:   return "ntfs failed reading volume boot record";
    case NTFSError_VolumeUnknownSignature: return "ntfs failed unknown volume signature";
    }

    return "";
}

// Volume API
typedef struct {
    ntfs_error Error;
    void      *Handle;

    uint64_t SectorsPerCluster;
    uint64_t MftCluster;
    uint64_t BytesPerSector;
    uint64_t BytesPerCluster;
    uint64_t BytesPerMftEntry;
} ntfs_volume;

#define NTFS_BOOT_RECORD_SIZE       512
#define NTFS_BOOT_RECORD_SIGNATURE  0xAA55

NTFS_API ntfs_volume NTFS_VolumeOpen(char DriveLetter);
NTFS_API void        NTFS_VolumeClose(ntfs_volume *Volume);

#endif   // NTFS_PARSER_H


#ifdef NTFS_PARSER_IMPLEMENTATION

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Platform specific APIs
static void  NTFS__Win32Log(wchar_t *Message, wchar_t *FileName, size_t Line);
static void *NTFS__Win32FileOpen(wchar_t *FilePath);
static bool  NTFS__Win32FileRead(void *Handle, uint64_t Offset, void *Buffer, size_t Size);

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

ntfs_volume NTFS_VolumeOpen(char DriveLetter)
{
    ntfs_volume Result = { 0 };

    wchar_t DrivePath[] = L"\\\\.\\ :";
    DrivePath[4]        = NTFS_CAST(wchar_t, DriveLetter);
    void *VolumeHandle  = NTFS__Win32FileOpen(DrivePath);
    if (VolumeHandle == 0) {
        NTFS_RETURN(&Result, NTFSError_VolumeOpen);
    }
    Result.Handle = VolumeHandle;

    uint8_t BootSector[NTFS_BOOT_RECORD_SIZE];
    if (!NTFS__Win32FileRead(Result.Handle, 0, &BootSector, sizeof(BootSector))) {
        NTFS_RETURN(&Result, NTFSError_VolumeReadBootRecord);
    };

    uint16_t Signature = *NTFS_CAST(uint16_t *, &BootSector[510]);
    if (Signature != NTFS_BOOT_RECORD_SIGNATURE) {
        NTFS_RETURN(&Result, NTFSError_VolumeUnknownSignature);
    }

    // TODO: Validate valid values
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

#endif  // NTFS_PARSER_IMPLEMENTATION

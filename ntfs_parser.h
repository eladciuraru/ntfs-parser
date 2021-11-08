
#ifndef _NTFS_PARSER_H
#define _NTFS_PARSER_H

#include <stdbool.h>
#include <stdint.h>

#define NTFS_Assert(Con, Msg) do { if (!(Con)) { __debugbreak(); } } while(0)


typedef struct {
    void *Handle;

    // Extracted
    uint64_t SectorsPerCluster;
    uint64_t MftCluster;
    uint64_t BytesPerSector;

    // Calcualted
    uint64_t BytesPerCluster;
    uint64_t BytesPerMftEntry;
} ntfs_volume;

ntfs_volume NTFS_VolumeOpen(char DriveLetter);
void        NTFS_VolumeClose(ntfs_volume *Volume);

inline bool NTFS_VolumeIsValid(ntfs_volume *Volume)
{
    bool Result = Volume->Handle != (void *)-1;
    Result     &= Volume->Handle != 0;

    return Result;
}


#pragma pack(push, 1)

typedef struct {
    uint8_t  Jump[3];
    uint8_t  OemID[8];
    uint16_t BytesPerSector;
    uint8_t  SectorsPerCluster;
    uint16_t ReservedSectors;
    uint8_t  Unsued0[3];
    uint16_t Unused1;
    uint8_t  MediaDescriptor;
    uint16_t Unused2;
    uint16_t SectorsPerTrack;
    uint16_t NumberOfHeads;
    uint32_t HiddenSectors;
    uint32_t Unused3;
    uint32_t Unused4;
    uint64_t TotalSectors;
    uint64_t MftCluster;
    uint64_t MftMirrorCluster;
    int8_t   ClustersPerFileRecord;
    uint8_t  Unused5[3];
    int8_t   ClustersPerIndexBuffer;
    uint8_t  Unused6[3];
    uint64_t SerialNumber;
    uint32_t Checksum;
    uint8_t  Code[426];
    uint16_t Signature;
} ntfs_vbr;

#define NTFS_VBR_SIGNATURE 0xAA55

#pragma pack(pop)

#endif  // _NTFS_PARSER_H


#ifdef NTFS_PARSER_IMPLEMENTATION

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


static bool Win32_ReadFrom_(void *Buffer, size_t Size, uint64_t From, HANDLE Handle)
{
    NTFS_Assert(Size == (uint32_t)Size, "Not supporting reading 64bit size");

    LARGE_INTEGER From_ = { .QuadPart = From };
    SetFilePointerEx(Handle, From_, 0, FILE_BEGIN);

    DWORD BytesRead = 0;
    BOOL  Result    = ReadFile(Handle, Buffer, Size, &BytesRead, NULL);
    return Result && BytesRead == Size;
}

ntfs_volume NTFS_VolumeOpen(char DriveLetter)
{
    ntfs_volume Volume = { 0 };

    wchar_t DrivePath[] = L"\\\\.\\ :";
    DrivePath[4]        = (wchar_t)DriveLetter;
    HANDLE VolumeHandle = CreateFileW(DrivePath, GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      0, OPEN_EXISTING, 0, 0);
    if (VolumeHandle == INVALID_HANDLE_VALUE) {
        goto SKIP;
    }
    Volume.Handle = VolumeHandle;

    ntfs_vbr BootSector;
    Win32_ReadFrom_(&BootSector, sizeof(BootSector), 0, Volume.Handle);
    if (BootSector.Signature != NTFS_VBR_SIGNATURE) {
        NTFS_VolumeClose(&Volume);
        goto SKIP;
    }

    // TODO: Validate valid values
    Volume.BytesPerSector    = BootSector.BytesPerSector;
    Volume.SectorsPerCluster = BootSector.SectorsPerCluster;
    Volume.MftCluster        = BootSector.MftCluster;
    Volume.BytesPerCluster   = Volume.BytesPerSector * Volume.SectorsPerCluster;
    if (BootSector.ClustersPerFileRecord < 0) {
        Volume.BytesPerMftEntry = 1 << (-BootSector.ClustersPerFileRecord);
    } else {
        Volume.BytesPerMftEntry = BootSector.ClustersPerFileRecord * Volume.BytesPerCluster;
    }

SKIP:
    return Volume;
}


void NTFS_VolumeClose(ntfs_volume *Volume)
{
    if (NTFS_VolumeIsValid(Volume)) {
        CloseHandle(Volume->Handle);
        Volume->Handle = INVALID_HANDLE_VALUE;
    }
}

#endif  // NTFS_PARSER_IMPLEMENTATION

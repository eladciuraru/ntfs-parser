
#ifndef _NTFS_PARSER_H
#define _NTFS_PARSER_H

#include <stdbool.h>

typedef struct {
    void *Handle;
} ntfs_volume;

ntfs_volume NTFS_VolumeOpen(char DriveLetter);
void        NTFS_VolumeClose(ntfs_volume *Volume);

inline bool NTFS_VolumeIsValid(ntfs_volume *Volume)
{
    bool Result = Volume->Handle != (void *)-1;
    Result     &= Volume->Handle != 0;

    return Result;
}

#endif  // _NTFS_PARSER_H


#ifdef NTFS_PARSER_IMPLEMENTATION

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

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

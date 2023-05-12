#define NTFS_PARSER_IMPLEMENTATION
#include "ntfs_parser.h"

#include <stdio.h>
#include <windows.h>
#include <wchar.h>

void DumpFileRecord(ntfs_file *File);
void PrintHexDump(void *Buffer, size_t Size);
void PrintFileTime(uint64_t FileTime);


int wmain(int Argc, wchar_t **Argv)
{
    int         Result = 0;
    ntfs_volume Volume = { 0 };
    ntfs_file   File   = { 0 };

    if (Argc != 3) {
        printf("Usage: %ls ntfs_volume mft_index\n", Argv[0]);
        printf("    ntfs_volume - path to ntfs volume (VHD or Volume letter)\n");
        printf("    mft_index   - index to MFT record to dump\n");
        NTFS_RETURN(Result, 1);
    }

    wchar_t *VolumePath  = Argv[1];
    wchar_t *MftIndexEnd = 0;
    uint64_t MftIndex    = wcstoull(Argv[2], &MftIndexEnd, 10);
    if (MftIndexEnd == Argv[2]) {
        printf("error: mft_index is not a number\n");
        NTFS_RETURN(Result, 1);
    }

    Volume = NTFS_VolumeOpenFromFile(L"ntfs_volume.vhd");
    if (wcslen(VolumePath) == 1) {
        Volume = NTFS_VolumeOpen(VolumePath[0]);
    } else {
        Volume = NTFS_VolumeOpenFromFile(VolumePath);
    }
    if (Volume.Error) {
        printf("error: Failed to load volume - %s\n", NTFS_ErrorToString(Volume.Error));
        NTFS_RETURN(Result, 1);
    }

    File = NTFS_FileOpenFromIndex(&Volume, NTFS_SystemFile_Mft);
    if (File.Error) {
        printf("error: Failed to load MFT record - %s\n", NTFS_ErrorToString(File.Error));
        NTFS_RETURN(Result, 1);
    }

    uint64_t MaxMftIndex = (File.Size / Volume.BytesPerMftEntry) - 1;
    if (MftIndex > MaxMftIndex) {
        printf("Clamping MftIndex to max possible index %llu\n", MaxMftIndex);
        MftIndex = MaxMftIndex;
    }

    // No need to reopen MftRecord
    if (MftIndex != 0) {
        NTFS_FileClose(&File);
        File = NTFS_FileOpenFromIndex(&Volume, MftIndex);
        if (File.Error) {
            printf("error: failed to open file - %s\n", NTFS_ErrorToString(File.Error));
            NTFS_RETURN(Result, 1);
        }
    }

    DumpFileRecord(&File);

skip:
    NTFS_FileClose(&File);
    NTFS_VolumeClose(&Volume);

    return Result;
}

void DumpFileRecord(ntfs_file *File)
{
    printf("File information:\n");
    printf("    Name:         %ls\n", File->Name);
    printf("    Size:         %lld\n", File->Size);
    printf("    AlignedSize:  %lld\n", File->AlignedSize);
    printf("    Flags:        0x%04x\n", File->Flags.Value);

    printf("    CreationTime: ");
    PrintFileTime(File->CreationTime);
    printf("\n");

    printf("    ModifiedTime: ");
    PrintFileTime(File->ModifiedTime);
    printf("\n");

    printf("    ChangedTime:  ");
    PrintFileTime(File->ChangedTime);
    printf("\n");

    printf("    ReadTime:     ");
    PrintFileTime(File->ReadTime);
    printf("\n");

    printf("    Parent Index: %lld\n", File->ParentIndex);
    printf("\n\n");

    for (size_t i = 0; i < NTFS__ListLen(File->Record.AttrList); i++) {
        ntfs_attr *Attr = File->Record.AttrList + i;

        printf("Attribute %02d\n", Attr->Id);
        printf("    Type:        0x%03x (%s)\n",
               Attr->Type, NTFS_AttrTypeToString(Attr->Type));
        printf("    NonResident: %s\n", (Attr->NonResFlag) ? "true" : "false");
        printf("    Flags:       0x%x\n", Attr->Flags);

        if (Attr->Name) {
            printf("    Name:        %.*ls\n", Attr->NameLength, Attr->Name);
        }

        if (Attr->NonResFlag) {
            printf("    Data Runs (%lld):\n", Attr->NonResident.Size);
            for (size_t j = 0; j < NTFS__ListLen(Attr->NonResident.RunList); j++) {
                printf("        StartVCN: 0x%zx, Count: 0x%zx\n",
                       Attr->NonResident.RunList[j].StartVCN,
                       Attr->NonResident.RunList[j].Count);
            }

        } else {
            if (Attr->Resident.Data) {
                printf("\nHexdump (%d):\n", Attr->Resident.Size);
                PrintHexDump(Attr->Resident.Data, Attr->Resident.Size);
            }
        }

        printf("\n\n");
    }
}

void PrintHexDump(void *Buffer, size_t Size)
{
    uint8_t *ByteBuffer = Buffer;
    char     AsciiBuffer[17] = { 0 };
    size_t   AsciiBufferSize = sizeof(AsciiBuffer) - 1;

    for (size_t i = 0; i < Size; i++) {
        size_t BufferIndex = i % AsciiBufferSize;

        if (BufferIndex == 0) {
            printf("%08llx: ", i);
            memset(AsciiBuffer, 0, AsciiBufferSize);
        }

        char Sep[] = {' ', '\x00', '\x00' };
        if (BufferIndex + 1 == AsciiBufferSize / 2) {
            Sep[1] = ' ';
        }
        printf("%02x%s", ByteBuffer[i], Sep);
        AsciiBuffer[BufferIndex] = ByteBuffer[i];
        if (AsciiBuffer[BufferIndex] < ' ' || AsciiBuffer[BufferIndex] >= '~') {
            AsciiBuffer[BufferIndex] = '.';
        }

        if (BufferIndex == AsciiBufferSize - 1) {
            printf(" %s\n", AsciiBuffer);

        } else if (i + 1 == Size) {
            for (size_t j = BufferIndex + 1; j < AsciiBufferSize; j++) {
                printf("   ");

                if (j + 1 == AsciiBufferSize / 2) {
                    printf(" ");
                }
            }
            printf(" %s\n", AsciiBuffer);
        }
    }
    printf("%08llx:\n", Size);
}

void PrintFileTime(uint64_t FileTime)
{
    FILETIME  FileTime_ = {
        .dwHighDateTime = FileTime >> 32,
        .dwLowDateTime  = FileTime  & 0xFFFFFFFF,
    };
    SYSTEMTIME SystemTime = { 0 };
    FileTimeToSystemTime(&FileTime_, &SystemTime);

    wchar_t TimeStr[260];
    wchar_t DateStr[260];
    GetDateFormatW(LOCALE_SYSTEM_DEFAULT, DATE_SHORTDATE,
                   &SystemTime, 0, DateStr, ARRAYSIZE(TimeStr));
    GetTimeFormatW(LOCALE_SYSTEM_DEFAULT, TIME_FORCE24HOURFORMAT,
                   &SystemTime, 0, TimeStr, ARRAYSIZE(TimeStr));

    printf("%ls %ls", DateStr, TimeStr);
}

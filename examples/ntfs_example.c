#define NTFS_PARSER_IMPLEMENTATION
#include "ntfs_parser.h"

#include <stdio.h>
#include <windows.h>

void PrintHexDump(void *Buffer, size_t Size);

#define RETURN_SKIP(value)    NTFS_STATEMENT(Result = 1; goto skip;)
#define BOOL_TO_STRING(value) ((char *[]){"false", "true"}[value])


int main(void)
{
    int Result = 0;

    // ntfs_volume Volume = NTFS_VolumeOpen('C');
    ntfs_volume Volume = NTFS_VolumeOpenFromFile(L"ntfs_volume.vhd");
    if (Volume.Error) {
        printf("error: %s\n", NTFS_ErrorToString(Volume.Error));
        RETURN_SKIP(1);
    }

    printf("MTF cluster number: 0x%zx\n", Volume.MftCluster);
    printf("MTF cluster Offset: 0x%zx\n", Volume.MftCluster * Volume.BytesPerCluster);
    printf("\n\n");

    ntfs_file File = NTFS_FileOpenFromIndex(&Volume, NTFS_SystemFile_UpCase);
    if (File.Error) {
        printf("error: %s\n", NTFS_ErrorToString(File.Error));
        RETURN_SKIP(1);
    }

    for (size_t i = 0; i < NTFS__ListLen(File.Record.AttrList); i++) {
        ntfs_attr *Attr = File.Record.AttrList + i;

        printf("Attribute %02d\n", Attr->Id);
        printf("    Type:        0x%03x (%s)\n",
               Attr->Type, NTFS_AttrTypeToString(Attr->Type));
        printf("    NonResident: %s\n", BOOL_TO_STRING(Attr->NonResFlag));
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

skip:
    NTFS_FileClose(&File);
    NTFS_VolumeClose(&Volume);

    return Result;
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

#define NTFS_PARSER_IMPLEMENTATION
#include "ntfs_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <wchar.h>

typedef struct {
    uint16_t Label[64];
    uint32_t Type;
    uint32_t DisplayRule;
    uint32_t CollationRule;
    uint32_t Flags;
    uint64_t MinimumSize;
    uint64_t MaximumSize;
} attr_def;


int wmain(int Argc, wchar_t **Argv)
{
    int         Result = 0;
    ntfs_volume Volume = { 0 };
    ntfs_file   File   = { 0 };
    uint8_t    *Buffer = 0;

    if (Argc != 2) {
        printf("Usage: %ls ntfs_volume\n", Argv[0]);
        printf("    ntfs_volume - path to ntfs volume (VHD or Volume letter)\n");
        NTFS_RETURN(Result, 1);
    }

    wchar_t *VolumePath = Argv[1];
    printf("Dumping $AttrDef file for: %ls\n\n", VolumePath);

    if (wcslen(VolumePath) == 1) {
        Volume = NTFS_VolumeOpen(VolumePath[0]);
    } else {
        Volume = NTFS_VolumeOpenFromFile(VolumePath);
    }
    if (Volume.Error) {
        printf("error: %s\n", NTFS_ErrorToString(Volume.Error));
        NTFS_RETURN(Result, 1);
    }

    File = NTFS_FileOpenFromIndex(&Volume, NTFS_SystemFile_AttrDef);
    if (File.Error) {
        printf("error: %s\n", NTFS_ErrorToString(File.Error));
        NTFS_RETURN(Result, 1);
    }

    Buffer = malloc(File.AlignedSize);
    if (!Buffer) {
        printf("error: no memory\n");
        NTFS_RETURN(Result, 1);
    }

    if (!NTFS_FileRead(&File, 0, Buffer, File.AlignedSize)) {
        printf("error: %s\n", NTFS_ErrorToString(File.Error));
        NTFS_RETURN(Result, 1);
    }

    attr_def *Array = NTFS_CAST(attr_def *, Buffer);
    size_t    Count = File.Size / sizeof(attr_def);
    for (size_t Index = 0; Index < Count && Array[Index].Label[0]; Index++) {
        attr_def *AttrDef = Array + Index;

        printf("### Attribute %lld ###\n", Index);
        printf("Label:          %.*ls\n",
               NTFS_CAST(int, sizeof(AttrDef->Label)), AttrDef->Label);

        printf("Type:           0x%03x\n", AttrDef->Type);
        printf("Display Rule:   %d\n", AttrDef->DisplayRule);
        printf("Collation Rule: %d\n", AttrDef->CollationRule);
        printf("Flags:          0x%02x", AttrDef->Flags);
        if (AttrDef->Flags != 0){
            printf(" ( ");
            if (AttrDef->Flags & 0x02) {
                printf("Indexed ");
            }
            if (AttrDef->Flags & 0x40) {
                printf("Resident ");
            }
            if (AttrDef->Flags & 0x80) {
                printf("Non-Resident ");
            }
            printf(")");
        }
        printf("\n");

        printf("Minimum size:   0x%02llx\n", AttrDef->MinimumSize);
        printf("Maximum size:   0x%02llx\n", AttrDef->MaximumSize);
        printf("\n\n");
    }

skip:
    NTFS_VolumeClose(&Volume);
    NTFS_FileClose(&File);
    free(Buffer);

    return Result;
}

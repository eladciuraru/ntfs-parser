#define NTFS_PARSER_IMPLEMENTATION
#include "ntfs_parser.h"

#include <stdio.h>
#include <windows.h>


int main(void)
{
    ntfs_volume Volume = NTFS_VolumeOpen('C');
    if (Volume.Error) {
        printf("error: %s\n", NTFS_ErrorToString(Volume.Error));
        return 1;
    }

    printf("MTF cluster number: 0x%zx\n", Volume.MftCluster);
    printf("MTF cluster Offset: 0x%zx\n", Volume.MftCluster * Volume.BytesPerCluster);

    NTFS_VolumeClose(&Volume);
    return 0;
}

#include <stdio.h>

#define NTFS_PARSER_IMPLEMENTATION
#include "ntfs_parser.h"

#pragma comment(lib, "advapi32.lib")


static bool Win32_IsProcessElevated(void)
{
    HANDLE Token = NULL;
    BOOL   Flag  = OpenProcessToken(GetCurrentProcess(),
                                    TOKEN_QUERY, &Token);
    if (Flag == FALSE) {
        goto SKIP;
    }

    TOKEN_ELEVATION Elevation = { 0 };
    DWORD           Length    = 0;
    Flag = GetTokenInformation(Token, TokenElevation,
                               &Elevation, sizeof(Elevation),
                               &Length);
    if (Flag == FALSE) {
        goto SKIP;
    }

    Flag = Elevation.TokenIsElevated;

SKIP:
    if (Token != NULL) { CloseHandle(Token); }

    return Flag != FALSE;
}


int main(void)
{
    if (!Win32_IsProcessElevated()) {
        fprintf(stderr, "Process should be run as administrator..\n");
        return 1;
    }

    ntfs_volume NtfsVolume = NTFS_VolumeOpen('X');
    if (!NTFS_VolumeIsValid(&NtfsVolume)) {
        fprintf(stderr, "Failed to open volume\n");
        return 1;
    }

    NTFS_VolumeClose(&NtfsVolume);

    return 0;
}

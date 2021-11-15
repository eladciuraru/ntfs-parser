#include <stdio.h>
#include <stdlib.h>

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


void PrintHexDump(void *Buffer, size_t Size)
{
    uint8_t *ByteBuffer = Buffer;
    char     AsciiBuffer[17] = { 0 };
    size_t   AsciiBufferSize = sizeof(AsciiBuffer) - 1;

    for (size_t I = 0; I < Size; I++) {
        size_t BufferIndex = I % AsciiBufferSize;

        if (BufferIndex == 0) {
            printf("%08llx: ", I);
            memset(AsciiBuffer, 0, AsciiBufferSize);
        }

        char Sep[] = {' ', '\x00', '\x00' };
        if (BufferIndex + 1 == AsciiBufferSize / 2) {
            Sep[1] = ' ';
        }
        printf("%02x%s", ByteBuffer[I], Sep);
        AsciiBuffer[BufferIndex] = ByteBuffer[I];
        if (AsciiBuffer[BufferIndex] < ' ' || AsciiBuffer[BufferIndex] >= '~') {
            AsciiBuffer[BufferIndex] = '.';
        }

        if (BufferIndex == AsciiBufferSize - 1) {
            printf(" %s\n", AsciiBuffer);

        } else if (I + 1 == Size) {
            for (size_t J = BufferIndex + 1; J < AsciiBufferSize; J++) {
                printf("   ");

                if (J + 1 == AsciiBufferSize / 2) {
                    printf(" ");
                }
            }
            printf(" %s\n", AsciiBuffer);
        }
    }
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

    void    *RecordBuffer = Win32_AllocateMemory_(NtfsVolume.BytesPerMftEntry);
    uint64_t EntryIndex   = 10;
    if (!NTFS_MftReadRecord(&NtfsVolume, EntryIndex, RecordBuffer)) {
        fprintf(stderr, "Failed to read mft entry (%lld)\n", EntryIndex);
        return 1;
    }

    ntfs_attr_iter AttrIter = { 0 };
    char *BoolStrings[] = { "False", "True" };
    for (size_t Index = 0; NTFS_NextFileAttribute(RecordBuffer, &AttrIter); Index += 1) {
        ntfs_attr_header *Header = AttrIter.Attribute;

        printf("#### Attribute %02lld ####\n", Index);

        printf("Type: %02x (%s)\n", Header->Type, NTFS_AttributeType(Header->Type));
        printf("Length: %08x\n", Header->Length);
        printf("IsNonResident: %02x (%s)\n",
               Header->IsNonResident, BoolStrings[!!Header->IsNonResident]);
        printf("NameLength: %02x\n", Header->NameLength);
        printf("NameOffset: %04x\n", Header->NameOffset);
        printf("Flags: %04x\n", Header->Flags);
        printf("AttributeID: %04x\n", Header->AttributeID);

        printf("\n");
        if (Header->IsNonResident) {
            printf("Non Resident:\n");
            printf("\tFirstCluster: %08llx\n", Header->_NonResident.FirstCluster);
            printf("\tLastCluster: %08llx\n", Header->_NonResident.LastCluster);
            printf("\tDataRunOffset: %04x\n", Header->_NonResident.DataRunOffset);
            printf("\tCompressionUnit: %04x\n", Header->_NonResident.CompressionUnit);
            printf("\tAttributeAllocated: %08llx\n", Header->_NonResident.AttributeAllocated);
            printf("\tAttributeSize: %08llx\n", Header->_NonResident.AttributeSize);
            printf("\tStreamDataSize: %08llx\n", Header->_NonResident.StreamDataSize);

        } else {
            printf("Resident:\n");
            printf("\tAttributeLength: %08x\n", Header->_Resdient.AttributeLength);
            printf("\tAttributeOffset: %04x\n", Header->_Resdient.AttributeOffset);
            printf("\tIsIndexed: %02x\n", Header->_Resdient.IsIndexed);
        }

        printf("\nHex Dump:\n");
        PrintHexDump(AttrIter.Attribute, AttrIter.Attribute->Length);

        printf("\n\n");
    }

    NTFS_VolumeClose(&NtfsVolume);

    system("pause");
    return 0;
}

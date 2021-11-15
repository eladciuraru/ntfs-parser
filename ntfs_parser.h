
#ifndef _NTFS_PARSER_H
#define _NTFS_PARSER_H

#include <stdbool.h>
#include <stdint.h>

#define NTFS_Assert(Con, Msg) do { if (!(Con)) { __debugbreak(); } } while(0)
#define NTFS_Cast(Type, Target) ((Type)(Target))

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
bool        NTFS_MftReadRecord(ntfs_volume *Volume, uint64_t Index, void *Buffer);


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

typedef struct {
    uint32_t Magic;
    uint16_t UpdateSequenceOffset;
    uint16_t UpdateSequenceSize;
    uint64_t LogFileSequenceNumber;
    uint16_t SequenceNumber;
    uint16_t HardLinkCount;
    uint16_t AttributesOffset;
    uint16_t Flags;  // 0x01 - in use, 0x02 - is directory
    uint32_t RecordRealSize;
    uint32_t RecordAllocatedSize;
    uint64_t FileReference;
    uint16_t NextAttributeID;
    uint16_t Unused;
    uint32_t RecordNumber;
} ntfs_file_record;

#define NTFS_FILE_RECORD_MAGIC 0x454C4946

#define NTFS_COMMON_ATTRIBUTE_HEADER \
    uint32_t Type;                   \
    uint32_t Length;                 \
    uint8_t  IsNonResident;          \
    uint8_t  NameLength;             \
    uint16_t NameOffset;             \
    uint16_t Flags;                  \
    uint16_t AttributeID

typedef union {
    struct {
        NTFS_COMMON_ATTRIBUTE_HEADER;
    };

    struct {
        NTFS_COMMON_ATTRIBUTE_HEADER;

        uint32_t AttributeLength;
        uint16_t AttributeOffset;
        uint8_t  IsIndexed;
        uint8_t  Padding;
    } _Resdient;

    struct {
        NTFS_COMMON_ATTRIBUTE_HEADER;

        uint64_t FirstCluster;
        uint64_t LastCluster;
        uint16_t DataRunOffset;
        uint16_t CompressionUnit;
        uint32_t Unused;
        uint64_t AttributeAllocated;
        uint64_t AttributeSize;
        uint64_t StreamDataSize;
    } _NonResident;
} ntfs_attr_header;

#undef NTFS_COMMON_ATTRIBUTE_HEADER

#pragma pack(pop)


typedef enum {
    // AttributeType_StandardInformation = ,
    // AttributeType_AttributeList       = ,
    // AttributeType_FileName            = ,
    // AttributeType_VolumeVersion       = ,
    // AttributeType_SecurityDescriptor  = ,
    // AttributeType_VolumeName          = ,
    // AttributeType_VolumeInformation   = ,
    AttributeType_Data                = 0x80,
    // AttributeType_IndexRoot           = ,
    // AttributeType_IndexAllocation     = ,
    AttributeType_SentinalEnd = -1,  // 0xFFFFFFFF
} ntfs_attr_type;

inline char *NTFS_AttributeType(ntfs_attr_type Type)
{
    char *Result = 0;
    switch (Type) {
        case AttributeType_Data: Result = "AttributeType_Data";    break;
        default:                 Result = "AttributeType_Unknown"; break;
    }

    return Result;
}

typedef struct {
    union {
        struct {
            uint8_t  LengthBytes: 4;
            uint8_t  OffsetBytes: 4;
        };

        uint8_t Header;
    };

    uint64_t Length;
    int64_t  Offset;

    // Internal
    void *_Pointer;
} ntfs_data_run;

typedef struct {
    ntfs_attr_header *Attribute;
} ntfs_attr_iter;

#endif  // _NTFS_PARSER_H


#ifdef NTFS_PARSER_IMPLEMENTATION

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


static inline void *NTFS_AdjustPointer_(void *Pointer, size_t Offset)
{
    void *Result = NTFS_Cast(uint8_t *, Pointer) + Offset;

    return Result;
}

// static inline size_t NTFS_OffsetPointers_(void *Pointer1, void *Pointer2)
// {
//     intptr_t Diff   = NTFS_Cast(uint8_t *, Pointer1) - NTFS_Cast(uint8_t *, Pointer2);
//     size_t   Result = (Diff < 0) ? -Diff : Diff;

//     return Result;
// }

static inline uint8_t NTFS_ConsumePointerU8_(void **Pointer)
{
    uint8_t *Pointer_ = NTFS_Cast(uint8_t *, *Pointer);
    uint8_t  Result   = *Pointer_;

    *Pointer = ++Pointer_;

    return Result;
}

static bool Win32_ReadFrom_(HANDLE Handle, uint64_t From, void *Buffer, size_t Size)
{
    NTFS_Assert(Size == NTFS_Cast(uint32_t, Size), "Not supporting reading 64bit size");

    LARGE_INTEGER From_ = { .QuadPart = From };
    SetFilePointerEx(Handle, From_, 0, FILE_BEGIN);

    DWORD BytesRead = 0;
    BOOL  Result    = ReadFile(Handle, Buffer, Size, &BytesRead, NULL);
    return Result && BytesRead == Size;
}

// TODO: Change memory allocation scheme
static void *Win32_AllocateMemory_(size_t Size)
{
    void *Result = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, Size);
    NTFS_Assert(Result != 0, "Failed to allocate memory");  // TODO: Handle this

    return Result;
}

static void Win32_FreeMemory_(void *Address)
{
    HeapFree(GetProcessHeap(), 0, Address);
}


bool NTFS_NextFileAttribute(ntfs_file_record *FileRecord, ntfs_attr_iter *Iter)
{
    if (Iter->Attribute == 0) {
        Iter->Attribute =
            NTFS_AdjustPointer_(FileRecord, FileRecord->AttributesOffset);
    } else {
        Iter->Attribute =
            NTFS_AdjustPointer_(Iter->Attribute, Iter->Attribute->Length);
    }

    if (Iter->Attribute->Type == AttributeType_SentinalEnd) {
        Iter->Attribute = 0;
        return false;
    }

    return true;
}


// TODO: Add support for multiple of the same attribute (e.g. multiple data streams)
ntfs_attr_header *NTFS_FindFileAttribute(ntfs_file_record *FileRecord, ntfs_attr_type AttrType)
{
    ntfs_attr_header *Result =
        NTFS_AdjustPointer_(FileRecord, FileRecord->AttributesOffset);

    do {
        if (Result->Type == AttrType) {
            break;
        }

        // Advance to next attribute
        Result = NTFS_AdjustPointer_(Result, Result->Length);
    } while (Result->Type != AttributeType_SentinalEnd);

    return Result;
}


ntfs_volume NTFS_VolumeOpen(char DriveLetter)
{
    ntfs_volume Volume = { 0 };

    wchar_t DrivePath[] = L"\\\\.\\ :";
    DrivePath[4]        = NTFS_Cast(wchar_t, DriveLetter);
    HANDLE VolumeHandle = CreateFileW(DrivePath, GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      0, OPEN_EXISTING, 0, 0);
    if (VolumeHandle == INVALID_HANDLE_VALUE) {
        goto SKIP;
    }
    Volume.Handle = VolumeHandle;

    ntfs_vbr BootSector;
    Win32_ReadFrom_(Volume.Handle, 0, &BootSector, sizeof(BootSector));
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


static bool NTFS_NextDataRun_(ntfs_attr_header *NonResAttr, ntfs_data_run *DataRun)
{
    NTFS_Assert(NonResAttr->IsNonResident, "Resident attribute is invalid here");

    if (DataRun->_Pointer == 0) {
        DataRun->_Pointer =
            NTFS_AdjustPointer_(NonResAttr,
                                NonResAttr->_NonResident.DataRunOffset);
    }

    if (DataRun->_Pointer >= NTFS_AdjustPointer_(NonResAttr, NonResAttr->_NonResident.Length)) {
        return false;
    }

    DataRun->Header = NTFS_ConsumePointerU8_(&DataRun->_Pointer);
    if (DataRun->Header == 0) {
        return false;
    }

    DataRun->Length = 0;
    for (int I = 0; I < DataRun->LengthBytes; I++) {
        DataRun->Length |= NTFS_ConsumePointerU8_(&DataRun->_Pointer) << (8 * I);
    }

    bool IsSigned   = false;
    DataRun->Offset = 0;
    for (int I = 0; I < DataRun->OffsetBytes; I++) {
        uint8_t Value = NTFS_ConsumePointerU8_(&DataRun->_Pointer);

        DataRun->Offset |= Value << (8 * I);
        IsSigned         = (Value & 0x80) != 0;
    }
    if (IsSigned) {
        DataRun->Offset = ~DataRun->Offset + 1;
    }

    return true;
}


bool NTFS_MftReadRecord(ntfs_volume *Volume, uint64_t Index, void *Buffer)
{
    bool     Result         = false;
    uint64_t VirtualCluster = Index * Volume->BytesPerMftEntry / Volume->BytesPerCluster;
    uint64_t VirtualOffset  = Index * Volume->BytesPerMftEntry % Volume->BytesPerCluster;

    // First file record is the MFT itself
    // NOTE: loading the MFT record each time to be synced with OS changes
    void *MftEntry = Win32_AllocateMemory_(Volume->BytesPerMftEntry);
    Win32_ReadFrom_(Volume->Handle, Volume->MftCluster * Volume->BytesPerCluster,
                    MftEntry, Volume->BytesPerMftEntry);

    ntfs_attr_header *DataAttr = NTFS_FindFileAttribute(MftEntry, AttributeType_Data);
    ntfs_data_run     DataRun  = { 0 };

    uint64_t PhysicalCluster = 0;
    while (NTFS_NextDataRun_(DataAttr, &DataRun) == true) {
        PhysicalCluster += DataRun.Offset;

        if (VirtualCluster < DataRun.Length) {
            // Found the relevant data run
            uint64_t EntryAddress = (PhysicalCluster + VirtualCluster) *
                                    Volume->BytesPerCluster + VirtualOffset;
            Win32_ReadFrom_(Volume->Handle, EntryAddress,
                            Buffer, Volume->BytesPerMftEntry);
            Result = true;
            break;

        } else {
            VirtualCluster -= DataRun.Length;
        }

        printf("Offset (%llx) - Length (%llx)\n", DataRun.Offset, DataRun.Length);
    }

    // TODO: Validate record

    Win32_FreeMemory_(MftEntry);

    return Result;
}


#endif  // NTFS_PARSER_IMPLEMENTATION

/* MIT License
 * 
 * Copyright (c) 2025 Mikael Penttinen
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * -----------------------------------------------------------------------------
 *
 * installer.c
 *
 * @brief {Short description of the source file}
*/

/*----------------------------------------------------------------------------*/
/* INCLUDE DIRECTIVES                                                         */
/*----------------------------------------------------------------------------*/

#include "app_status.h"
#include "app_types.h"
#include "crc32.h"
#include "installer.h"
#include "fragmentstore/command.h"
#include "fragmentstore/fragmentstore.h"
#include "ed25519.h"
#include "ed25519_extra.h"
#include "no_init_ram.h"

#include <assert.h>
#include <stdio.h>

#include "stm32f4xx_hal.h"

/*----------------------------------------------------------------------------*/
/* PRIVATE TYPE DEFINITIONS                                                   */
/*----------------------------------------------------------------------------*/

typedef struct
{
    FragmentArea_t      fa;             /* Fragment area handle */
    bool                valid;          /* This slot contains a valid firmware */
    uint32_t            highestAddr;    /* Highest address of this firmware */
    size_t              lastFragIdx;    /* Index of the last fragment */
    Metadata_t          metadata;       /* Metadata in the area */
    Fragment_t          fragMem;        /* Memory allocation for reading */
} InstallSlot_t;

typedef struct
{
    uint32_t startAddress;
    size_t   size;
    uint32_t handle;
} Stm32FlashSector_t;

/*----------------------------------------------------------------------------*/
/* MACRO DEFINITIONS                                                          */
/*----------------------------------------------------------------------------*/

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define member_size(type, member) (sizeof( ((type *)0)->member ))

#define KB (1024U)
#define MB (1024U * KB)

#define W25Qxx_SECTOR_SIZE  (4U*KB)
#define UPDATE_SLOT_SIZE    (2U*MB)

#define SLOT_0_ADDRESS          (0x0U)
#define SLOT_1_ADDRESS          (UPDATE_SLOT_SIZE)
#define SLOT_2_ADDRESS          (2U * UPDATE_SLOT_SIZE)
#define COMMAND_AREA_ADDRESS    (3U * UPDATE_SLOT_SIZE)

#define REQUIRE_V(x) \
if(!(x)) \
{ \
printf("%s failed!\r\n", #x);\
return; \
}

#define REQUIRE_B(x) \
if(!(x)) \
{ \
printf("%s failed!\r\n", #x);\
return false; \
}

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

/*----------------------------------------------------------------------------*/
/* VARIABLE DEFINITIONS                                                       */
/*----------------------------------------------------------------------------*/

static CommandArea_t        f_ca;
static InstallSlot_t        f_slots[3];
static w25qxx_handle_t*     f_w25q128;
static KeyContainer_t       f_keys;

static const Stm32FlashSector_t f_FLASH_SECTORS[FLASH_SECTOR_TOTAL] = {
    {0x08000000,  16U*KB, FLASH_SECTOR_0 },
    {0x08004000,  16U*KB, FLASH_SECTOR_1 },
    {0x08008000,  16U*KB, FLASH_SECTOR_2 },
    {0x0800C000,  16U*KB, FLASH_SECTOR_3 },
    {0x08010000,  64U*KB, FLASH_SECTOR_4 },
    {0x08020000, 128U*KB, FLASH_SECTOR_5 },
    {0x08040000, 128U*KB, FLASH_SECTOR_6 },
    {0x08060000, 128U*KB, FLASH_SECTOR_7 },
    {0x08080000, 128U*KB, FLASH_SECTOR_8 },
    {0x080A0000, 128U*KB, FLASH_SECTOR_9 },
    {0x080C0000, 128U*KB, FLASH_SECTOR_10 },
    {0x080E0000, 128U*KB, FLASH_SECTOR_11 },
    {0x08100000,  16U*KB, FLASH_SECTOR_12 },
    {0x08104000,  16U*KB, FLASH_SECTOR_13 },
    {0x08108000,  16U*KB, FLASH_SECTOR_14 },
    {0x0810C000,  16U*KB, FLASH_SECTOR_15 },
    {0x08110000,  64U*KB, FLASH_SECTOR_16 },
    {0x08120000, 128U*KB, FLASH_SECTOR_17 },
    {0x08140000, 128U*KB, FLASH_SECTOR_18 },
    {0x08160000, 128U*KB, FLASH_SECTOR_19 },
    {0x08180000, 128U*KB, FLASH_SECTOR_20 },
    {0x081A0000, 128U*KB, FLASH_SECTOR_21 },
    {0x081C0000, 128U*KB, FLASH_SECTOR_22 },
    {0x081E0000, 128U*KB, FLASH_SECTOR_23 },
};

static_assert(ARRAY_SIZE(f_FLASH_SECTORS) == FLASH_SECTOR_TOTAL, "Incomplete sector map");

static_assert((sizeof(Metadata_t) % sizeof(uint32_t)) == 0U, "Metadata not word aligned");
static_assert((member_size(Fragment_t, content) % sizeof(uint32_t)) == 0U, "Fragment content not word aligned");
static_assert(member_size(Metadata_t, metadataSignature) == 64U, "Signature size must be 64 bytes");
static_assert(member_size(Metadata_t, firmwareSignature) == 64U, "Signature size must be 64 bytes");
static_assert(member_size(Fragment_t, signature) == 64U, "Signature size must be 64 bytes");

/*----------------------------------------------------------------------------*/
/* CALLBACKS FOR FRAGMENT AREAS                                               */
/*----------------------------------------------------------------------------*/

static inline bool MetadataEqual(const Metadata_t* a, const Metadata_t* b)
{
    return 0 == memcmp(a, b, sizeof(Metadata_t));
}

static bool VerifyMemory(Address_t address, size_t size, const uint8_t* cmp)
{
    uint8_t buf[128];

    size_t pos = 0U;

    while (pos < size)
    {
        const size_t left = size - pos;
        const size_t blockSize = MIN(left, sizeof(buf));

        const Address_t readAddr = address + pos;
        if (0U != w25qxx_read(f_w25q128, readAddr, buf, blockSize))
        {
            return false;
        }

        if (0 != memcmp(buf, &cmp[pos], blockSize))
        {
            return false;
        }

        pos += blockSize;
    }

    return true;
}

/** Read fragment memory
 * 
 * @param address Read start address
 * @param size Read size
 * @param out Read data area
 * @return Read successful
 */
bool ReadMemory(Address_t address, size_t size, uint8_t* out)
{
    return 0U == w25qxx_read(f_w25q128, address, out, size);
}

/** Write fragment memory
 * 
 * @param address Write start address
 * @param size Write size
 * @param out Write data area
 * @return Write successful
 */
bool WriteMemory(Address_t address, size_t size, const uint8_t* in)
{
    if (0U != w25qxx_write(f_w25q128, address, (uint8_t*)in, size))
    {
        return false;
    }

    if (!VerifyMemory(address, size, in))
    {
        printf("Write verify failed miserably!\r\n");
        return false;
    }

    return true;
}

/** Erase fragment memory sectors
 * 
 * @param address Erase start address (even sector address)
 * @param size Erase size (multiple of sector size)
 * @return Erase successful
 */
bool EraseSectors(Address_t address, size_t size)
{
    const Address_t start = address;
    const Address_t end = address + size;

    for (Address_t pos = start; pos < end; pos += W25Qxx_SECTOR_SIZE)
    {
        if (0U != w25qxx_sector_erase_4k(f_w25q128, pos))
        {
            return false;
        }
    }

    return true;
}

/** Validate one fragment
 * 
 * @param frag Pointer to fragment structure
 * @return fragment is valid
 */
bool ValidateFragment(const Fragment_t* frag)
{
    const uint32_t fsa = frag->startAddress;
    const uint32_t fea = frag->startAddress + frag->size;
    
    const bool sizeOk = frag->size <= sizeof(frag->content);
    const bool fsaOk = fsa >= FIRST_FLASH_ADDRESS;
    const bool feaOk = fea <= LAST_FLASH_ADDRESS;

    return sizeOk && fsaOk && feaOk;
}

/** Validate one fragment
 * 
 * @param metadata Pointer to metadata structure
 * @return metadata is valid
 */
bool ValidateMetadata(const Metadata_t* metadata)
{
    if (metadata == NULL)
    {
        return false;
    }
    
    const uint8_t* msg = (const uint8_t*)metadata;
    const size_t msgLen = sizeof(Metadata_t) - sizeof(metadata->metadataSignature);

    int valid = ed25519_verify(
        metadata->metadataSignature, 
        msg, 
        msgLen,
        f_keys.metadataPubKey
    );

    return 1 == valid;
}

/*----------------------------------------------------------------------------*/
/* PRIVATE FUNCTION DEFINITIONS                                               */
/*----------------------------------------------------------------------------*/

static bool VerifySlotContent(InstallSlot_t* slot)
{
    Metadata_t* meta = &slot->metadata;
    Fragment_t* frag = &slot->fragMem;

    FA_ReturnCode_t res = FA_ReadMetadata(&slot->fa, meta);

    if (res == FA_ERR_OK)
    {
        ed25519_multipart_t ctx;
        int ed = ed25519_multipart_init(
            &ctx, 
            meta->firmwareSignature, 
            f_keys.firmwarePubKey
        );

        if (ed != 1U)
        {
            printf("ed25519_multipart_init failed");
            return false;
        }

        size_t lastIdx = 0U;
        res = FA_FindLastFragment(&slot->fa, frag, &lastIdx);

        if (res != FA_ERR_OK)
        {
            printf("FA_FindLastFragment failed!\r\n");
            return false;
        }

        slot->lastFragIdx = lastIdx;

        uint32_t nextStart = (meta->type == APP_TYPE_RESCUE)
            ? RESCUE_DATA_BEGIN
            : FIRST_FLASH_ADDRESS;

        for (size_t i = 0; i <= lastIdx; i++)
        {
            res = FA_ReadFragment(&slot->fa, i, frag);
            if (res != FA_ERR_OK)
            {
                printf("Fragment %u was not valid\r\n", i);
                return false;
            }

            if (frag->startAddress != nextStart)
            {
                printf("Fragment %u: unexpected start address: %lX, expected %lX\r\n", i, frag->startAddress, nextStart);
                return false;
            }
            else
            {
                nextStart += frag->size;
            }

            uint32_t verifyOffset = 0U;
            size_t   verifyLen = frag->size;

            if (frag->startAddress < meta->startAddress)
            {
                verifyOffset = meta->startAddress - frag->startAddress;
            }

            if (verifyOffset < verifyLen)
            {
                verifyLen -= verifyOffset;
            }

            if (verifyLen > 0U)
            {
                ed = ed25519_multipart_continue(&ctx, &frag->content[verifyOffset], verifyLen);
                if (ed != 1U)
                {
                    printf("ed25519_multipart_continue failed\r\n");
                    return false;
                }
            }

            const uint32_t fragEndAddr = frag->startAddress + frag->size;
            if (fragEndAddr > slot->highestAddr)
            {
                slot->highestAddr = fragEndAddr;
            }
        }

        ed = ed25519_multipart_end(&ctx);
        if (ed != 1U)
        {
            printf("ed25519_multipart_end failed\r\n");
            return false;
        }

        slot->valid = true;
        return true;
    }

    return false;
}

static inline bool InRange(uint32_t val, uint32_t low, uint32_t high)
{
    return (val >= low) && (val <= high);
}

static bool EraseRequiredSectors(uint32_t startAddress, uint32_t endAddress)
{
    bool eraseActive = false;

    for (size_t i = 0; i < FLASH_SECTOR_TOTAL; i++)
    {
        const Stm32FlashSector_t* sec = &f_FLASH_SECTORS[i];

        const uint32_t secStart = sec->startAddress;
        const uint32_t secEnd = sec->startAddress + sec->size - 1U;

        if (InRange(startAddress, secStart, secEnd))
        {
            eraseActive = true;
        }

        if (eraseActive)
        {
            printf("Erasing sector %lu\r\n", sec->handle);

            
            FLASH_EraseInitTypeDef eraseInitStruct;
            uint32_t error = 0;
            
            eraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
            eraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; // 2.7V to 3.6V
            eraseInitStruct.Sector       = sec->handle;
            eraseInitStruct.NbSectors    = 1;
            
            HAL_FLASH_Unlock();
            HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&eraseInitStruct, &error);
            HAL_FLASH_Lock();

            if (status != HAL_OK)
            {
                printf("Sector erase failed error code %lu\r\n", error);
                return false;
            }
        }

        if (InRange(endAddress, secStart, secEnd))
        {
            eraseActive = false;
            break;
        }
    }

    return true;
}

static inline bool FlashAligned(uint32_t val)
{
    return (val & ~0xFFFFFFFCU) == 0U;
}

static inline uint32_t FlashAlignLow(uint32_t val)
{
    return val & 0xFFFFFFFCU;
}

static inline uint32_t FlashAlignHigh(uint32_t val)
{
    while(!FlashAligned(val))
    {
        val++;
    }

    return val;
}

static bool ProgramFlash(uint32_t address, const uint8_t* data, size_t size)
{
    printf("Programming %u bytes to address %08lX\r\n", size, address);
    uint32_t startAddress = address;
    uint32_t endAddress = address + size;

    if (!InRange(startAddress, APP_METADATA_ADDRESS, LAST_FLASH_ADDRESS) ||
        !InRange(endAddress, APP_METADATA_ADDRESS, LAST_FLASH_ADDRESS))
    {
        printf("Write request exceeds flash boundaries!\r\n");
        return false;
    }

    HAL_FLASH_Unlock();

    uint32_t startWord = FlashAlignHigh(startAddress);
    uint32_t endWord = FlashAlignLow(endAddress);
    uint32_t i = 0U;

    for (uint32_t addr = startAddress; addr < startWord; addr++)
    {
        HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr, data[i]);
        i++;
        
        if (status != HAL_OK)
        {
            printf("HAL_FLASH_Program failed with status %i\r\n", (int)status);
            HAL_FLASH_Lock();
            return false;
        }
    }

    for (uint32_t addr = startWord; addr < endWord; addr += 4U)
    {
        const uint32_t* word = (const uint32_t*)(&data[i]);
        HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, *word);
        i += 4U;
        
        if (status != HAL_OK)
        {
            printf("HAL_FLASH_Program failed with status %i\r\n", (int)status);
            HAL_FLASH_Lock();
            return false;
        }
    }

    for (uint32_t addr = endWord; addr < endAddress; addr++)
    {
        HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr, data[i]);
        i++;
        
        if (status != HAL_OK)
        {
            printf("HAL_FLASH_Program failed with status %i\r\n", (int)status);
            HAL_FLASH_Lock();
            return false;
        }
    }

    HAL_FLASH_Lock();
    return true;
}

static bool InstallFrom(InstallSlot_t* slot)
{
    if (!slot->valid)
    {
        return false;
    }

    Metadata_t* meta = &slot->metadata;
    Fragment_t* frag = &slot->fragMem;

    const uint32_t metadataAddress = (meta->type == APP_TYPE_RESCUE)
        ? RESCUE_METADATA_ADDRESS
        : APP_METADATA_ADDRESS;

    if (!ValidateMetadata(meta))
    {
        printf("Install target metadata reverification failed!\r\n");
        return false;
    }

    if (!EraseRequiredSectors(metadataAddress, slot->highestAddr))
    {
        return false;
    }

    if (!ProgramFlash(metadataAddress, (const uint8_t*)meta, sizeof(Metadata_t)))
    {
        return false;
    }

    for (size_t i = 0; i <= slot->lastFragIdx; i++)
    {
        FA_ReturnCode_t res = FA_ReadFragment(&slot->fa, i, frag);

        if (res != FA_ERR_OK)
        {
            printf("FA_ReadFragment failed!\r\n");
            return false;
        }

        if (!ProgramFlash(frag->startAddress, frag->content, frag->size))
        {
            return false;
        }
    }

    return true;
}

static bool EmptyMetadata(const Metadata_t* m)
{
    const uint8_t* buf = (const uint8_t*)m;
    for (size_t i = 0; i < sizeof(Metadata_t); i++)
    {
        if (buf[i] != 0x0)
        {
            return false;
        }
    }
    return true;
}

static bool InstallAllowed(const Metadata_t* target, bool automaticRollback)
{
    const Metadata_t* app = (target->type == APP_TYPE_RESCUE)
        ? RESCUE_STATUS_GetMetadata()
        : APP_STATUS_GetMetadata();

    const bool appValid = (target->type == APP_TYPE_RESCUE)
        ? RESCUE_STATUS_LastVerifyResult()
        : APP_STATUS_LastVerifyResult();

    if (!appValid)
    {
        return true;
    }

    if (automaticRollback && 
        (target->type == app->type) &&
        (NO_INIT_RAM_content.installTag == APP_TAG_TRYOUT))
    {
        return true;
    }

    if ((target->type == app->type) &&
        (target->rollbackNumber >= app->rollbackNumber))
    {
        return true;
    }

    if ((target->type != APP_TYPE_RESCUE) &&
        (app->type == APP_TYPE_RESCUE))
    {
        return true;
    }

    return false;
}

static bool ExecuteInstallCommand(const Metadata_t* metaArg)
{
    CommandStatus_t status = CA_GetStatus(&f_ca);

    if (status == COMMAND_STATE_FAILED)
    {
        printf("Install request has failed before. Quitting!\r\n");
        return false;
    }
    
    InstallSlot_t* slot = NULL;
    
    for (size_t i = 0; i < ARRAY_SIZE(f_slots); i++)
    {
        if (f_slots[i].valid && 
            (0 == memcmp(metaArg, &f_slots[i].metadata, sizeof(Metadata_t))))
        {
            slot = &f_slots[i];
            printf("Found target firmware from slot %u\r\n", i);
            break;
        }
    }
    
    if (slot == NULL)
    {
        printf("Target firmware not found! Install failed!\r\n");
        REQUIRE_B(CA_SetStatus(&f_ca, COMMAND_STATE_FAILED));
        return false;
    }
    
    if (!InstallAllowed(metaArg, false))
    {
        printf("Install prevented by anti-rollback logic!\n\r");
        REQUIRE_B(CA_SetStatus(&f_ca, COMMAND_STATE_FAILED));
        return false;
    }

    if (status == COMMAND_STATE_NONE)
    {
        // Skip history write if the app was not valid!
        if (APP_STATUS_LastVerifyResult())
        {
            REQUIRE_B(CA_WriteHistory(&f_ca, (const Metadata_t*)APP_METADATA_ADDRESS));
        }
        REQUIRE_B(CA_SetStatus(&f_ca, COMMAND_STATE_HISTORY_WRITTEN));
        status = COMMAND_STATE_HISTORY_WRITTEN;
        printf("History written\r\n");
    }

    if (status == COMMAND_STATE_HISTORY_WRITTEN)
    {
        if (InstallFrom(slot))
        {
            REQUIRE_B(CA_SetStatus(&f_ca, COMMAND_STATE_FIRMWARE_WRITTEN));
            status = COMMAND_STATE_FIRMWARE_WRITTEN;
        }
        else
        {
            printf("Installation from slot failed!\r\n");
            REQUIRE_B(CA_SetStatus(&f_ca, COMMAND_STATE_FAILED));
            return false;
        }
    }

    if (status == COMMAND_STATE_FIRMWARE_WRITTEN)
    {
        return CA_EraseInstallCommand(&f_ca);
    }

    return false;
}

static bool ExecuteRollbackCommand(Metadata_t* metaArg, bool automaticRollback)
{
    CommandStatus_t status = CA_GetStatus(&f_ca);

    if (status == COMMAND_STATE_FAILED)
    {
        printf("Rollback request has failed before. Quitting!\r\n");
        return false;
    }

    InstallSlot_t* slot = NULL;
    
    if (EmptyMetadata(metaArg))
    {
        if (!CA_ReadHistory(&f_ca, metaArg))
        {
            printf("Cannot read previous firmware! Rollback failed!\r\n");
            REQUIRE_B(CA_SetStatus(&f_ca, COMMAND_STATE_FAILED));
            return false;
        }
    }

    if (APP_STATUS_LastVerifyResult() &&
        MetadataEqual(metaArg, APP_STATUS_GetMetadata()))
    {
        printf("Unable to perform rollback to the same version as currently installed!\r\n");
        return false;
    }

    for (size_t i = 0; i < ARRAY_SIZE(f_slots); i++)
    {
        if (f_slots[i].valid && 
            (0 == memcmp(metaArg, &f_slots[i].metadata, sizeof(Metadata_t))))
        {
            slot = &f_slots[i];
            printf("Found target rollback firmware from slot %u\r\n", i);
            break;
        }
    }

    if (slot == NULL)
    {
        printf("Target rollback firmware not found! Install failed!\r\n");
        REQUIRE_B(CA_SetStatus(&f_ca, COMMAND_STATE_FAILED));
        return false;
    }

    if (!InstallAllowed(metaArg, automaticRollback))
    {
        printf(
            "Rollback prevented by anti-rollback logic when rolling back %s\n\r",
            automaticRollback ? "automatically" : "manually"
        );
    }

    if (status == COMMAND_STATE_NONE)
    {
        // Do not actually update the history state
        REQUIRE_B(CA_SetStatus(&f_ca, COMMAND_STATE_HISTORY_WRITTEN));
        status = COMMAND_STATE_HISTORY_WRITTEN;
        printf("History state set. History not updated\r\n");
    }

    if (status == COMMAND_STATE_HISTORY_WRITTEN)
    {
        if (InstallFrom(slot))
        {
            REQUIRE_B(CA_SetStatus(&f_ca, COMMAND_STATE_FIRMWARE_WRITTEN));
            status = COMMAND_STATE_FIRMWARE_WRITTEN;
        }
        else
        {
            printf("Installation from slot failed!\r\n");
            REQUIRE_B(CA_SetStatus(&f_ca, COMMAND_STATE_FAILED));
            return false;
        }
    }

    if (status == COMMAND_STATE_FIRMWARE_WRITTEN)
    {
        return CA_EraseInstallCommand(&f_ca);
    }

    return false;
}

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION DEFINITIONS                                                */
/*----------------------------------------------------------------------------*/

void INSTALLER_InitAreas(w25qxx_handle_t* w25q128, const KeyContainer_t* keys)
{
    REQUIRE_V(w25q128 != NULL);
    REQUIRE_V(keys != NULL);

    f_w25q128 = w25q128;
    f_keys = *keys;

    static const MemoryConfig_t memConfs[] = {
        {
            .baseAddress = SLOT_0_ADDRESS,
            .sectorSize = W25Qxx_SECTOR_SIZE,
            .memorySize = UPDATE_SLOT_SIZE,
            .eraseValue = 0xFF,

            .Reader = ReadMemory,
            .Writer = WriteMemory,
            .Eraser = EraseSectors,
        },
        {
            .baseAddress = SLOT_1_ADDRESS,
            .sectorSize = W25Qxx_SECTOR_SIZE,
            .memorySize = UPDATE_SLOT_SIZE,
            .eraseValue = 0xFF,

            .Reader = ReadMemory,
            .Writer = WriteMemory,
            .Eraser = EraseSectors,
        },
        {
            .baseAddress = SLOT_2_ADDRESS,
            .sectorSize = W25Qxx_SECTOR_SIZE,
            .memorySize = UPDATE_SLOT_SIZE,
            .eraseValue = 0xFF,

            .Reader = ReadMemory,
            .Writer = WriteMemory,
            .Eraser = EraseSectors,
        },
        {
            .baseAddress = COMMAND_AREA_ADDRESS,
            .sectorSize = W25Qxx_SECTOR_SIZE,
            .memorySize = 3U * W25Qxx_SECTOR_SIZE,
            .eraseValue = 0xFF,

            .Reader = ReadMemory,
            .Writer = WriteMemory,
            .Eraser = EraseSectors,
        }
    };

    _Static_assert((ARRAY_SIZE(f_slots) + 1U) <= ARRAY_SIZE(memConfs), "Not enough memconfs");

    REQUIRE_V(CA_InitStruct(&f_ca, &memConfs[3], &InlineCrc32));

    for (size_t i = 0; i < ARRAY_SIZE(f_slots); i++)
    {
        REQUIRE_V(FA_ERR_OK == FA_InitStruct(&f_slots[i].fa, &memConfs[i], ValidateFragment, ValidateMetadata));
        if (VerifySlotContent(&f_slots[i]))
        {
            printf(
                "Install slot %i contains a valid %s\r\n",
                i,
                (f_slots[i].metadata.type == APP_TYPE_RESCUE)
                    ? "rescue app"
                    : "firmware"
            );
        }
        else
        {
            printf("Install slot %i does not contain a valid binary\r\n", i);
        }
    }

}

bool INSTALLER_CheckInstallRequest(void)
{
    Metadata_t metaArg;
    CommandType_t cmd;

    if (CA_ReadInstallCommand(&f_ca, &cmd, &metaArg))
    {
        if (cmd == COMMAND_TYPE_INSTALL_FIRMWARE)
        {
            printf("Install command read!\r\n");
            return ExecuteInstallCommand(&metaArg);
        }
        else if (cmd == COMMAND_TYPE_ROLLBACK)
        {
            printf("Rollback command read!\r\n");
            return ExecuteRollbackCommand(&metaArg, false);
        }
        else
        {
            printf("Unknown command read: %i!\r\n", (int)cmd);
        }
    }

    printf("No install command set!\r\n");

    if (NO_INIT_RAM_content.appTag == APP_TAG_INVALID)
    {
        printf("Application invalid flag set!\r\n");

        if (CA_ReadHistory(&f_ca, &metaArg))
        {
            CommandStatus_t status = CA_GetStatus(&f_ca);
            if (status != COMMAND_STATE_FAILED)
            {
                /* Erase all other progress except previously failed operation */
                (void)CA_EraseInstallCommand(&f_ca);
            }
            return ExecuteRollbackCommand(&metaArg, true);
        }
        else
        {
            printf("Cannot find history for automatic rollback!\r\n");
            return false;
        }
    }

    return false;
}

bool INSTALLER_TryRepair(void)
{
    if (APP_STATUS_LastMetadataVerifyResult() &&
        !APP_STATUS_LastVerifyResult())
    {
        /* Try repairing the current application if the metadata was ok */
        /* but content was not */
        return ExecuteInstallCommand(APP_STATUS_GetMetadata());
    }

    return false;
}

bool INSTALLER_TryInstallRescueApp(const Metadata_t** out)
{
    for (size_t i = 0; i < ARRAY_SIZE(f_slots); i++)
    {
        if (f_slots[i].valid && f_slots[i].metadata.type == APP_TYPE_RESCUE)
        {
            *out = &f_slots[i].metadata;
            return InstallFrom(&f_slots[i]);
        }
    }
    return false;
}

/* EoF installer.c */

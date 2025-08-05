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

#include "installer.h"
#include "fragmentstore/fragmentstore.h"
#include "ed25519.h"

#include <assert.h>
#include <stdio.h>

/*----------------------------------------------------------------------------*/
/* PRIVATE TYPE DEFINITIONS                                                   */
/*----------------------------------------------------------------------------*/

typedef struct
{
    FragmentArea_t  fa;         /* Fragment area handle */
    bool            valid;      /* This slot contains a valid firmware */
    Metadata_t      metadata;   /* Metadata in the area */
    Fragment_t      fragMem;    /* Memory allocation for reading */
} InstallSlot_t;

/*----------------------------------------------------------------------------*/
/* MACRO DEFINITIONS                                                          */
/*----------------------------------------------------------------------------*/

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define KB (1024U)
#define MB (1024U * KB)

#define W25Qxx_SECTOR_SIZE  (4U*KB)
#define UPDATE_SLOT_SIZE    (2U*MB)

#define SLOT_0_ADDRESS      (0x0U)
#define SLOT_1_ADDRESS      (UPDATE_SLOT_SIZE)
#define SLOT_2_ADDRESS      (2U * UPDATE_SLOT_SIZE)

#define REQUIRE(x) \
if(!(x)) \
{ \
printf("%s failed!\r\n", #x);\
return; \
}

#define REQUIRE_ELSE(x, action) \
if(!(x)) \
{ \
printf("%s failed!\r\n", #x);\
action; \
return; \
}

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

/*----------------------------------------------------------------------------*/
/* VARIABLE DEFINITIONS                                                       */
/*----------------------------------------------------------------------------*/

static InstallSlot_t f_slots[1];
static w25qxx_handle_t* f_w25q128;
static InstallerKeys_t f_keys;

/*----------------------------------------------------------------------------*/
/* CALLBACKS FOR FRAGMENT AREAS                                               */
/*----------------------------------------------------------------------------*/

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
    (void)frag;
    return true;
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

    return 1 == ed25519_verify(
        metadata->metadataSignature, 
        msg, 
        msgLen,
        f_keys.metadataPubKey
    );
}

/*----------------------------------------------------------------------------*/
/* PRIVATE FUNCTION DEFINITIONS                                               */
/*----------------------------------------------------------------------------*/

static bool VerifySlotContent(InstallSlot_t* slot)
{
    FA_ReturnCode_t res = FA_ReadMetadata(&slot->fa, &slot->metadata);
    
    if (res == FA_ERR_OK)
    {
        /* TODO: Verify that the integrity of the firmware is valid */
        return true;
    }

    return false;
}

static bool InstallFrom(InstallSlot_t* slot)
{
    /* TODO: Install to system flash */
    (void)slot;
    return false;
}

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION DEFINITIONS                                                */
/*----------------------------------------------------------------------------*/

void INSTALLER_InitAreas(w25qxx_handle_t* w25q128, const InstallerKeys_t* keys)
{
    REQUIRE(w25q128 != NULL);
    REQUIRE(keys != NULL);

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
        }
    };

    _Static_assert(ARRAY_SIZE(f_slots) <= ARRAY_SIZE(memConfs), "Not enough memconfs");

    for (size_t i = 0; i < ARRAY_SIZE(f_slots); i++)
    {
        REQUIRE(FA_ERR_OK == FA_InitStruct(&f_slots[i].fa, &memConfs[i], ValidateFragment, ValidateMetadata));
        if (VerifySlotContent(&f_slots[i]))
        {
            printf("Fragment area at %lX contains a valid firmware\r\n", memConfs[i].baseAddress);
        }
        else
        {
            printf("Fragment area at %lX does not contain a valid firmware\r\n", memConfs[i].baseAddress);
        }
    }

}

bool INSTALLER_CheckInstallRequest(void)
{
    (void)InstallFrom(&f_slots[0]);
    return false;
}

/* EoF installer.c */

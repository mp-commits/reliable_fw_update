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
 * app_status.c
 *
 * @brief {Short description of the source file}
*/

/*----------------------------------------------------------------------------*/
/* INCLUDE DIRECTIVES                                                         */
/*----------------------------------------------------------------------------*/

#include "app_status.h"
#include "ed25519.h"
#include "crc32.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>

/*----------------------------------------------------------------------------*/
/* PRIVATE TYPE DEFINITIONS                                                   */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* MACRO DEFINITIONS                                                          */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* VARIABLE DEFINITIONS                                                       */
/*----------------------------------------------------------------------------*/

static bool f_metadataOk = false;
static bool f_valid = false;

#ifdef ENABLE_RESCUE_PARTITION
static bool f_rescueValid = false;
#endif

/*----------------------------------------------------------------------------*/
/* PRIVATE FUNCTION DEFINITIONS                                               */
/*----------------------------------------------------------------------------*/

static bool InRange(uint32_t val, uint32_t low, uint32_t high)
{
    return (val >= low) && (val <= high);
}

static bool IsMetadataValid(const Metadata_t* metadata, const uint8_t* publicKey)
{
    const uint8_t* sign = metadata->metadataSignature;
    const uint8_t*  msg = (const uint8_t*)metadata;
    size_t          len = sizeof(Metadata_t) - sizeof(metadata->metadataSignature);
    
    if (1 != ed25519_verify(sign, msg, len, publicKey))
    {
        return false;
    }

    /* Add \0 terminator to metadata magic string */
    char metaStr[sizeof(metadata->magic) + 1U];
    memcpy(metaStr, metadata->magic, sizeof(metadata->magic));
    metaStr[sizeof(metadata->magic)] = '\0';

    if (0 != strcmp(metaStr, "_M_E_T_A_D_A_T_A"))
    {
        return false;
    }

    const uint32_t start = metadata->startAddress;
    const uint32_t end = metadata->startAddress + metadata->firmwareSize;

    if (!InRange(start, FIRST_FLASH_ADDRESS, LAST_FLASH_ADDRESS))
    {
        return false;
    }

    if (!InRange(end, FIRST_FLASH_ADDRESS, LAST_FLASH_ADDRESS))
    {
        return false;
    }

    return true;
}

static bool IsApplicationValid(const Metadata_t* metadata, const uint8_t* publicKey)
{
    const uint8_t* sig = metadata->firmwareSignature;
    const uint8_t* msg = (const uint8_t*)(metadata->startAddress);
    const size_t   len = metadata->firmwareSize;

    if (ed25519_verify(sig, msg, len, publicKey))
    {
        uint32_t sp = *(volatile uint32_t*)metadata->startAddress;
        uint32_t pc = *(volatile uint32_t*)(metadata->startAddress + 4);

        const bool stackPointerValid = sp == 0x20030000U;
        const bool programCounterValid = InRange(pc, FIRST_FLASH_ADDRESS, LAST_FLASH_ADDRESS);

        return stackPointerValid && programCounterValid;
    }

    return false;
}

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION DEFINITIONS                                                */
/*----------------------------------------------------------------------------*/

bool APP_STATUS_Verify(const KeyContainer_t* keys)
{
    f_metadataOk = false;
    f_valid = false;
    const Metadata_t* metadata = (const Metadata_t*)(APP_METADATA_ADDRESS);

    if (IsMetadataValid(metadata, keys->metadataPubKey))
    {
        f_metadataOk = true;

        if (IsApplicationValid(metadata, keys->firmwarePubKey))
        {
            f_valid = true;
            return true;
        }
    }

    return false;
}

const Metadata_t* APP_STATUS_GetMetadata(void)
{
    return (const Metadata_t*)APP_METADATA_ADDRESS;
}

bool APP_STATUS_LastVerifyResult(void)
{
    return f_valid;
}

bool APP_STATUS_LastMetadataVerifyResult(void)
{
    return f_metadataOk;
}

void APP_STATUS_PrintMetadata(const Metadata_t* metadata)
{
    const uint32_t fwSignCrc = InlineCrc32(metadata->firmwareSignature, sizeof(metadata->firmwareSignature));
    const uint32_t metaSignCrc = InlineCrc32(metadata->metadataSignature, sizeof(metadata->metadataSignature));

    /* Add \0 terminator to metadata magic string */
    char metaStr[sizeof(metadata->magic) + 1U];
    memcpy(metaStr, metadata->magic, sizeof(metadata->magic));
    metaStr[sizeof(metadata->magic)] = '\0';

    char nameStr[sizeof(metadata->name) + 1U];
    memcpy(nameStr, metadata->name, sizeof(metadata->name));
    nameStr[strnlen(metadata->name, sizeof(metadata->name))] = '\0';

    printf("Metadata magic:           %s\r\n", metaStr);
    printf("Firmware type:            %lu\r\n", metadata->type);
    printf("Firmware version:         0x%lX\r\n", metadata->version);
    printf("Firmware rollback number: %lu\r\n", metadata->rollbackNumber);
    printf("Firmware ID:              0x%lX\r\n", metadata->firmwareId);
    printf("Firmware start address:   0x%lX\r\n", metadata->startAddress);
    printf("Firmware size:            0x%lX\r\n", metadata->firmwareSize);
    printf("Firmware name:            %s\r\n", nameStr);
    printf("Firmware signature CRC32: 0x%lX\r\n", fwSignCrc);
    printf("Metadata signature CRC32: 0x%lX\r\n", metaSignCrc);
}

bool RESCUE_STATUS_Verify(const KeyContainer_t* keys)
{
#ifdef ENABLE_RESCUE_PARTITION
    const Metadata_t* metadata = (const Metadata_t*)RESCUE_METADATA_ADDRESS;

    if (IsMetadataValid(metadata, keys->metadataPubKey))
    {
        if (IsApplicationValid(metadata, keys->firmwarePubKey))
        {
            f_rescueValid = true;
            return true;
        }
    }

    return false;
#else
    (void)keys;
    return f_valid;
#endif
}

const Metadata_t* RESCUE_STATUS_GetMetadata(void)
{
    return (const Metadata_t*)RESCUE_METADATA_ADDRESS;
}

bool RESCUE_STATUS_LastVerifyResult(void)
{
#ifdef ENABLE_RESCUE_PARTITION
    return f_rescueValid;
#else
    return f_valid;
#endif
}

/* EoF app_status.c */

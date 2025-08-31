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
 * udpserver.c
 *
 * @brief {Short description of the source file}
*/

/*----------------------------------------------------------------------------*/
/* INCLUDE DIRECTIVES                                                         */
/*----------------------------------------------------------------------------*/

#include <string.h>
#include <stdio.h>

#include "cmsis_os.h"
#include "stm32f4xx_it.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include "lwip/api.h"

#include "bigendian.h"
#include "crc32.h"
#include "metadata.h"
#include "server.h"
#include "system_reset.h"

#include "fragmentstore/fragmentstore.h"
#include "fragmentstore/command.h"
#include "updateserver/transfer.h"

/*----------------------------------------------------------------------------*/
/* PRIVATE TYPE DEFINITIONS                                                   */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* MACRO DEFINITIONS                                                          */
/*----------------------------------------------------------------------------*/

#define UDP_PORT 7

#define KB (1024U)
#define MB (1024U * KB)

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

#define SET_ADDRESS(var, address, port) \
memset(&server_addr, 0, sizeof(server_addr)); \
server_addr.sin_family = AF_INET; \
server_addr.sin_port = htons(port); \
server_addr.sin_addr.s_addr = address;

#define W25Qxx_SECTOR_SIZE (4U*KB)
#define UPDATE_SLOT_SIZE    (2U*MB)

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

/*----------------------------------------------------------------------------*/
/* VARIABLE DEFINITIONS                                                       */
/*----------------------------------------------------------------------------*/

static bool f_resetRequest;
static UpdateServer_t f_us;
static TransferBuffer_t f_tb;
static FragmentArea_t f_fa[3];
static Metadata_t f_metadata[3];
static CommandArea_t f_ca;
static uint8_t f_memBlock[5 * 1024];
static w25qxx_handle_t* f_w25q128 = NULL;

/*----------------------------------------------------------------------------*/
/* PRIVATE FUNCTION DEFINITIONS                                               */
/*----------------------------------------------------------------------------*/

static bool MetadataEqual(const Metadata_t* a, const Metadata_t* b)
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
    (void)metadata;
    return true;
}

static uint8_t ReadDataById(
    uint8_t id, 
    uint8_t* out, 
    size_t maxSize, 
    size_t* readSize)
{
    if (maxSize < 16U)
    {
        return PROTOCOL_NACK_INTERNAL_ERROR;
    }
    switch (id)
    {
    case PROTOCOL_DATA_ID_FIRMWARE_VERSION:
        *readSize = BE_PutU32(out, FIRMWARE_METADATA.version);
        return PROTOCOL_ACK_OK;
    case PROTOCOL_DATA_ID_FIRMWARE_TYPE:
        *readSize = BE_PutU32(out, FIRMWARE_METADATA.type);
        return PROTOCOL_ACK_OK;
    case PROTOCOL_DATA_ID_FIRMWARE_NAME:
        memcpy(out, FIRMWARE_METADATA.name, sizeof(FIRMWARE_METADATA.name));
        *readSize = sizeof(FIRMWARE_METADATA.name);
        return PROTOCOL_ACK_OK;
    default:
        return PROTOCOL_NACK_REQUEST_OUT_OF_RANGE;
    }
}

static uint8_t WriteDataById(
    uint8_t id, 
    const uint8_t* in, 
    size_t size)
{
    switch (id)
    {
    case PROTOCOL_DATA_ID_FIRMWARE_UPDATE:
        if (size != sizeof(Metadata_t))
        {
            printf("Invalid update command size: %u\r\n", size);
            return PROTOCOL_NACK_INVALID_REQUEST;
        }
        printf("Received update metadata %lX\r\n", InlineCrc32(in, size));
        const Metadata_t* metadata = (const Metadata_t*)in;
        if (!ValidateMetadata(metadata))
        {
            printf("Update metadata validity check failed!\r\n");
            return PROTOCOL_NACK_INVALID_REQUEST;
        }
        if (!CA_WriteInstallCommand(&f_ca, COMMAND_TYPE_INSTALL_FIRMWARE, metadata))
        {
            printf("Writing update command failed!\r\n");
            return PROTOCOL_NACK_BUSY_REPEAT_REQUEST;
        }
        return PROTOCOL_ACK_OK;

    case PROTOCOL_DATA_ID_FIRMWARE_ROLLBACK:
        if (size == sizeof(Metadata_t))
        {
            printf("Received specific rollback command to %lx\r\n", InlineCrc32(in, size));
            const Metadata_t* metadata = (const Metadata_t*)in;
            if (!ValidateMetadata(metadata))
            {
                printf("Rollback metadata validity check failed!\r\n");
                return PROTOCOL_NACK_INVALID_REQUEST;
            }
            if (!CA_WriteInstallCommand(&f_ca, COMMAND_TYPE_ROLLBACK, metadata))
            {
                printf("Writing rollback command failed!\r\n");
                return PROTOCOL_NACK_BUSY_REPEAT_REQUEST;
            }
        }
        else
        {
            printf("Received unspecific rollback command\r\n");
            if (!CA_WriteInstallCommand(&f_ca, COMMAND_TYPE_ROLLBACK, NULL))
            {
                printf("Writing rollback command failed!\r\n");
                return PROTOCOL_NACK_BUSY_REPEAT_REQUEST;
            }
        }

        return PROTOCOL_ACK_OK;

    case PROTOCOL_DATA_ID_RESET:
        printf("Received reset request!\r\n");
        f_resetRequest = true;
        return PROTOCOL_ACK_OK;

    case PROTOCOL_DATA_ID_ERASE_SLOT:
        if ((size == 1U) && (*in < 3U))
        {
            const uint8_t slot = *in;
            printf("Erasing slot %i...\r\n", (int)slot);
            const FA_ReturnCode_t res = FA_EraseArea(&f_fa[slot]);
            if (res == FA_ERR_OK)
            {
                printf("OK\r\n");
                memset(&f_metadata[slot], 0, sizeof(Metadata_t));
                return PROTOCOL_ACK_OK;
            }
            
            printf("FAILED\r\n");
            return PROTOCOL_NACK_BUSY_REPEAT_REQUEST;
        }
        else
        {
            return PROTOCOL_NACK_INVALID_REQUEST;
        }

    default:
        return PROTOCOL_NACK_REQUEST_OUT_OF_RANGE;
    }
}

static uint8_t PutMetadata(
    const uint8_t* data, 
    size_t size)
{
    printf("Received metadata %lX\r\n", InlineCrc32(data, size));

    if (size != sizeof(Metadata_t))
    {
        return PROTOCOL_NACK_REQUEST_OUT_OF_RANGE;
    }

    // Find usable slot for the incoming metadata
    size_t slot = 0;
    for (size_t i = 0; i < 3; i++)
    {
        if (!MetadataEqual(&f_metadata[i], &FIRMWARE_METADATA))
        {
            slot = i;
            break;
        }
    }

    FA_ReturnCode_t code =  FA_WriteMetadata(&f_fa[slot], (const Metadata_t*)data);

    if (code == FA_ERR_OK)
    {
        (void)memcpy(&f_metadata[slot], (const Metadata_t*)data, sizeof(Metadata_t));
        printf("Wrote metadata to slot %u\r\n", slot);
        return PROTOCOL_ACK_OK;
    }
    else if (code == FA_ERR_BUSY)
    {
        printf("Write service busy\r\n");
        return PROTOCOL_NACK_BUSY_REPEAT_REQUEST;
    }
    else
    {
        printf("Write service failed: %i\r\n", code);
        return PROTOCOL_NACK_REQUEST_FAILED;
    }
}

static uint8_t PutFragment(
    const uint8_t* data, 
    size_t size)
{
    printf("Received fragment %lX\r\n", InlineCrc32(data, size));

    if (size != sizeof(Fragment_t))
    {
        return PROTOCOL_NACK_REQUEST_OUT_OF_RANGE;
    }

    const Fragment_t* frag = (const Fragment_t*)data;

    int slot = -1;
    for (int i = 0; i < 3; i++)
    {
        if (frag->firmwareId == f_metadata[i].firmwareId)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        printf("No suitable slot to write fragment into!\r\n");
        return PROTOCOL_NACK_REQUEST_FAILED;
    }

    FA_ReturnCode_t code =  FA_WriteFragment(&f_fa[slot], frag->number, frag);

    if (code == FA_ERR_OK)
    {
        printf("Wrote fragment to slot %u.%lu\r\n", slot, frag->number);
        return PROTOCOL_ACK_OK;
    }
    else if (code == FA_ERR_BUSY)
    {
        printf("Write service busy\r\n");
        return PROTOCOL_NACK_BUSY_REPEAT_REQUEST;
    }
    else
    {
        printf("Write service failed: %i\r\n", code);
        return PROTOCOL_NACK_REQUEST_FAILED;
    }
}

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION DEFINITIONS                                                */
/*----------------------------------------------------------------------------*/

void SERVER_UdpUpdateServer(w25qxx_handle_t *arg)
{
    f_resetRequest = false;
    f_w25q128 = arg;
    REQUIRE(f_w25q128 != NULL);

    static const MemoryConfig_t memConf[4] = {
        // Fragment area 0
        {
            .baseAddress = 0x0,
            .sectorSize = W25Qxx_SECTOR_SIZE,
            .memorySize = UPDATE_SLOT_SIZE,
            .eraseValue = 0xFF,

            .Reader = ReadMemory,
            .Writer = WriteMemory,
            .Eraser = EraseSectors,
        },
        // Fragment area 1
        {
            .baseAddress = UPDATE_SLOT_SIZE,
            .sectorSize = W25Qxx_SECTOR_SIZE,
            .memorySize = UPDATE_SLOT_SIZE,
            .eraseValue = 0xFF,

            .Reader = ReadMemory,
            .Writer = WriteMemory,
            .Eraser = EraseSectors,
        },
        // Fragment area 2
        {
            .baseAddress = 2U * UPDATE_SLOT_SIZE,
            .sectorSize = W25Qxx_SECTOR_SIZE,
            .memorySize = UPDATE_SLOT_SIZE,
            .eraseValue = 0xFF,

            .Reader = ReadMemory,
            .Writer = WriteMemory,
            .Eraser = EraseSectors,
        },
        // Update command area
        {
            .baseAddress = 3U * UPDATE_SLOT_SIZE,
            .sectorSize = W25Qxx_SECTOR_SIZE,
            .memorySize = 3U * W25Qxx_SECTOR_SIZE,
            .eraseValue = 0xFF,

            .Reader = ReadMemory,
            .Writer = WriteMemory,
            .Eraser = EraseSectors,
        },
    };

    for (size_t i = 0; i < 3; i++)
    {
        REQUIRE(FA_ERR_OK == FA_InitStruct(&f_fa[i], &memConf[i], ValidateFragment, ValidateMetadata));
        (void)FA_ReadMetadata(&f_fa[i], &f_metadata[i]);
    }
    REQUIRE(CA_InitStruct(&f_ca, &memConf[3], &InlineCrc32));
    REQUIRE(US_InitServer(&f_us, ReadDataById, WriteDataById, PutMetadata, PutFragment));
    REQUIRE(TRANSFER_Init(&f_tb, &f_us, f_memBlock, sizeof(f_memBlock)));

    int sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    uint8_t packet[1472];
    int recvLen;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    REQUIRE(sock >= 0);

    SET_ADDRESS(server_addr, INADDR_ANY, UDP_PORT);
    int bindRes = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    REQUIRE_ELSE(bindRes >= 0, close(sock));

    printf("UDP update server listening on 192.168.1.50:%d\r\n", UDP_PORT);

    while (1) {
        recvLen = recvfrom(
            sock, 
            (char*)packet, 
            sizeof(packet), 
            0,
            (struct sockaddr *)&client_addr, 
            &client_addr_len
        );

        if (recvLen < 0) {
            perror("recvfrom failed");
            continue;
        }

        SERVER_NotifyCallback();

        const size_t resSize = TRANSFER_Process(&f_tb, packet, recvLen, sizeof(packet));

        sendto(
            sock, 
            packet, 
            resSize, 
            0,
            (struct sockaddr *)&client_addr, 
            client_addr_len
        );

        if (f_resetRequest)
        {
            break;
        }
    }

    close(sock);

    if (f_resetRequest)
    {
        printf("Executing reset request\r\n");
        TIM6_Delay_us(1000);
        system_reset_graceful();
    }
}

/* EoF udpserver.c */

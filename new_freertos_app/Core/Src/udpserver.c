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

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include "lwip/api.h"

#include "bigendian.h"
#include "crc32.h"
#include "metadata.h"
#include "server.h"

#include "fragmentstore/fragmentstore.h"
#include "updateserver/transfer.h"

/*----------------------------------------------------------------------------*/
/* PRIVATE TYPE DEFINITIONS                                                   */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* MACRO DEFINITIONS                                                          */
/*----------------------------------------------------------------------------*/

#define UDP_PORT 7

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


/*----------------------------------------------------------------------------*/
/* VARIABLE DEFINITIONS                                                       */
/*----------------------------------------------------------------------------*/

static UpdateServer_t f_us;
static TransferBuffer_t f_tb;
static uint8_t f_memBlock[5 * 1024];

/*----------------------------------------------------------------------------*/
/* PRIVATE FUNCTION DEFINITIONS                                               */
/*----------------------------------------------------------------------------*/

static uint8_t TEST_ReadDataById(
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

static uint8_t TEST_WriteDataById(
    uint8_t id, 
    const uint8_t* in, 
    size_t size)
{
    printf("Received data id %lu content ", (uint32_t)(id));
    for (size_t i = 0; i < size; i++)
    {
        printf("%lX ", (uint32_t)(in[i]));
    }
    printf("\r\n");

    return PROTOCOL_ACK_OK;
}

static uint8_t TEST_PutMetadata(
    const uint8_t* data, 
    size_t size)
{
    printf("Received metadata %lX\r\n", InlineCrc32(data, size));
    return PROTOCOL_ACK_OK;
}

static uint8_t TEST_PutFragment(
    const uint8_t* data, 
    size_t size)
{
    printf("Received fragment %lX\r\n", InlineCrc32(data, size));
    return PROTOCOL_ACK_OK;
}

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION DEFINITIONS                                                */
/*----------------------------------------------------------------------------*/

void SERVER_UdpUpdateServer(w25qxx_handle_t *arg)
{
    (void)arg;

    REQUIRE(US_InitServer(&f_us, TEST_ReadDataById, TEST_WriteDataById, TEST_PutMetadata, TEST_PutFragment));
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
    }

    close(sock);
}

/* EoF udpserver.c */

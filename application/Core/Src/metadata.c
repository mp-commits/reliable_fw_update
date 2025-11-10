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
 * metadata.c
 *
 * @brief Metadata of this firmware version. Signatures are generated after build
*/

/*----------------------------------------------------------------------------*/
/* INCLUDE DIRECTIVES                                                         */
/*----------------------------------------------------------------------------*/

#include "app_types.h"
#include "metadata.h"
#include "git_hash.h"
#include <assert.h>

/*----------------------------------------------------------------------------*/
/* MACRO DEFINITIONS                                                          */
/*----------------------------------------------------------------------------*/

#define REPEAT8(x) (x),(x),(x),(x),(x),(x),(x),(x)
#define REPEAT64(x) REPEAT8(x),\
                    REPEAT8(x),\
                    REPEAT8(x),\
                    REPEAT8(x),\
                    REPEAT8(x),\
                    REPEAT8(x),\
                    REPEAT8(x),\
                    REPEAT8(x)

/*----------------------------------------------------------------------------*/
/* VARIABLE DEFINITIONS                                                       */
/*----------------------------------------------------------------------------*/

extern const uint32_t ISR_VECTOR_START[];

const Metadata_t FIRMWARE_METADATA __attribute__((section (".metadata"))) = 
{
    .magic = "_M_E_T_A_D_A_T_A",
    .type = APP_TYPE_FIRMWARE,
    .version = 1U,
    .rollbackNumber = 0U,
    .firmwareId = GIT_HASH,
    .startAddress = (uint32_t)ISR_VECTOR_START,
    .firmwareSize = 0x00000000U,
    .name = "test_firmware_ver1\0\0\0\0\0\0\0\0\0\0\0\0\0",
    .firmwareSignature = {REPEAT64(0xA5)},
    .metadataSignature = {REPEAT64(0xDA)}
};

_Static_assert(sizeof(FIRMWARE_METADATA) <= 0x200U, "Metadata too large");

/* EoF metadata.c */

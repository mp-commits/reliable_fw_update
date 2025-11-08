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
 * no_init_ram.c
 *
 * @brief {Short description of the source file}
*/

/*----------------------------------------------------------------------------*/
/* INCLUDE DIRECTIVES                                                         */
/*----------------------------------------------------------------------------*/

#include "no_init_ram.h"
#include "crc32.h"
#include <string.h>

/*----------------------------------------------------------------------------*/
/* PRIVATE TYPE DEFINITIONS                                                   */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* MACRO DEFINITIONS                                                          */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* VARIABLE DEFINITIONS                                                       */
/*----------------------------------------------------------------------------*/

NoInitRamContent_t NO_INIT_RAM_content __attribute__((section (".no_init_ram")));

/*----------------------------------------------------------------------------*/
/* PRIVATE FUNCTION DEFINITIONS                                               */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION DEFINITIONS                                                */
/*----------------------------------------------------------------------------*/

void NO_INIT_RAM_Init(void)
{
    const uint8_t* mem = (const uint8_t*)&NO_INIT_RAM_content;
    const size_t size = sizeof(NO_INIT_RAM_content) - sizeof(uint32_t);
    const uint32_t actCrc = InlineCrc32(mem, size);

    if (NO_INIT_RAM_content.crc != actCrc)
    {
        memset(&NO_INIT_RAM_content, 0, sizeof(NO_INIT_RAM_content));
    }
}

void NO_INIT_RAM_SetMember(uint32_t* member, uint32_t value)
{
    const uint32_t* const begin = (const uint32_t*)&NO_INIT_RAM_content;
    const uint32_t* const end = &NO_INIT_RAM_content.crc;

    if (((uint32_t)member >= (uint32_t)begin) && ((uint32_t)member < (uint32_t)end))
    {
        *member = value;
        const uint8_t* mem = (const uint8_t*)&NO_INIT_RAM_content;
        const size_t size = sizeof(NO_INIT_RAM_content) - sizeof(uint32_t);
        NO_INIT_RAM_content.crc = InlineCrc32(mem, size);
    }
}

/* EoF no_init_ram.c */

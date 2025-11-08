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
 * no_init_ram.h
 *
 * @brief {Short description of the source file}
*/

#ifndef NO_INIT_RAM_H_
#define NO_INIT_RAM_H_

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------*/
/* INCLUDE DIRECTIVES                                                         */
/*----------------------------------------------------------------------------*/

#include <stdint.h>

/*----------------------------------------------------------------------------*/
/* PUBLIC TYPE DEFINITIONS                                                    */
/*----------------------------------------------------------------------------*/

typedef struct
{
    uint32_t resetCount;
    uint32_t appTag;
    uint32_t bootloaderTag;
    uint32_t resetArg;

    uint32_t reserved[11];

    uint32_t crc;
} NoInitRamContent_t;

/*----------------------------------------------------------------------------*/
/* PUBLIC MACRO DEFINITIONS                                                   */
/*----------------------------------------------------------------------------*/

#define APP_TAG_INVALID (0xDEADBEEFU)
#define APP_TAG_GOOD    (0x600DF00DU)

/*----------------------------------------------------------------------------*/
/* PUBLIC VARIABLE DEFINITIONS                                                */
/*----------------------------------------------------------------------------*/

extern NoInitRamContent_t NO_INIT_RAM_content;

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION DECLARATIONS                                               */
/*----------------------------------------------------------------------------*/


extern void NO_INIT_RAM_Init(void);

extern void NO_INIT_RAM_SetMember(uint32_t* member, uint32_t value);

#ifdef __cplusplus
} /* extern C */
#endif

/* EoF no_init_ram.h */

#endif /* NO_INIT_RAM_H_ */

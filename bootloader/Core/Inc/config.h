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
 * config.h
 *
 * @brief Bootloader build config
*/

#ifndef CONFIG_H_
#define CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------*/
/* INCLUDE DIRECTIVES                                                         */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* PUBLIC TYPE DEFINITIONS                                                    */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* PUBLIC MACRO DEFINITIONS                                                   */
/*----------------------------------------------------------------------------*/

#define ENABLE_RESCUE_PARTITION

#define APP_METADATA_ADDRESS        (0x08010000U)
#define FIRST_FLASH_ADDRESS         (APP_METADATA_ADDRESS + sizeof(Metadata_t))
#define LAST_FLASH_ADDRESS          (0x08200000U)

#ifdef ENABLE_RESCUE_PARTITION
// Rescue partition enabled address
#define RESCUE_METADATA_ADDRESS     (0x081C0000U)
#define RESCUE_DATA_BEGIN           (RESCUE_METADATA_ADDRESS + sizeof(Metadata_t))
#else
// Rescue partition disable address == app address
#define RESCUE_METADATA_ADDRESS     (0x08010000U)
#define RESCUE_DATA_BEGIN           (RESCUE_METADATA_ADDRESS + sizeof(Metadata_t))
#endif

/*----------------------------------------------------------------------------*/
/* PUBLIC VARIABLE DEFINITIONS                                                */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION DECLARATIONS                                               */
/*----------------------------------------------------------------------------*/


#ifdef __cplusplus
} /* extern C */
#endif

/* EoF config.h */

#endif /* CONFIG_H_ */

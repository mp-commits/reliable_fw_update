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
 * installer.h
 *
 * @brief {Short description of the source file}
*/

#ifndef INSTALLER_H_
#define INSTALLER_H_

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------*/
/* INCLUDE DIRECTIVES                                                         */
/*----------------------------------------------------------------------------*/

#include "driver_w25qxx.h"
#include "keys.h"
#include "fragmentstore/fragmentstore.h"
#include <stdbool.h>
#include <stdint.h>

/*----------------------------------------------------------------------------*/
/* PUBLIC TYPE DEFINITIONS                                                    */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* PUBLIC MACRO DEFINITIONS                                                   */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* PUBLIC VARIABLE DEFINITIONS                                                */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION DECLARATIONS                                               */
/*----------------------------------------------------------------------------*/

extern void INSTALLER_InitAreas(w25qxx_handle_t* w25q128, const KeyContainer_t* keys);

extern bool INSTALLER_CheckInstallRequest(void);

extern bool INSTALLER_TryRepair(void);

extern bool INSTALLER_TryInstallRescueApp(const Metadata_t** out);

#ifdef __cplusplus
} /* extern C */
#endif

/* EoF installer.h */

#endif /* INSTALLER_H_ */

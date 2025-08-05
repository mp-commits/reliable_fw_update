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
 * bigendian.h
 *
 * @brief Bigendian decode and encode functions
*/

#ifndef BIGENDIAN_H_
#define BIGENDIAN_H_

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------*/
/* INCLUDE DIRECTIVES                                                         */
/*----------------------------------------------------------------------------*/

#include <stdint.h>
#include <stddef.h>

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION DECLARATIONS                                               */
/*----------------------------------------------------------------------------*/

/** Encode 32-bit value in big endian byte order
 *
 * @return Bytes written to buf (4)
 */
extern size_t BE_PutU32(uint8_t* buf, uint32_t val);

#ifdef __cplusplus
} /* extern C */
#endif

/* EoF bigendian.h */

#endif /* BIGENDIAN_H_ */

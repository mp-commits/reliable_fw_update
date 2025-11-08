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
 * w25qxx_init.h
 *
 * @brief {Short description of the source file}
*/

#ifndef W25QXX_INIT_H_
#define W25QXX_INIT_H_

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------*/
/* INCLUDE DIRECTIVES                                                         */
/*----------------------------------------------------------------------------*/

#include "driver_w25qxx.h"
#include "stm32f4xx_hal.h"

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION DECLARATIONS                                               */
/*----------------------------------------------------------------------------*/

/** Initialize the W25Q128 chip to use a specific SPI port
 * 
 * @param hspi Initialized SPI handle
 * @param csPort SPI CS port
 * @param csPin  SPI CS pin
 * 
 * @return w25qxx_handle_t driver handle or NULL if failed
 */
extern w25qxx_handle_t* W25Q128_Init(SPI_HandleTypeDef* hspi, GPIO_TypeDef* csPort, uint16_t csPin);

#ifdef __cplusplus
} /* extern C */
#endif

/* EoF w25qxx_init.h */

#endif /* W25QXX_INIT_H_ */

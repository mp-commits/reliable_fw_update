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
 * w25qxx_init.c
 *
 * @brief {Short description of the source file}
*/

/*----------------------------------------------------------------------------*/
/* INCLUDE DIRECTIVES                                                         */
/*----------------------------------------------------------------------------*/

#include "w25qxx_init.h"
#include "stm32f4xx_it.h"

#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>

/*----------------------------------------------------------------------------*/
/* PRIVATE TYPE DEFINITIONS                                                   */
/*----------------------------------------------------------------------------*/

typedef struct
{
    w25qxx_handle_t w25q128;
    SPI_HandleTypeDef* hspi;
    GPIO_TypeDef* csPort;
    uint16_t csPin;
} ImplHandle_t;

/*----------------------------------------------------------------------------*/
/* MACRO DEFINITIONS                                                          */
/*----------------------------------------------------------------------------*/

#define ASSERT_0(stmt) \
if (0 != (stmt))\
{\
    printf("%s failed\r\n", #stmt);\
    return NULL;\
}

#define ASSERT_NOT_NULL(ptr) \
if (NULL == ptr)\
{\
    printf("%s is NULL\r\n", #ptr);\
    return NULL;\
}

/*----------------------------------------------------------------------------*/
/* VARIABLE DEFINITIONS                                                       */
/*----------------------------------------------------------------------------*/

static ImplHandle_t f_impl;

/*----------------------------------------------------------------------------*/
/* PRIVATE FUNCTION DEFINITIONS                                               */
/*----------------------------------------------------------------------------*/

uint8_t W25QXX_INT_SpiInit(void)
{
    /* SPI initialization occurs outside of this module! */
    return 0U;
}

uint8_t W25QXX_INT_SpiDeInit(void)
{
    /* SPI deinitialization occurs outside of this module! */
    return 0U;
}

uint8_t W25QXX_INT_SpiWriteRead(uint8_t instruction, uint8_t instruction_line,
                                uint32_t address, uint8_t address_line, uint8_t address_len,
                                uint32_t alternate, uint8_t alternate_line, uint8_t alternate_len,
                                uint8_t dummy, uint8_t *in_buf, uint32_t in_len,
                                uint8_t *out_buf, uint32_t out_len, uint8_t data_line)
{
    (void)instruction;
    (void)address;
    (void)address_len;
    (void)alternate;
    (void)alternate_len;

    if ((instruction_line != 0) || 
        (address_line != 0) || 
        (alternate_line != 0) || 
        (dummy != 0) || 
        (data_line != 1))
    {
        return 1;
    }
    
    HAL_GPIO_WritePin(f_impl.csPort, f_impl.csPin, GPIO_PIN_RESET);

    uint8_t res = 0;
    HAL_StatusTypeDef hal = HAL_OK;

    if (in_len > 0U)
    {
        hal = HAL_SPI_Transmit(f_impl.hspi, in_buf, in_len, 1000);

        if (hal != HAL_OK)
        {
            res = 1U;
        }
    }

    if ((res == 0U) && (out_len > 0U))
    {
        hal = HAL_SPI_Receive(f_impl.hspi, out_buf, out_len, 1000);

        if (hal != HAL_OK)
        {
            res = 1U;
        }
    }

    HAL_GPIO_WritePin(f_impl.csPort, f_impl.csPin, GPIO_PIN_SET);

    return res;
}

void W25QXX_INT_Delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        TIM6_Delay_us(1000U);
    }
}

void W25QXX_INT_Delay_us(uint32_t us)
{
    TIM6_Delay_us(us);
}

void W25QXX_INT_DebugPrint(const char* const fmt, ...)
{
    char str[256];
    uint16_t len;
    va_list args;
    
    memset((char *)str, 0, sizeof(char) * 256); 
    va_start(args, fmt);
    vsnprintf((char *)str, 255, (char const *)fmt, args);
    va_end(args);
    
    len = strlen((char *)str);

    if (len < (sizeof(str) - 2U))
    {
        str[len] = '\r';
        str[len] = '\n';
    }

    (void)printf(str);
}

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION DEFINITIONS                                                */
/*----------------------------------------------------------------------------*/

w25qxx_handle_t* W25Q128_Init(SPI_HandleTypeDef* hspi, GPIO_TypeDef* csPort, uint16_t csPin)
{
    memset(&f_impl, 0, sizeof(f_impl));

    ASSERT_NOT_NULL(hspi);
    ASSERT_NOT_NULL(csPort);

    f_impl.hspi = hspi;
    f_impl.csPort = csPort;
    f_impl.csPin = csPin;

    DRIVER_W25QXX_LINK_INIT(&f_impl.w25q128, w25qxx_handle_t);
    DRIVER_W25QXX_LINK_SPI_QSPI_INIT(&f_impl.w25q128, W25QXX_INT_SpiInit);
    DRIVER_W25QXX_LINK_SPI_QSPI_DEINIT(&f_impl.w25q128, W25QXX_INT_SpiDeInit);
    DRIVER_W25QXX_LINK_SPI_QSPI_WRITE_READ(&f_impl.w25q128, W25QXX_INT_SpiWriteRead);
    DRIVER_W25QXX_LINK_DELAY_MS(&f_impl.w25q128, W25QXX_INT_Delay_ms);
    DRIVER_W25QXX_LINK_DELAY_US(&f_impl.w25q128, W25QXX_INT_Delay_us);
    DRIVER_W25QXX_LINK_DEBUG_PRINT(&f_impl.w25q128, W25QXX_INT_DebugPrint);

    ASSERT_0(w25qxx_set_type(&f_impl.w25q128, W25Q128));
    ASSERT_0(w25qxx_set_interface(&f_impl.w25q128, W25QXX_INTERFACE_SPI));
    ASSERT_0(w25qxx_set_dual_quad_spi(&f_impl.w25q128, W25QXX_BOOL_FALSE));
    ASSERT_0(w25qxx_init(&f_impl.w25q128));

    return &f_impl.w25q128;
}

/* EoF w25qxx_init.c */

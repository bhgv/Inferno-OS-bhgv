/*
 * Copyright (C) 2015 - 2018, IBEROXARXA SERVICIOS INTEGRALES, S.L.
 * Copyright (C) 2015 - 2018, Jaume Olivé Petrus (jolive@whitecatboard.org)
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *     * The WHITECAT logotype cannot be changed, you can remove it, but you
 *       cannot change it in any way. The WHITECAT logotype is:
 *
 *          /\       /\
 *         /  \_____/  \
 *        /_____________\
 *        W H I T E C A T
 *
 *     * Redistributions in binary form must retain all copyright notices printed
 *       to any local or remote output device. This include any reference to
 *       Lua RTOS, whitecatboard.org, Lua, and other copyright notices that may
 *       appear in the future.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Lua RTOS, spiffs port functions
 *
 */

#include "k210_spiffs.h"
//#include "esp_attr.h"
#include "spiffs.h"
#include "../hal/w25qxx.h"

#include <stdlib.h>

#include "../sys/mutex.h"

#include <devices.h>
//#include <task.h>


#define SPI_FLASH_ALIGN 0




static handle_t spi3 = 0;


s32_t k210_spi_flash_init() {
    spi3 = io_open("/dev/spi3");
    if (!spi3) {
        return 0;
    }
    w25qxx_init(spi3);

    return 1;
}

void spiffs_lock(spiffs *fs) {
    mtx_lock(fs->user_data);
}

void spiffs_unlock(spiffs *fs) {
    mtx_unlock(fs->user_data);
}

s32_t k210_spi_flash_read(u32_t addr, u32_t size, u8_t *dst) {
#if SPI_FLASH_ALIGN
    u32_t aaddr;
    u8_t *buff = NULL;
    u8_t *abuff = NULL;
    u32_t asize;

    asize = size;

    // Align address to 4 byte
    aaddr = (addr + (4 - 1)) & (u32_t) -4;
    if (aaddr != addr) {
        aaddr -= 4;
        asize += (addr - aaddr);
    }

    // Align size to 4 byte
    asize = (asize + (4 - 1)) & (u32_t) -4;

    if ((aaddr != addr) || (asize != size)) {
        // Align buffer
        buff = malloc(asize + 4);
        if (!buff) {
            return SPIFFS_ERR_INTERNAL;
        }

        abuff = (u8_t *) (((ptrdiff_t) buff + (4 - 1)) & (u32_t) -4);

        if (w25qxx_read_data(aaddr, (void *) abuff, asize) != W25QXX_OK) {
            free(buff);
            return SPIFFS_ERR_INTERNAL;
        }

        memcpy(dst, abuff + (addr - aaddr), size);

        free(buff);
    } else {
#endif
        if (w25qxx_read_data(addr, (void *) dst, size) != W25QXX_OK) {
            return SPIFFS_ERR_INTERNAL;
        }
#if SPI_FLASH_ALIGN
    }
#endif
    return SPIFFS_OK;
}

s32_t k210_spi_flash_write(u32_t addr, u32_t size, const u8_t *src) {
#if SPI_FLASH_ALIGN
    u32_t aaddr;
    u8_t *buff = NULL;
    u8_t *abuff = NULL;
    u32_t asize;

    asize = size;

    // Align address to 4 byte
    aaddr = (addr + (4 - 1)) & -4;
    if (aaddr != addr) {
        aaddr -= 4;
        asize += (addr - aaddr);
    }

    // Align size to 4 byte
    asize = (asize + (4 - 1)) & -4;

    if ((aaddr != addr) || (asize != size)) {
        // Align buffer
        buff = malloc(asize + 4);
        if (!buff) {
            return SPIFFS_ERR_INTERNAL;
        }

        abuff = (u8_t *) (((ptrdiff_t) buff + (4 - 1)) & -4);

        if (w25qxx_read_data(aaddr, (void *) abuff, asize) != W25QXX_OK) {
            free(buff);
            return SPIFFS_ERR_INTERNAL;
        }

        memcpy(abuff + (addr - aaddr), src, size);

        if (w25qxx_write_data(aaddr, (uint8_t *) abuff, asize) != W25QXX_OK) {
            free(buff);
            return SPIFFS_ERR_INTERNAL;
        }

        free(buff);
    } else {
#endif
        if (w25qxx_write_data(addr, (uint8_t *) src, size) != W25QXX_OK) {
            return SPIFFS_ERR_INTERNAL;
        }
#if SPI_FLASH_ALIGN
    }
#endif
    return SPIFFS_OK;
}

s32_t k210_spi_flash_erase(u32_t addr, u32_t size) {
    if (w25qxx_sector_erase(addr >> 12) != W25QXX_OK) {
        return SPIFFS_ERR_INTERNAL;
    }

    return SPIFFS_OK;
}

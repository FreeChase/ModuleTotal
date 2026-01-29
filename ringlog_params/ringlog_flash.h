/*
 * config.h
 *
 *  Created on: 2025年2月18日
 *      Author: admin
 */

#ifndef _RINGLOG_FLASH_H_
#define _RINGLOG_FLASH_H_

#include "stdint.h"

#ifdef __cplusplus 
extern "C" {
#endif

/* ================= 配置 ================= */
#define FLASH_BASE_ADDR     (0x7E0000)	//* FLASH total 0x800000 
#define FLASH_SIZE          (32 * 1024)   
#define SECTOR_SIZE         (4 * 1024)    
#define UNIT_SIZE           (4 * 1024)    

#define SECTOR_NUM          (FLASH_SIZE / SECTOR_SIZE)
#define UNITS_PER_SECTOR    (SECTOR_SIZE / UNIT_SIZE)
#define TOTAL_UNITS         (FLASH_SIZE / UNIT_SIZE)

#define CFG_MAGIC           0x43464721u 

/* ================= 数据结构 ================= */
#pragma pack(4)
typedef struct {
    uint32_t magic;
    uint32_t length;
    uint32_t checksum;
} cfg_hdr_t;

typedef struct {
    uint32_t write_unit;
} ring_log_t;
#pragma pack()



int ringlog_flash_init(void);

int ringlog_flash_read(void *out_buf, uint32_t buf_size, uint32_t *out_len);

int ringlog_flash_write(void *input_buf, uint32_t buf_size);
#ifdef __cplusplus 
}
#endif

#endif /* TEST__CONFIG_CONFIG_H_ */

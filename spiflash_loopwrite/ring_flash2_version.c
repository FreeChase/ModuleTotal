#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ================= 配置 ================= */
#define FLASH_SIZE          (16 * 1024)   
#define SECTOR_SIZE         (4 * 1024)    
#define UNIT_SIZE           (1 * 1024)    

#define CFG_MAGIC           0x43464721u 
#define CURRENT_CFG_VER     2  /* 当前程序的配置版本号 */

/* ================= 数据结构 ================= */

// 增加 version 字段以支持平滑升级
typedef struct {
    uint32_t magic;
    uint16_t version;   /* 配置版本号 */
    uint16_t length;    /* Payload 长度 */
    uint32_t checksum;  /* Payload 的 XOR 校验 */
} __attribute__((packed)) cfg_hdr_t;

typedef struct {
    uint32_t write_unit;
} ring_log_t;

/* ================= 模拟应用层数据结构 ================= */

// 旧版本结构体
typedef struct {
    int volume;
} cfg_v1_t;

// 新版本结构体 (增加了 brightness)
typedef struct {
    int volume;
    int brightness; 
} cfg_v2_t;

/* ================= Flash Mock (底层驱动) ================= */
static uint8_t g_flash[FLASH_SIZE];

static void spiflash_read(uint32_t addr, void *buf, uint32_t len) {
    memcpy(buf, &g_flash[addr], len);
}

static void spiflash_write(uint32_t addr, const void *buf, uint32_t len) {
    memcpy(&g_flash[addr], buf, len);
}

static void spiflash_erase_sector(uint32_t sector) {
    memset(&g_flash[sector * SECTOR_SIZE], 0xFF, SECTOR_SIZE);
}

/* ================= 工具函数 ================= */
static inline uint32_t unit_addr(uint32_t unit) {
    return unit * UNIT_SIZE;
}

static uint32_t calculate_checksum(const uint8_t *data, uint32_t len) {
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < len; i++) checksum ^= data[i];
    return checksum;
}

static int sector_is_ff(uint32_t sector) {
    uint8_t val;
    uint32_t addr = sector * SECTOR_SIZE;
    for (uint32_t i = 0; i < SECTOR_SIZE; i++) {
        spiflash_read(addr + i, &val, 1);
        if (val != 0xFF) return 0;
    }
    return 1;
}

static int unit_is_valid(uint32_t unit, cfg_hdr_t *hdr) {
    spiflash_read(unit_addr(unit), hdr, sizeof(cfg_hdr_t));
    if (hdr->magic != CFG_MAGIC) return 0;
    if (hdr->length == 0 || hdr->length > UNIT_SIZE - sizeof(cfg_hdr_t)) return 0;

    uint8_t buf[UNIT_SIZE];
    spiflash_read(unit_addr(unit) + sizeof(cfg_hdr_t), buf, hdr->length);
    return (calculate_checksum(buf, hdr->length) == hdr->checksum);
}

/* ================= 核心 API ================= */

void ring_log_init(ring_log_t *l) {
    for (uint32_t s = 0; s < (FLASH_SIZE / SECTOR_SIZE); s++) {
        if (sector_is_ff(s)) {
            l->write_unit = s * (SECTOR_SIZE / UNIT_SIZE);
            return;
        }
    }
    spiflash_erase_sector(0);
    l->write_unit = 0;
}

int ring_cfg_write(ring_log_t *l, uint16_t version, const void *cfg, uint32_t len) {
    if (len > UNIT_SIZE - sizeof(cfg_hdr_t)) return -1;

    if ((l->write_unit % (SECTOR_SIZE / UNIT_SIZE)) == 0) {
        uint32_t next_s = (l->write_unit / (SECTOR_SIZE / UNIT_SIZE) + 1) % (FLASH_SIZE / SECTOR_SIZE);
        spiflash_erase_sector(next_s);
    }

    uint32_t addr = unit_addr(l->write_unit);
    spiflash_write(addr + sizeof(cfg_hdr_t), cfg, len);

    cfg_hdr_t hdr = {
        .magic = CFG_MAGIC,
        .version = version, // 写入当前版本号
        .length = (uint16_t)len,
        .checksum = calculate_checksum((const uint8_t *)cfg, len)
    };
    spiflash_write(addr, &hdr, sizeof(hdr));

    l->write_unit = (l->write_unit + 1) % (FLASH_SIZE / UNIT_SIZE);
    return 0;
}

/**
 * 增强版读取：增加 out_version 参数
 */
int ring_cfg_read_latest(ring_log_t *l, void *out_buf, uint32_t buf_size, uint32_t *out_len, uint16_t *out_version) {
    uint32_t total_units = FLASH_SIZE / UNIT_SIZE;
    for (uint32_t i = 1; i <= total_units; i++) {
        uint32_t curr_u = (l->write_unit + total_units - i) % total_units;
        cfg_hdr_t hdr;
        if (unit_is_valid(curr_u, &hdr)) {
            if (hdr.length > buf_size) return -2;
            spiflash_read(unit_addr(curr_u) + sizeof(cfg_hdr_t), out_buf, hdr.length);
            *out_len = hdr.length;
            *out_version = hdr.version; // 返回读到的版本号
            return 0;
        }
    }
    return -1;
}

/* ================= 测试：模拟升级逻辑 ================= */

int main(void) {
    memset(g_flash, 0xFF, sizeof(g_flash));
    ring_log_t log;
    ring_log_init(&log);

    // 1. 模拟旧程序写入 V1 版本的配置
    cfg_v1_t v1_data = { .volume = 50 };
    printf("Old system saves V1 config (volume: 50)\n");
    ring_cfg_write(&log, 1, &v1_data, sizeof(v1_data));

    // 2. 模拟新程序启动
    printf("--- System Reboot with New Firmware (V2) ---\n");
    cfg_v2_t my_config;
    uint32_t read_len;
    uint16_t read_ver;
    uint8_t temp_buf[256];

    if (ring_cfg_read_latest(&log, temp_buf, sizeof(temp_buf), &read_len, &read_ver) == 0) {
        if (read_ver == 1) {
            // 执行数据迁移 (Migration)
            printf("Migration: V1 -> V2 detected.\n");
            cfg_v1_t *old = (cfg_v1_t *)temp_buf;
            my_config.volume = old->volume;
            my_config.brightness = 100; // 赋予 V2 新字段默认值
        } 
        else if (read_ver == 2) {
            memcpy(&my_config, temp_buf, sizeof(cfg_v2_t));
        }
        
        printf("Current Config: Volume=%d, Brightness=%d\n", my_config.volume, my_config.brightness);
        
        // 可选：升级后立即保存一次 V2 版本，下次启动就直接走 read_ver == 2 分支
        ring_cfg_write(&log, CURRENT_CFG_VER, &my_config, sizeof(my_config));
    }

    return 0;
}
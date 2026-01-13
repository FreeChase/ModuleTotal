#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ================= 配置 ================= */
#define FLASH_SIZE          (16 * 1024)   
#define SECTOR_SIZE         (4 * 1024)    
#define UNIT_SIZE           (1 * 1024)    

#define SECTOR_NUM          (FLASH_SIZE / SECTOR_SIZE)
#define UNITS_PER_SECTOR    (SECTOR_SIZE / UNIT_SIZE)
#define TOTAL_UNITS         (FLASH_SIZE / UNIT_SIZE)

#define CFG_MAGIC           0x43464721u 

/* ================= 数据结构 ================= */
typedef struct {
    uint32_t magic;
    uint32_t length;
    uint32_t checksum;  /* Payload 的 XOR 校验 */
} __attribute__((packed)) cfg_hdr_t;

typedef struct {
    uint32_t write_unit;   /* 下一次写入的单元索引 */
} ring_log_t;

/* ================= Flash Mock (实际开发时替换为底层驱动) ================= */
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

/* 校验某个单元的数据是否完整 */
static int unit_is_valid(uint32_t unit, cfg_hdr_t *hdr) {
    spiflash_read(unit_addr(unit), hdr, sizeof(cfg_hdr_t));
    if (hdr->magic != CFG_MAGIC) return 0;
    if (hdr->length == 0 || hdr->length > UNIT_SIZE - sizeof(cfg_hdr_t)) return 0;

    uint8_t buf[UNIT_SIZE];
    spiflash_read(unit_addr(unit) + sizeof(cfg_hdr_t), buf, hdr->length);
    return (calculate_checksum(buf, hdr->length) == hdr->checksum);
}

/* ================= 核心 API (简化版) ================= */

/**
 * 初始化：按照你的要求，找到第一个全空扇区，直接作为写起点
 */
void ring_log_init(ring_log_t *l) {
    for (uint32_t s = 0; s < SECTOR_NUM; s++) {
        if (sector_is_ff(s)) {
            l->write_unit = s * UNITS_PER_SECTOR; // 规则：上电第一次写一定是 unit0
            return;
        }
    }
    // 兜底：如果没找到空扇区（不符合不变式），强制擦除第0扇区
    spiflash_erase_sector(0);
    l->write_unit = 0;
}

/**
 * 写入配置：
 * 1. 如果在扇区开头，预擦除下一个扇区作为 Guard
 * 2. 先写数据，最后写 Header (Commit)
 */
int ring_cfg_write(ring_log_t *l, const void *cfg, uint32_t len) {
    if (len > UNIT_SIZE - sizeof(cfg_hdr_t)) return -1;

    // 维持不变式：如果在扇区起始，擦除下一个扇区
    if ((l->write_unit % UNITS_PER_SECTOR) == 0) {
        uint32_t next_s = (l->write_unit / UNITS_PER_SECTOR + 1) % SECTOR_NUM;
        spiflash_erase_sector(next_s);
    }

    uint32_t addr = unit_addr(l->write_unit);
    // 写入 Payload
    spiflash_write(addr + sizeof(cfg_hdr_t), cfg, len);
    // 写入 Header
    cfg_hdr_t hdr = {
        .magic = CFG_MAGIC,
        .length = len,
        .checksum = calculate_checksum((const uint8_t *)cfg, len)
    };
    spiflash_write(addr, &hdr, sizeof(hdr));

    l->write_unit = (l->write_unit + 1) % TOTAL_UNITS;
    return 0;
}

/**
 * 读取最新配置：
 * 从当前 write_unit 往前逆序搜索第一个合法的配置
 */
int ring_cfg_read_latest(ring_log_t *l, void *out_buf, uint32_t buf_size, uint32_t *out_len) {
    for (uint32_t i = 1; i <= TOTAL_UNITS; i++) {
        uint32_t curr_u = (l->write_unit + TOTAL_UNITS - i) % TOTAL_UNITS;
        cfg_hdr_t hdr;
        if (unit_is_valid(curr_u, &hdr)) {
            if (hdr.length > buf_size) return -2;
            spiflash_read(unit_addr(curr_u) + sizeof(cfg_hdr_t), out_buf, hdr.length);
            *out_len = hdr.length;
            return 0;
        }
    }
    return -1;
}

/* ================= 测试 ================= */
int main(void) {
    memset(g_flash, 0xFF, sizeof(g_flash));
    ring_log_t log;

    // 1. 初始化并模拟一些写入
    ring_log_init(&log);
    int data = 100;
    ring_cfg_write(&log, &data, sizeof(data));
    data = 200;
    ring_cfg_write(&log, &data, sizeof(data));

    // 2. 模拟掉电重启：逻辑会找到第一个空扇区（Sector 1）
    ring_log_t log_reboot;
    ring_log_init(&log_reboot);
    
    printf("Write unit after reboot: %u (Expected: 4, which is Sector 1)\n", log_reboot.write_unit);

    // 3. 读取最新
    int read_val;
    uint32_t read_len;
    if (ring_cfg_read_latest(&log_reboot, &read_val, sizeof(read_val), &read_len) == 0) {
        printf("Latest data: %d\n", read_val);
    }

    return 0;
}
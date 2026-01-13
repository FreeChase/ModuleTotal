/*
 * ring_flash_log_opt.c
 *
 * 优化点：
 * 1. 移除 ring_log_t 中的 ff_sector 变量，状态全由 write_unit 推导。
 * 2. 优化 sector_is_ff 的栈占用，避免大数组分配。
 * 3. 代码紧凑化。
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ================= 配置区 ================= */

#define FLASH_SIZE          (16 * 1024)
#define SECTOR_SIZE         (4 * 1024)
#define UNIT_SIZE           (1 * 1024)

#define SECTOR_NUM          (FLASH_SIZE / SECTOR_SIZE)
#define UNITS_PER_SECTOR    (SECTOR_SIZE / UNIT_SIZE)
#define TOTAL_UNITS         (FLASH_SIZE / UNIT_SIZE)

/* ================= Flash 底层 mock ================= */

static uint8_t g_flash[FLASH_SIZE];

static void spiflash_read(uint32_t addr, void *buf, uint32_t len)
{
    memcpy(buf, &g_flash[addr], len);
}

static void spiflash_write(uint32_t addr, const void *buf, uint32_t len)
{
    memcpy(&g_flash[addr], buf, len);
}

static void spiflash_erase_sector(uint32_t sector)
{
    memset(&g_flash[sector * SECTOR_SIZE], 0xFF, SECTOR_SIZE);
}

/* ================= ring-glog 极简实现 ================= */

typedef struct {
    uint32_t write_unit;   /* 唯一真实状态 */
} ring_log_t;

ring_log_t glog;

/* 检查某 Sector 是否全为 0xFF */
static int sector_is_ff(uint32_t sector)
{
    /* 优化：使用小 buffer 检查，避免嵌入式栈溢出 (原代码用了1KB栈) */
    uint8_t buf[32]; 
    uint32_t addr_start = sector * SECTOR_SIZE;
    uint32_t addr_end   = addr_start + SECTOR_SIZE;

    for (uint32_t addr = addr_start; addr < addr_end; addr += sizeof(buf)) {
        spiflash_read(addr, buf, sizeof(buf));
        for (uint32_t i = 0; i < sizeof(buf); i++) {
            if (buf[i] != 0xFF) return 0;
        }
    }
    return 1;
}

/* ---------- init / recover ---------- */

void ring_log_init(ring_log_t *l)
{
    /* 扫描找到第一个 FF sector，将其起始位置作为写指针 */
    for (uint32_t s = 0; s < SECTOR_NUM; s++) {
        if (sector_is_ff(s)) {
            l->write_unit = s * UNITS_PER_SECTOR;
            return;
        }
    }
    l->write_unit = 0; /* 兜底 */
}

/* ---------- append ---------- */

int ring_log_append(ring_log_t *l, const void *data)
{
    /* 核心逻辑：如果当前由 sector 头部开始写，说明该 sector 已被视为 FF，
       为了维持环形特性，必须立刻擦除 下一个 sector */
    if ((l->write_unit % UNITS_PER_SECTOR) == 0) {
        uint32_t cur_sector = l->write_unit / UNITS_PER_SECTOR;
        uint32_t next_sector = (cur_sector + 1) % SECTOR_NUM;
        
        spiflash_erase_sector(next_sector);
    }

    spiflash_write(l->write_unit * UNIT_SIZE, data, UNIT_SIZE);

    /* 环形递增 */
    l->write_unit = (l->write_unit + 1) % TOTAL_UNITS;

    return 0;
}

/* ================= 测试代码 ================= */

static void dump_flash(void)
{
    printf("Flash state:\n");
    for (uint32_t s = 0; s < SECTOR_NUM; s++) {
        printf(" Sector %u : %s\n", s, sector_is_ff(s) ? "ALL FF" : "DATA");
    }
    printf("glog.write_unit: %u\n", glog.write_unit);
    /* ff_sector 变量已移除，无需打印 */
}

int main(void)
{
    uint8_t buf[UNIT_SIZE];
    
    /* 上电初始化 */
    memset(g_flash, 0xFF, sizeof(g_flash));
    printf("=== First boot ===\n");
    ring_log_init(&glog);
    
    printf("=== Write Before ===\n");
    dump_flash();

    /* 写入测试 */
    for (int i = 0; i < 6; i++) {
        memset(buf, i, sizeof(buf));
        ring_log_append(&glog, buf);
    }
    
    printf("=== Write After ===\n");
    dump_flash();

    /* 模拟掉电重启 */
    printf("\n=== Power cycle ===\n");
    ring_log_init(&glog); // 重新初始化，模拟从 Flash 恢复状态

    for (int i = 0; i < 12; i++) {
        memset(buf, i, sizeof(buf));
        ring_log_append(&glog, buf);
    }

    dump_flash();

    return 0;
}
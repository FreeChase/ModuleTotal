#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ================= Flash 参数 ================= */

#define SECTOR_NUM          4
#define SECTOR_SIZE         4096
#define BLOCK_SIZE          1024
#define BLOCK_PER_SECTOR    (SECTOR_SIZE / BLOCK_SIZE)

#define FLASH_BASE_ADDR     0x00000000
#define FLASH_TOTAL_SIZE    (SECTOR_NUM * SECTOR_SIZE)

/* ================= SPI Flash 模拟 ================= */

static uint8_t flash_mem[FLASH_TOTAL_SIZE];

int spiflash_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    memcpy(buf, &flash_mem[addr], len);
    return 0;
}

int spiflash_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    /* 模拟 Flash：只能 1->0 */
    for (uint32_t i = 0; i < len; i++) {
        flash_mem[addr + i] &= buf[i];
    }
    return 0;
}

int spiflash_erase_sector(uint32_t addr)
{
    memset(&flash_mem[addr], 0xFF, SECTOR_SIZE);
    return 0;
}

/* ================= Flash 管理器 ================= */

typedef struct {
    uint8_t cur_sector;   /* 当前写 sector */
    uint8_t cur_block;    /* sector 内 block */
    uint8_t ff_sector;    /* 全 0xFF 的 sector */
} flash_mgr_t;

static flash_mgr_t g_FlashManager;

/* ================= 工具函数 ================= */

static inline uint32_t sector_base(uint8_t s)
{
    return FLASH_BASE_ADDR + s * SECTOR_SIZE;
}

static inline uint32_t block_addr(uint8_t s, uint8_t b)
{
    return sector_base(s) + b * BLOCK_SIZE;
}

static bool sector_is_ff(uint8_t s)
{
    uint8_t buf[32];
    uint32_t addr = sector_base(s);

    for (uint32_t off = 0; off < SECTOR_SIZE; off += sizeof(buf)) {
        spiflash_read(addr + off, buf, sizeof(buf));
        for (int i = 0; i < sizeof(buf); i++) {
            if (buf[i] != 0xFF)
                return false;
        }
    }
    return true;
}

/* ================= 初始化（上电扫描） ================= */

void flash_mgr_init(void)
{
    int ff = -1;

    for (int s = 0; s < SECTOR_NUM; s++) {
        if (sector_is_ff(s)) {
            ff = s;
            break;
        }
    }

    /* 全新 Flash：全部是 FF */
    if (ff < 0) {
        g_FlashManager.ff_sector  = 0;
        g_FlashManager.cur_sector = SECTOR_NUM - 1;
        g_FlashManager.cur_block  = BLOCK_PER_SECTOR;
        return;
    }

    g_FlashManager.ff_sector  = ff;
    g_FlashManager.cur_sector = (ff + SECTOR_NUM - 1) % SECTOR_NUM;

    /* 找到当前 sector 中第一个 FF block */
    g_FlashManager.cur_block = 0;
    for (int b = 0; b < BLOCK_PER_SECTOR; b++) {
        uint8_t tmp[8];
        spiflash_read(block_addr(g_FlashManager.cur_sector, b), tmp, sizeof(tmp));
        if (tmp[0] == 0xFF) {
            g_FlashManager.cur_block = b;
            return;
        }
    }

    /* sector 已写满 */
    g_FlashManager.cur_block = BLOCK_PER_SECTOR;
}

/* ================= 写 1KB ================= */

int flash_mgr_write(const uint8_t *data)
{
    if (g_FlashManager.cur_block >= BLOCK_PER_SECTOR) {

        /* 切换到 FF sector */
        g_FlashManager.cur_sector = g_FlashManager.ff_sector;
        g_FlashManager.cur_block  = 0;

        /* 预擦下一个 sector */
        g_FlashManager.ff_sector = (g_FlashManager.cur_sector + 1) % SECTOR_NUM;
        spiflash_erase_sector(sector_base(g_FlashManager.ff_sector));
    }

    uint32_t addr = block_addr(g_FlashManager.cur_sector, g_FlashManager.cur_block);
    spiflash_write(addr, data, BLOCK_SIZE);

    g_FlashManager.cur_block++;
    return 0;
}

/* ================= 读最新 1KB ================= */

int flash_mgr_read_latest(uint8_t *out)
{
    int s = g_FlashManager.cur_sector;
    int b = g_FlashManager.cur_block - 1;

    if (b < 0) {
        s = (s + SECTOR_NUM - 1) % SECTOR_NUM;
        b = BLOCK_PER_SECTOR - 1;
    }

    spiflash_read(block_addr(s, b), out, BLOCK_SIZE);
    return BLOCK_SIZE;
}

/* ================= 测试样例 ================= */

static void dump_state(const char *tag)
{
    printf("[%s] cur_sector=%d cur_block=%d ff_sector=%d\n",
           tag, g_FlashManager.cur_sector, g_FlashManager.cur_block, g_FlashManager.ff_sector);
}

int main(void)
{
    /* 模拟上电：Flash 全擦 */
    memset(flash_mem, 0xFF, sizeof(flash_mem));

    printf("Flash init...\n");
    flash_mgr_init();
    dump_state("INIT");

    uint8_t wbuf[BLOCK_SIZE];
    uint8_t rbuf[BLOCK_SIZE];

    /* 连续写 10 次 */
    for (int i = 0; i < 20; i++) {
        memset(wbuf, i, sizeof(wbuf));
        flash_mgr_write(wbuf);
        dump_state("WRITE");
    }

    /* 读最新 */
    flash_mgr_read_latest(rbuf);
    printf("Latest block first byte = 0x%02X \n", rbuf[0]);

    /* 模拟掉电重启 */
    printf("\nSimulate reboot...\n");
    flash_mgr_init();
    dump_state("REINIT");

    flash_mgr_read_latest(rbuf);
    printf("After reboot latest byte = 0x%02X \n", rbuf[0]);

    return 0;
}

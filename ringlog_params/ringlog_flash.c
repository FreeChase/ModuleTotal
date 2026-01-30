#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "ringlog_flash.h"
#include "cdl_config.h"
#include "cdl_qspi_nor.h"

static ring_log_t glog;




/* ================= Flash Mock (底层驱动模拟) ================= */
static uint8_t g_flash[FLASH_SIZE];

static void spiflash_read(uint32_t addr, void *buf, uint32_t len) {
    uint32_t base;
    uint32_t u32RemainCount =  len;
    uint32_t u32CurReadCount =  0;
    uint32_t u32AlreadyReadCount =  0;
    uint8_t * pu8buf = (uint8_t*)buf;
    int ret;
    base = QSPI_MEM_BASE;
    //1. 判断地址是否256对齐
    if( addr % 256 == 0)
    {
        //*loop read 
        while (u32RemainCount)
        {
            if(u32RemainCount >= 256)
            {
                u32CurReadCount = 256;
            }
            else
            {   
                u32CurReadCount = u32RemainCount;
            }

            ret = qspi_nor_page_read(base + addr + u32AlreadyReadCount, pu8buf, u32CurReadCount);
            if(ret != 0 )
            {
                info("[Flash] Read Error Ret %d!!!\n", ret);
            }
            //* params modfiy
            u32AlreadyReadCount += u32CurReadCount;
            u32RemainCount -= u32CurReadCount;

        }

    }
    else
    {
        //TODO complete corner case
    }
    // memcpy(buf, &g_flash[addr], len);
}

static void spiflash_write(uint32_t addr, const void *buf, uint32_t len) {
    uint32_t pageCount = 0 ;
    uint32_t base;
    uint32_t u32RemainCount =  len;
    uint32_t u32CurWriteCount =  0;
    uint32_t u32AlreadyWriteCount =  0;
    uint8_t * pu8buf = (uint8_t*)buf;
    int ret;
    base = QSPI_MEM_BASE;

    // int qspi_nor_page_write(uint32_t addr, uint8_t *p_data, uint32_t size)

    //1. 判断地址是否256对齐
    if( addr % 256 == 0)
    {
        //*loop write 
        while (u32RemainCount)
        {
            if(u32RemainCount >= 256)
            {
                u32CurWriteCount = 256;
            }
            else
            {   
                u32CurWriteCount = u32RemainCount;
            }

            ret = qspi_nor_page_write(base + addr + u32AlreadyWriteCount, pu8buf + u32AlreadyWriteCount, u32CurWriteCount);
            if(ret != 0 )
            {
                info("[Flash] Page Write Error Ret %d!!!\n", ret);
            }
            //* params modfiy
            u32AlreadyWriteCount += u32CurWriteCount;
            u32RemainCount -= u32CurWriteCount;


        }
        
    }
    else
    {
        //TODO haven't 256 byte align
        //* need complete logic 
    }


}

static void spiflash_erase_sector(uint32_t addr) {
    uint32_t base;
    int ret;
    base = QSPI_MEM_BASE;
    info("[Flash] Erasing Sector Addr %u...\n", addr);
    // memset(&g_flash[addr], 0xFF, SECTOR_SIZE);
    if(addr%SECTOR_SIZE == 0)
    {
        ret = qspi_nor_sec_erase(addr + base);
        if(ret != 0 )
        {
            info("[Flash] Erasing Error Ret %d!!!\n", ret);
        }

    }
}

/* ================= 工具函数 ================= */
static inline uint32_t unit_addr(uint32_t unit) {
    return unit * UNIT_SIZE + FLASH_BASE_ADDR;
}

static uint32_t calculate_checksum(const uint8_t *data, uint32_t len) {
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < len; i++) checksum ^= data[i];
    return checksum;
}

/**
 * 优化版：仅检查扇区前 64 字节是否为全 0xFF
 * 这样可以大幅缩短初始化时扫描 Flash 的时间
 */
static int sector_is_ff(uint32_t addr) {
    uint8_t buf[64]; 
    // uint32_t addr = sector * SECTOR_SIZE;

    // 一次性读出前 64 字节，减少驱动调用开销
    spiflash_read(addr, buf, sizeof(buf));

    for (uint32_t i = 0; i < sizeof(buf); i++) {
        if (buf[i] != 0xFF) {
            return 0; // 只要有任何一字节不是 0xFF，即判定为脏
        }
    }
    return 1;
}

static int unit_is_valid(uint32_t unit, uint8_t * pu8Buf) {
    // uint8_t buf[UNIT_SIZE];
    cfg_hdr_t * phdr = NULL;
    spiflash_read(unit_addr(unit), pu8Buf, sizeof(cfg_hdr_t));
    phdr = (cfg_hdr_t *)pu8Buf;
    if (phdr->magic != CFG_MAGIC) return 0;
    if (phdr->length == 0 || phdr->length > UNIT_SIZE - sizeof(cfg_hdr_t)) return 0;

    // spiflash_read(unit_addr(unit) + sizeof(cfg_hdr_t), buf, hdr->length);
    spiflash_read(unit_addr(unit), pu8Buf, sizeof(cfg_hdr_t)+phdr->length);
    return (calculate_checksum((uint8_t*)&pu8Buf[sizeof(cfg_hdr_t)], phdr->length) == phdr->checksum);
}

/* ================= 核心 API ================= */

/**
 * 初始化逻辑：上电自愈
 */
void ring_log_init(ring_log_t *l) {
    uint32_t addr = 0;
    int found_empty = 0;
    for (uint32_t s = 0; s < SECTOR_NUM; s++) {
        addr = unit_addr( s * UNITS_PER_SECTOR);
        if (sector_is_ff(addr)) {
            l->write_unit = s * UNITS_PER_SECTOR;
            found_empty = 1;
            break;
        }
    }

    if (!found_empty) {
        addr =  unit_addr( 0 );
        l->write_unit = 0;
        spiflash_erase_sector(addr);
    }
    info("[Init] Ready to write at Unit %u\n", l->write_unit);
}

/**
 * 写入逻辑：运行时仅维护预留空间 (Guard Sector)
 */
int ring_cfg_write(ring_log_t *l, const void *cfg, uint32_t len) {
    uint32_t addr = 0;
    uint8_t buf[UNIT_SIZE]={0};
    if (len > UNIT_SIZE - sizeof(cfg_hdr_t)) return -1;

    if ((l->write_unit % UNITS_PER_SECTOR) == 0) {
        // uint32_t next_s = (l->write_unit / UNITS_PER_SECTOR + 1) % SECTOR_NUM;
        addr =  unit_addr((l->write_unit+UNITS_PER_SECTOR)%TOTAL_UNITS);
        spiflash_erase_sector(addr);
    }

    addr = unit_addr(l->write_unit);
    // spiflash_write(addr + sizeof(cfg_hdr_t), cfg, len);
    memcpy( (uint8_t *)&buf[sizeof(cfg_hdr_t)],cfg, len );
    cfg_hdr_t hdr = {
        .magic = CFG_MAGIC,
        .length = len,
        .checksum = calculate_checksum((const uint8_t *)cfg, len)
    };
    memcpy( (uint8_t *)&buf[0],(uint8_t *)&hdr, sizeof(hdr));

    spiflash_write(addr, (uint8_t *)&buf[0], sizeof(hdr) + len );

    l->write_unit = (l->write_unit + 1) % TOTAL_UNITS;
    return 0;
}

int ring_cfg_read_latest(ring_log_t *l, void *out_buf, uint32_t buf_size, uint32_t *out_len) {
    uint8_t u8buf[UNIT_SIZE];
    for (uint32_t i = 1; i <= TOTAL_UNITS; i++) {
        uint32_t curr_u = (l->write_unit + TOTAL_UNITS - i) % TOTAL_UNITS;
        cfg_hdr_t *phdr;
        if (unit_is_valid(curr_u, u8buf)) {
            //* allow hdr.length low or high than buf_size
            // spiflash_read(unit_addr(curr_u) + sizeof(cfg_hdr_t), out_buf, hdr.length);
            phdr = (uint8_t *)u8buf;
            memcpy(out_buf,(uint8_t*)&u8buf[sizeof(cfg_hdr_t)],phdr->length);
            *out_len = phdr->length;
            // info("!!!flash read2!!!\n");
            return 0;
        }
    }
    // info("!!!flash read3!!!\n");
    return -1;
}

int ringlog_flash_init(void)
{
    int ret;
    ring_log_init(&glog);
    return 0;
}

int ringlog_flash_read(void *out_buf, uint32_t buf_size, uint32_t *out_len)
{
    int ret;
    ret = ring_cfg_read_latest(&glog, out_buf, buf_size, out_len);
    return ret;
}

int ringlog_flash_write(void *input_buf, uint32_t buf_size)
{
    int ret;
    ret = ring_cfg_write(&glog, input_buf, buf_size);
    return ret;
}


/* ================= 测试 ================= */

static void dump_flash(void)
{
    info("Flash state:\n");
    for (uint32_t s = 0; s < SECTOR_NUM; s++) {
        info(" Sector %u : %s\n", s, sector_is_ff(s) ? "ALL FF" : "DATA");
    }
    info("glog.write_unit: %u\n", glog.write_unit);
    /* ff_sector 变量已移除，无需打印 */
}

static void dump_flash_realdata(void)
{
    info("Flash Read Data:\n");
    for (uint32_t s = 0; s < TOTAL_UNITS; s++) {
        info(" Sector %u :\n", s);
        memdisplay(&g_flash[s*UNIT_SIZE], 16, 1);
    }
    
    /* ff_sector 变量已移除，无需打印 */
}
#if 0
int main(void) {
    uint8_t buf[512];
    
    // memset(buf, 0xAA, sizeof(buf));
    for (int i = 0; i < 512; i++) {
        // memset(buf, i, sizeof(buf));
        buf[i] = 0x10 + i;
    }
    memset(g_flash, 0xFF, sizeof(g_flash));

    int read_val;
    uint32_t read_len;
    dump_flash();
    info("--- Test 1: Normal Boot ---\n");
    ring_log_init(&glog);
    int d = 100;
    ring_cfg_write(&glog, &d, sizeof(d));
    
    // dump_flash();
    info("\n--- Test 2: Start On All Dirty Flash ---\n");
    // 人为弄脏所有扇区 

    memset(g_flash, 0x00, sizeof(g_flash));
    dump_flash();
    

    ring_log_init(&glog); // 这里应该打印 Self-healing

    dump_flash();
    // dump_flash();
    info("\n--- Test 3: Test write and read  ---\n");

    ring_log_init(&glog);

    for (int i = 0; i < 10; i++) {
        // memset(buf, i, sizeof(buf));
        buf[ 0 ] = 0x20+i;
        ring_cfg_write(&glog, buf,sizeof(buf));
        dump_flash();
    }
    ring_cfg_read_latest(&glog, &read_val, sizeof(read_val), &read_len);
    info("Latest Data: 0x%x\n", read_val);
    info("Latest Len : %d\n", read_len);

    info("\n--- Test 4: Init and read , manual destroy lasted data---\n");

    ring_log_init(&glog);
    //* manual destroy the last data
    g_flash[9* UNIT_SIZE] = 0;

    ring_cfg_read_latest(&glog, &read_val, sizeof(read_val), &read_len);
    info("Latest Data: 0x%x\n", read_val);
    info("Latest Len : %d\n", read_len);

    dump_flash();

    info("\n--- Test 5: Init and write ,test writecount ---\n");

    ring_log_init(&glog);

    dump_flash();
    buf[0]=0x33;
    ring_cfg_write(&glog, buf,sizeof(buf));


    dump_flash();
    // dump_flash_realdata();
    return 0;
}
#endif

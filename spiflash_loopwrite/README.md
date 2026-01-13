# ring_flash.c
1. 增加flash ring 读写，通过全0xff 来定位最后一次写位置
2. 每次上电，都从sector的unit0开始写
3. 每次writeUnit %  sector_UnitSize == 0时，会预擦除下一个sector

# ring_flash_cfg.c

1. 在原来的基础上，增加表头和checksum
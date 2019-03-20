#ifndef CONFIG_H
#define CONFIG_H

/*
 * the sector size of block device
 * default is 512 bytes
 */
#define SECTOR_SHIFT 9
#define SECTOR_SIZE (1 << SECTOR_SHIFT)
#define SECTOR_MASK (SECTOR_SIZE - 1)
#define GET_SECTOR(offset) ((offset) >> SECTOR_SHIFT)
#define SECTOR_OFFSET(offset) ((offset) & SECTOR_MASK)
#define SECTOR_DOWN(x) ((x) & ~SECTOR_MASK)
#define SECTOR_UP(x) ((((x) - 1) | SECTOR_MASK) + 1)


#endif

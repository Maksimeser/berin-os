#include <stdint.h>
#include "ata.h"
#include "kstring.h"

// Структура Boot Sector для FAT32
struct fat32_bpb {
    uint8_t  jump[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    // FAT32 specific
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    uint8_t  reserved[12];
    uint8_t  drive_num;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} __attribute__((packed));

// Глобальные переменные для работы с FAT32
static uint32_t fat_start_lba = 0;
static uint32_t data_start_lba = 0;
static uint8_t  sectors_per_cluster = 0;
static uint32_t root_cluster = 0;
static int      fat32_ready = 0;

int fat32_init(void) {
    uint8_t buf[512];
    int r = ata_read_slave(0, 1, buf);
    if (r != 0) return r; // Вернет -1 (нет диска) или -2 (ошибка чтения)

    struct fat32_bpb *bpb = (struct fat32_bpb *)buf;

    if (bpb->bytes_per_sector != 512) return -10;
    if (bpb->fs_type[0] != 'F' || bpb->fs_type[3] != '3') return -11; // "FAT32"

    sectors_per_cluster = bpb->sectors_per_cluster;
    root_cluster = bpb->root_cluster;

    fat_start_lba = bpb->reserved_sectors;
    data_start_lba = fat_start_lba + (bpb->num_fats * bpb->fat_size_32);

    fat32_ready = 1;
    return 0;
}

void fat32_info(void) {
    extern void term_write(const char *s);
    extern void term_putchar(char c);

    if (!fat32_ready) {
        term_write("[ FAIL ] FAT32 not mounted\n");
        return;
    }

    uint8_t buf[512];
    ata_read_slave(0, 1, buf);
    struct fat32_bpb *bpb = (struct fat32_bpb *)buf;

    term_write("[ FAT32 INFO ]\n");
    term_write("  OEM: ");
    for(int i=0; i<8; i++) term_putchar(bpb->oem[i]);
    term_putchar('\n');

    term_write("  VolLabel: ");
    for(int i=0; i<11; i++) term_putchar(bpb->volume_label[i]);
    term_putchar('\n');

    term_write("  Cluster size: ");
    term_write("sectors\n");
    term_write("  FAT start LBA: ");
    // (пропустим вывод чисел для краткости)
    term_write("OK\n");
}

/* bfsck - check BerFS v1 filesystem consistency. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "berfs_host.h"

static int errors = 0;
static int warnings = 0;

#define ERR(...) do { fprintf(stderr, "[ FAIL ] " __VA_ARGS__); errors++; } while (0)
#define WARN(...) do { fprintf(stderr, "[ WARN ] " __VA_ARGS__); warnings++; } while (0)
#define OK(...)   do { fprintf(stdout, "[ OK ] " __VA_ARGS__); } while (0)

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: bfsck <image> [--offset N]\n");
        return 1;
    }

    const char *path = argv[1];
    uint64_t offset = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--offset") == 0 && i + 1 < argc) {
            offset = strtoull(argv[i + 1], NULL, 0);
            i++;
        }
    }

    struct berfs_fs fs;
    int rc = berfs_open(&fs, path, offset);
    if (rc != 0) {
        fprintf(stderr, "[ FAIL ] cannot open %s (rc=%d)\n", path, rc);
        return 1;
    }

    printf("BerFS check: %s\n", path);
    printf("  magic:   0x%08X\n", fs.sb.sb_magic);
    printf("  version: %u\n", fs.sb.sb_version);
    printf("  features: 0x%08X\n", fs.sb.sb_feature_flags);
    printf("  blocks:  %u (%u MB)\n", fs.sb.sb_total_blocks,
           fs.sb.sb_total_blocks * BERFS_BLOCK_SIZE / 1024 / 1024);
    printf("  inodes:  %u\n", fs.sb.sb_total_inodes);
    printf("  data starts at block %u\n", fs.sb.sb_data_block);
    printf("\n");

    /* --- superblock --- */
    if (fs.sb.sb_version != BERFS_VERSION)
        WARN("superblock version %u != expected %u\n",
             fs.sb.sb_version, BERFS_VERSION);
        else
            OK("superblock version %u\n", fs.sb.sb_version);

    if (fs.sb.sb_block_size != BERFS_BLOCK_SIZE)
        ERR("block size %u != %u\n", fs.sb.sb_block_size, BERFS_BLOCK_SIZE);
    else
        OK("block size %u\n", fs.sb.sb_block_size);

    if (fs.sb.sb_root_inode != BERFS_ROOT_INODE)
        ERR("root inode %u != %u\n", fs.sb.sb_root_inode, BERFS_ROOT_INODE);
    else
        OK("root inode = %u\n", fs.sb.sb_root_inode);

    if (fs.sb.sb_data_block <= fs.sb.sb_inode_table_block)
        ERR("data_block (%u) must be > inode_table_block (%u)\n",
            fs.sb.sb_data_block, fs.sb.sb_inode_table_block);
        else
            OK("layout ordering\n");

    /* --- block bitmap --- */
    uint32_t bb_nblocks = (fs.sb.sb_total_blocks + 8u * BERFS_BLOCK_SIZE - 1u)
    / (8u * BERFS_BLOCK_SIZE);
    uint8_t *bb = calloc((size_t)bb_nblocks, BERFS_BLOCK_SIZE);
    if (!bb) { ERR("calloc block bitmap\n"); berfs_close(&fs); return 1; }
    if (berfs_read_block_bitmap(&fs, bb) != 0) {
        ERR("read block bitmap\n"); free(bb); berfs_close(&fs); return 1;
    }

    uint32_t counted_free_blocks = 0;
    for (uint32_t b = 0; b < fs.sb.sb_total_blocks; b++) {
        if (!berfs_bit_test(bb, b)) counted_free_blocks++;
    }

    printf("\n");
    if (counted_free_blocks != fs.sb.sb_free_blocks)
        ERR("free blocks: bitmap says %u, superblock says %u\n",
            counted_free_blocks, fs.sb.sb_free_blocks);
        else
            OK("free blocks = %u (matches superblock)\n", counted_free_blocks);

    /* reserved blocks must be marked used */
    for (uint32_t b = 0; b < fs.sb.sb_data_block; b++) {
        if (!berfs_bit_test(bb, b)) {
            ERR("reserved block %u is marked FREE\n", b);
            break;
        }
    }

    /* --- inode bitmap --- */
    uint32_t ib_nblocks = (fs.sb.sb_total_inodes + 8u * BERFS_BLOCK_SIZE - 1u)
    / (8u * BERFS_BLOCK_SIZE);
    uint8_t *ib = calloc((size_t)ib_nblocks, BERFS_BLOCK_SIZE);
    if (!ib) { ERR("calloc inode bitmap\n"); free(bb); berfs_close(&fs); return 1; }
    if (berfs_read_inode_bitmap(&fs, ib) != 0) {
        ERR("read inode bitmap\n"); free(bb); free(ib); berfs_close(&fs); return 1;
    }

    uint32_t counted_free_inodes = 0;
    for (uint32_t i = 0; i < fs.sb.sb_total_inodes; i++) {
        if (!berfs_bit_test(ib, i)) counted_free_inodes++;
    }

    printf("\n");
    if (counted_free_inodes != fs.sb.sb_free_inodes)
        ERR("free inodes: bitmap says %u, superblock says %u\n",
            counted_free_inodes, fs.sb.sb_free_inodes);
        else
            OK("free inodes = %u (matches superblock)\n", counted_free_inodes);

    if (!berfs_bit_test(ib, 0))
        ERR("inode 0 must be marked used\n");
    else
        OK("inode 0 reserved\n");

    if (!berfs_bit_test(ib, 1))
        ERR("inode 1 (root) must be marked used\n");
    else
        OK("inode 1 (root) allocated\n");

    /* --- root inode --- */
    struct berfs_inode root;
    if (berfs_read_inode(&fs, BERFS_ROOT_INODE, &root) != 0) {
        ERR("read root inode\n");
    } else {
        uint16_t type = root.i_mode & 0xF000u;
        if (type != BERFS_TYPE_DIR)
            ERR("root inode type is 0x%04X, expected DIR (0x%04X)\n",
                type, BERFS_TYPE_DIR);
            else
                OK("root inode is a directory\n");
    }

    printf("\n");
    if (errors == 0 && warnings == 0)
        printf("[ OK ] clean\n");
    else
        printf("[ ---- ] %d error(s), %d warning(s)\n", errors, warnings);

    free(bb);
    free(ib);
    berfs_close(&fs);
    return errors ? 1 : 0;
}

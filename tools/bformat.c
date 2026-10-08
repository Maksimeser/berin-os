/* bformat - create a BerFS v1 filesystem image on a regular file. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../include/berfs.h"
#include "berfs_host.h"
#include "berfs_dir.h"

static void die(const char *msg) {
    fprintf(stderr, "[ FAIL ] %s: %s\n", msg, strerror(errno));
    exit(1);
}

static void usage(void) {
    fprintf(stderr,
            "usage: bformat <image> <size_mb>\n"
            "       bformat --i-know-what-im-doing <device> <size_mb>\n");
    exit(1);
}

int main(int argc, char **argv) {
    int force_device = 0;
    int argi = 1;

    if (argc >= 2 && strcmp(argv[1], "--i-know-what-im-doing") == 0) {
        force_device = 1;
        argi = 2;
    }
    if (argc < argi + 2) usage();

    const char *path = argv[argi];
    uint32_t size_mb = (uint32_t)strtoul(argv[argi + 1], NULL, 10);
    if (size_mb == 0) {
        fprintf(stderr, "[ FAIL ] invalid_argument: size_mb = 0\n");
        return 1;
    }

    /* refuse to write to block devices unless forced */
    struct stat st;
    if (stat(path, &st) == 0 && S_ISBLK(st.st_mode) && !force_device) {
        fprintf(stderr,
                "[ FAIL ] refusing to format block device %s\n"
                "        use --i-know-what-im-doing if you are sure\n", path);
        return 1;
    }

    uint32_t total_blocks = (size_mb * 1024 * 1024) / BERFS_BLOCK_SIZE;
    uint32_t total_inodes = total_blocks / 4;
    if (total_inodes > BERFS_MAX_INODES) total_inodes = BERFS_MAX_INODES;

    struct berfs_layout layout;
    if (berfs_calc_layout(total_blocks, total_inodes, &layout) != 0) {
        fprintf(stderr, "[ FAIL ] invalid_argument: image too small\n");
        return 1;
    }

    /* 1. Create file with correct size (sparse) */
    FILE *fp = fopen(path, "wb+");
    if (!fp) die("fopen");

    if (fseek(fp, (long)total_blocks * BERFS_BLOCK_SIZE - 1, SEEK_SET) != 0)
        die("fseek");
    if (fputc(0, fp) == EOF) die("fputc");

    /* 2. Superblock at block 1 */
    uint8_t sb_buf[BERFS_BLOCK_SIZE];
    memset(sb_buf, 0, sizeof(sb_buf));
    struct berfs_superblock *sb = (struct berfs_superblock *)sb_buf;

    sb->sb_magic              = BERFS_MAGIC;
    sb->sb_version            = BERFS_VERSION;
    sb->sb_feature_flags      = 0;
    sb->sb_block_size         = BERFS_BLOCK_SIZE;
    sb->sb_total_blocks       = total_blocks;
    sb->sb_total_inodes       = total_inodes;
    sb->sb_free_blocks        = total_blocks - layout.data_start_block;
    sb->sb_free_inodes        = total_inodes - 2;   /* inode 0 + root inode 1 */
    sb->sb_root_inode         = BERFS_ROOT_INODE;
    sb->sb_inode_bitmap_block = 2;
    sb->sb_block_bitmap_block = 2 + layout.inode_bitmap_blocks;
    sb->sb_inode_table_block  = 2 + layout.inode_bitmap_blocks
    + layout.block_bitmap_blocks;
    sb->sb_data_block         = layout.data_start_block;
    sb->sb_mount_time         = 0;
    sb->sb_write_time         = (uint64_t)time(NULL);
    sb->sb_mount_count        = 0;
    sb->sb_max_mount          = 30;

    if (fseek(fp, BERFS_BLOCK_SIZE, SEEK_SET) != 0) die("fseek sb");
    if (fwrite(sb_buf, BERFS_BLOCK_SIZE, 1, fp) != 1) die("fwrite sb");

    /* 3. Inode bitmap: inode 0 and 1 used */
    uint8_t *inode_bitmap = calloc(1, layout.inode_bitmap_blocks * BERFS_BLOCK_SIZE);
    if (!inode_bitmap) die("calloc ib");
    inode_bitmap[0] = 0x03;

    if (fseek(fp, (long)sb->sb_inode_bitmap_block * BERFS_BLOCK_SIZE, SEEK_SET) != 0)
        die("fseek ib");
    if (fwrite(inode_bitmap, layout.inode_bitmap_blocks * BERFS_BLOCK_SIZE, 1, fp) != 1)
        die("fwrite ib");

    /* 4. Block bitmap: reserved blocks 0..data_start-1 used */
    uint8_t *block_bitmap = calloc(1, layout.block_bitmap_blocks * BERFS_BLOCK_SIZE);
    if (!block_bitmap) die("calloc bb");
    for (uint32_t b = 0; b < layout.data_start_block; b++)
        block_bitmap[b / 8] |= (uint8_t)(1u << (b % 8));

    if (fseek(fp, (long)sb->sb_block_bitmap_block * BERFS_BLOCK_SIZE, SEEK_SET) != 0)
        die("fseek bb");
    if (fwrite(block_bitmap, layout.block_bitmap_blocks * BERFS_BLOCK_SIZE, 1, fp) != 1)
        die("fwrite bb");

    /* 5. Inode table: root inode at index 1 */
    size_t inode_table_size = (size_t)layout.inode_table_blocks * BERFS_BLOCK_SIZE;
    uint8_t *inode_table = calloc(1, inode_table_size);
    if (!inode_table) die("calloc it");

    struct berfs_inode *root = (struct berfs_inode *)
    (inode_table + BERFS_ROOT_INODE * sizeof(struct berfs_inode));
    root->i_mode        = BERFS_TYPE_DIR | 0755;
    root->i_uid         = 0;
    root->i_size        = 0;
    root->i_links_count = 2;
    root->i_blocks      = 0;

    if (fseek(fp, (long)sb->sb_inode_table_block * BERFS_BLOCK_SIZE, SEEK_SET) != 0)
        die("fseek it");
    if (fwrite(inode_table, inode_table_size, 1, fp) != 1) die("fwrite it");

    /* 6. Close and reopen via library to add . and .. entries */
    fclose(fp);
    fp = NULL;
    free(inode_bitmap);
    free(block_bitmap);
    free(inode_table);

    struct berfs_fs fs;
    if (berfs_open(&fs, path, 0) != 0) {
        fprintf(stderr, "[ FAIL ] cannot reopen for dot entries\n");
        return 1;
    }
    struct berfs_inode root2;
    if (berfs_read_inode(&fs, BERFS_ROOT_INODE, &root2) != 0) {
        fprintf(stderr, "[ FAIL ] cannot read root inode\n");
        berfs_close(&fs);
        return 1;
    }
    if (berfs_dir_add(&fs, &root2, BERFS_ROOT_INODE, ".",  BERFS_ROOT_INODE, BERFS_DT_DIR) != 0 ||
        berfs_dir_add(&fs, &root2, BERFS_ROOT_INODE, "..", BERFS_ROOT_INODE, BERFS_DT_DIR) != 0) {
        fprintf(stderr, "[ FAIL ] cannot add dot entries\n");
    berfs_close(&fs);
    return 1;
        }
        berfs_close(&fs);

        /* 7. Report */
        printf("[ OK ] formatted %s\n", path);
        printf("  size:        %u MB (%u blocks of %u bytes)\n",
               size_mb, total_blocks, BERFS_BLOCK_SIZE);
        printf("  inodes:      %u\n", total_inodes);
        printf("  inode bmap:  block %u (%u block(s))\n",
               sb->sb_inode_bitmap_block, layout.inode_bitmap_blocks);
        printf("  block bmap:  block %u (%u block(s))\n",
               sb->sb_block_bitmap_block, layout.block_bitmap_blocks);
        printf("  inode table: block %u (%u block(s))\n",
               sb->sb_inode_table_block, layout.inode_table_blocks);
        printf("  data start:  block %u\n", sb->sb_data_block);
        printf("  free blocks: %u\n", sb->sb_free_blocks);
        printf("  free inodes: %u\n", sb->sb_free_inodes);

        return 0;
}

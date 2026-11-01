#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stdint.h>

struct boot_info {
    uint64_t fb_base;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;
};

#endif

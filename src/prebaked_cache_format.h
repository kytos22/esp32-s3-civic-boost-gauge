#pragma once

#include <Arduino.h>

static constexpr uint32_t PREBAKED_CACHE_MAGIC = 0x31434742UL; // "BGC1"
static constexpr uint32_t PREBAKED_CACHE_FORMAT_VERSION = 1;

struct __attribute__((packed)) PrebakedCacheHeader {
    uint32_t magic;
    uint32_t format_version;
    uint32_t total_size;
    uint32_t payload_crc32;
    uint32_t state_count;
    uint32_t state_size;
    uint32_t tile_mask_bytes;
    uint32_t arc_command_count;
    uint32_t arc_command_size;
    uint32_t cursor_pixel_count;
    uint32_t cursor_pixel_size;
    uint32_t spatial_run_count;
    uint32_t spatial_run_size;
    uint32_t arc_pixel_count;
};

static_assert(sizeof(PrebakedCacheHeader) == 56,
              "Unexpected prebaked cache header layout");

inline size_t prebaked_cache_align4(size_t value)
{
    return (value + 3U) & ~size_t(3U);
}


#pragma once

#include <Arduino.h>

struct PrebakedVisual {
    const uint32_t *run_offsets;
    const uint16_t *run_lengths;
    const uint16_t *colors;
    uint32_t run_count;
    uint32_t pixel_count;
};

extern const PrebakedVisual PREBAKED_GAUGE_VISUAL;
extern const PrebakedVisual PREBAKED_STARTUP_VISUAL;

#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include "Arduino_DriveBus_Library.h"
#include "pin_config.h"
#include "prebaked_cache_format.h"
#include "prebaked_gauge_cache.h"
#include "prebaked_visuals.h"
#include <Wire.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <esp_heap_caps.h>
#include <esp_rom_sys.h>
#include <esp32s3/rom/miniz.h>

#define ENABLE_PERF_TELEMETRY 0
#define ENABLE_RENDER_DIAGNOSTICS 0
#define DUMP_PREBAKED_FRAMES 0
#define DUMP_BAKED_CACHE 0

extern "C"
{
LV_FONT_DECLARE(boost_font_90_bold);
}

// =========================
// Display / Touch
// =========================

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

Arduino_SH8601 *gfx = new Arduino_SH8601(
    bus, LCD_RST, 0 /* rotation */, false /* IPS */, LCD_WIDTH, LCD_HEIGHT);

std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus =
    std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);

std::unique_ptr<Arduino_IIC> FT3168(
    new Arduino_FT3x68(IIC_Bus, FT3168_DEVICE_ADDRESS));

// =========================
// LVGL buffer
// =========================

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[LCD_WIDTH * LCD_HEIGHT / 10];

static lv_color_t *static_frame = nullptr;
static lv_color_t *composited_frame = nullptr;
static bool prebaked_enabled = false;
static bool prebaked_force_full = true;
static bool gauge_restore_pending = false;
static bool startup_splash_visible = false;

#if ENABLE_RENDER_DIAGNOSTICS
static uint32_t diagnostic_flush_count = 0;
static uint32_t diagnostic_flush_pixels = 0;
#endif

#if DUMP_PREBAKED_FRAMES
void dump_sparse_frame(const char *name, const lv_color_t *frame)
{
    const lv_color_t black = lv_color_black();
    uint32_t count = 0;
    for (uint32_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; ++i) {
        if (frame[i].full != black.full) ++count;
    }

    Serial.printf("PBK1 %s %lu\n", name, (unsigned long)count);
    Serial.flush();
    for (uint32_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; ++i) {
        if (frame[i].full == black.full) continue;
        Serial.write((const uint8_t *)&i, sizeof(i));
        Serial.write((const uint8_t *)&frame[i].full, sizeof(frame[i].full));
    }
    Serial.printf("\nENDPBK1 %s\n", name);
    Serial.flush();
}
#endif

#if ENABLE_PERF_TELEMETRY
struct DisplayPerfStats {
    uint64_t flush_time_us = 0;
    uint64_t flushed_pixels = 0;
    uint64_t lvgl_time_us = 0;
    uint32_t flush_count = 0;
    uint32_t lvgl_count = 0;
    uint32_t max_flush_us = 0;
    uint32_t max_lvgl_us = 0;
    uint64_t custom_time_us = 0;
    uint32_t custom_count = 0;
    uint32_t max_custom_us = 0;
    uint64_t display_time_us = 0;
};

static DisplayPerfStats perf_stats;
#endif

// =========================
// UI objects
// =========================

lv_obj_t *label_value;
lv_obj_t *label_unit;
lv_obj_t *civic_logo_image;
lv_obj_t *brightness_panel;
lv_obj_t *brightness_label;
lv_obj_t *brightness_slider;
lv_obj_t *zero_calibrate_button;
lv_obj_t *zero_calibrate_label;
lv_obj_t *indicator_outline;
lv_obj_t *indicator_cursor;
lv_obj_t *arc_fill;
static lv_style_t arc_indicator_style;

static const uint8_t BOOST_TICK_COUNT = 46;
static const uint8_t BOOST_LABEL_COUNT = 10;

lv_obj_t *tick_lines[BOOST_TICK_COUNT];
lv_point_t tick_points[BOOST_TICK_COUNT][2];
lv_obj_t *scale_labels[BOOST_LABEL_COUNT];
lv_point_t indicator_outline_points[2];
lv_point_t indicator_cursor_points[2];

// =========================
// Config presión
// =========================

// Rango visual del gauge
static const float BOOST_MIN_PSI = -15.0f;
static const float BOOST_MAX_PSI =  30.0f;

// Sensibilidad de la tabla del sensor alimentado a 5 V y conectado
// directamente al ADC: 0.1 V = -15 PSI, 1.0 V = 0 PSI y 3.33 V = 30 PSI.
// Recalibrar desplaza el cero, pero conserva estas pendientes.
static const float SENSOR_VACUUM_SPAN_MV = 900.0f;
static const float SENSOR_30_PSI_SPAN_MV = 2330.0f;
static const float SENSOR_DEFAULT_ZERO_MV = 1000.0f;
static const float SENSOR_MIN_VALID_ZERO_MV = 600.0f;
static const float SENSOR_MAX_VALID_ZERO_MV = 1300.0f;
static const float SENSOR_ZERO_DEADBAND_PSI = 0.25f;
static const float SENSOR_FILTER_ALPHA = 0.18f;
static const uint8_t SENSOR_SAMPLE_COUNT = 8;
static const uint8_t SENSOR_CALIBRATION_SAMPLES = 64;

// Centro y radio del gauge
// Native framebuffer orientation with the USB connector at the bottom.
// The original design was rotated clockwise during every display flush;
// geometry is now generated in that final orientation instead.
static const int CENTER_X = (LCD_WIDTH - 1) - (LCD_HEIGHT / 2);
static const int CENTER_Y = LCD_WIDTH / 2;
static const float GAUGE_START_DEG = 225.0f;
static const float GAUGE_ZERO_DEG = 270.0f;
static const float GAUGE_END_DEG = 495.0f;
static const float GAUGE_SWEEP_DEG = GAUGE_END_DEG - GAUGE_START_DEG;

static const int GAUGE_DIAMETER = 466;
static const int TICK_OUTER_RADIUS = 201;
static const int TICK_MINOR_INNER_RADIUS = 190;
static const int TICK_MAJOR_INNER_RADIUS = 178;
static const int LABEL_RADIUS = 158;
static const int CURSOR_INNER_RADIUS = 199;
static const int CURSOR_OUTER_RADIUS = 232;
static const int ARC_WIDTH = 24;
static const int ARC_COLOR_SEGMENT_DEG = 10;
static const int ARC_COLOR_SEGMENT_COUNT = 27;
static const int PREBAKED_SEGMENT_DEG = 30;
static const int PREBAKED_SEGMENT_COUNT = 9;
static const int BAKED_GAUGE_STEP_TENTHS = 5;
static const int BAKED_GAUGE_STATE_COUNT =
    (int)(GAUGE_SWEEP_DEG * 10.0f) / BAKED_GAUGE_STEP_TENTHS + 1;
static const int BAKED_TILE_SIZE = 16;
static const int BAKED_TILE_COLUMNS =
    (LCD_WIDTH + BAKED_TILE_SIZE - 1) / BAKED_TILE_SIZE;
static const int BAKED_TILE_ROWS =
    (LCD_HEIGHT + BAKED_TILE_SIZE - 1) / BAKED_TILE_SIZE;
static const int BAKED_TILE_COUNT = BAKED_TILE_COLUMNS * BAKED_TILE_ROWS;
static const int BAKED_TILE_MASK_BYTES = (BAKED_TILE_COUNT + 7) / 8;
static const uint32_t SHOW_CYCLE_MS = 12000;
static const uint32_t STARTUP_SPLASH_MS = 5000;
static const uint32_t STARTUP_SWEEP_UP_MS = 1300;
static const uint32_t STARTUP_SWEEP_DOWN_MS = 1400;
static const uint32_t GAUGE_FRAME_PERIOD_US = 16667;
static const uint32_t VALUE_UPDATE_MS = 33;
static const uint32_t SERIAL_UPDATE_MS = 200;
#if ENABLE_PERF_TELEMETRY
static const uint32_t PERF_UPDATE_MS = 2000;
#endif

float filtered_bar = 0.0f;
static uint16_t last_sensor_mv = 0;
static uint16_t last_sensor_sample_mv = 0;
static float filtered_sensor_mv = SENSOR_DEFAULT_ZERO_MV;
static bool sensor_filter_initialized = false;
static float sensor_vacuum_mv = SENSOR_DEFAULT_ZERO_MV - SENSOR_VACUUM_SPAN_MV;
static float sensor_atmosphere_mv = SENSOR_DEFAULT_ZERO_MV;
static float sensor_30_psi_mv = SENSOR_DEFAULT_ZERO_MV + SENSOR_30_PSI_SPAN_MV;
static lv_timer_t *zero_feedback_timer = nullptr;

enum GaugeMode {
    GAUGE_MODE_LIVE,
    GAUGE_MODE_SHOW
};

enum StartupPhase {
    STARTUP_SPLASH,
    STARTUP_SWEEP_UP,
    STARTUP_SWEEP_DOWN,
    STARTUP_COMPLETE
};

GaugeMode gauge_mode = GAUGE_MODE_LIVE;
StartupPhase startup_phase = STARTUP_SPLASH;
uint32_t show_started_ms = 0;
uint32_t startup_phase_started_ms = 0;
float startup_sweep_target_psi = 0.0f;
uint8_t screen_brightness = 191; // 75% of the display brightness range
bool brightness_menu_open = false;
bool suppress_show_click = false;
lv_area_t arc_color_areas[ARC_COLOR_SEGMENT_COUNT];
lv_area_t prebaked_arc_areas[PREBAKED_SEGMENT_COUNT];
lv_area_t prebaked_cursor_areas[PREBAKED_SEGMENT_COUNT];

struct ArcPixel {
    uint32_t offset;
    uint16_t angle_tenths;
    uint8_t opacity;
};

struct __attribute__((packed)) SpatialArcPixel {
    uint16_t angle_tenths;
    uint8_t opacity;
};

struct ArcPixelRun {
    uint32_t offset;
    uint16_t first_pixel;
    uint16_t length;
};

struct __attribute__((packed)) BakedArcCommand {
    uint16_t length;
    uint16_t color;
};

struct __attribute__((packed)) BakedCursorPixel {
    uint16_t relative_offset;
    uint16_t color;
};

struct BakedGaugeState {
    uint32_t first_arc_command;
    uint32_t first_cursor_pixel;
    uint16_t arc_command_count;
    uint16_t cursor_pixel_count;
    uint16_t color;
    lv_area_t cursor_area;
    lv_area_t arc_delta_area;
};

static ArcPixel *prebaked_arc_pixels = nullptr;
static SpatialArcPixel *spatial_arc_pixels = nullptr;
static ArcPixelRun *spatial_arc_runs = nullptr;
static uint32_t prebaked_arc_pixel_count = 0;
static uint16_t spatial_arc_run_count = 0;
static BakedGaugeState *baked_gauge_states = nullptr;
static BakedArcCommand *baked_arc_commands = nullptr;
static BakedCursorPixel *baked_cursor_pixels = nullptr;
static uint8_t *baked_arc_tile_masks = nullptr;
static uint32_t baked_arc_command_count = 0;
static uint32_t baked_cursor_pixel_count = 0;
static uint8_t *prebaked_cache_storage = nullptr;
static lv_area_t previous_cursor_area = {0, 0, -1, -1};

struct PrebakedValueGlyph {
    char character;
    lv_font_glyph_dsc_t descriptor;
    uint16_t width;
    uint16_t height;
    uint8_t *opacity;
};

static const char VALUE_GLYPH_CHARACTERS[] = "-.0123456789";
static const uint8_t VALUE_GLYPH_COUNT = sizeof(VALUE_GLYPH_CHARACTERS) - 1;
static const int VALUE_FONT_LINE_HEIGHT = 66;
static const int VALUE_FONT_BASE_LINE = 1;
static const int VALUE_LOGICAL_WIDTH = 250;
static const int VALUE_LOGICAL_X = (LCD_WIDTH - VALUE_LOGICAL_WIDTH) / 2;
static const int VALUE_LOGICAL_Y =
    (LCD_HEIGHT - VALUE_FONT_LINE_HEIGHT) / 2 - 24;
static const lv_area_t VALUE_PHYSICAL_AREA = {
    (lv_coord_t)(LCD_HEIGHT - 1 -
                 (VALUE_LOGICAL_Y + VALUE_FONT_LINE_HEIGHT - 1)),
    (lv_coord_t)VALUE_LOGICAL_X,
    (lv_coord_t)(LCD_HEIGHT - 1 - VALUE_LOGICAL_Y),
    (lv_coord_t)(VALUE_LOGICAL_X + VALUE_LOGICAL_WIDTH - 1)
};
static PrebakedValueGlyph value_glyphs[VALUE_GLYPH_COUNT];
static bool value_glyphs_ready = false;
static char rendered_value_text[16] = "";

// =========================
// Utilidades
// =========================

float clampf(float v, float minv, float maxv)
{
    if (v < minv) return minv;
    if (v > maxv) return maxv;
    return v;
}

float mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

lv_color_t boost_color(float psi_value);
void include_dirty_area(lv_area_t &destination, const lv_area_t &source);

lv_point_t rotate_design_point_clockwise(lv_coord_t x, lv_coord_t y)
{
    return {
        (lv_coord_t)(LCD_HEIGHT - 1 - y),
        x
    };
}

void align_rotated_object(lv_obj_t *obj, lv_coord_t logical_x, lv_coord_t logical_y)
{
    lv_point_t point = rotate_design_point_clockwise(logical_x, logical_y);
    lv_obj_align(
        obj, LV_ALIGN_CENTER,
        point.x - LCD_WIDTH / 2,
        point.y - LCD_HEIGHT / 2);
    lv_obj_set_style_transform_pivot_x(obj, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(obj, LV_PCT(50), 0);
    lv_obj_set_style_transform_angle(obj, 900, 0);
}

bool calibrate_pressure_zero()
{
    uint32_t mv_sum = 0;
    for (uint8_t sample = 0; sample < SENSOR_CALIBRATION_SAMPLES; ++sample) {
        mv_sum += analogReadMilliVolts(PRESSURE_SENSOR_PIN);
        delayMicroseconds(100);
    }

    float measured_zero_mv = mv_sum / (float)SENSOR_CALIBRATION_SAMPLES;
    if (measured_zero_mv < SENSOR_MIN_VALID_ZERO_MV ||
        measured_zero_mv > SENSOR_MAX_VALID_ZERO_MV) {
        Serial.printf("Zero calibration rejected: %.1f mV\n", measured_zero_mv);
        return false;
    }

    sensor_atmosphere_mv = measured_zero_mv;
    sensor_vacuum_mv = measured_zero_mv - SENSOR_VACUUM_SPAN_MV;
    sensor_30_psi_mv = measured_zero_mv + SENSOR_30_PSI_SPAN_MV;
    filtered_sensor_mv = measured_zero_mv;
    sensor_filter_initialized = true;
    last_sensor_mv = (uint16_t)lroundf(measured_zero_mv);
    Serial.printf("Zero calibrated: %.1f mV\n", measured_zero_mv);
    return true;
}

float gauge_angle_for_psi(float psi_value)
{
    if (psi_value <= 0.0f) {
        return mapf(psi_value, BOOST_MIN_PSI, 0.0f, GAUGE_START_DEG, GAUGE_ZERO_DEG);
    }

    return mapf(psi_value, 0.0f, BOOST_MAX_PSI, GAUGE_ZERO_DEG, GAUGE_END_DEG);
}

bool area_is_valid(const lv_area_t &area)
{
    return area.x1 <= area.x2 && area.y1 <= area.y2;
}

#if ENABLE_RENDER_DIAGNOSTICS
void lvgl_log_print(const char *message)
{
    Serial.print("[lvgl] ");
    Serial.print(message);
}

uint32_t count_non_black_pixels(const lv_color_t *frame)
{
    if (frame == nullptr) return 0;

    const lv_color_t black = lv_color_black();
    uint32_t count = 0;
    for (uint32_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; ++i) {
        if (frame[i].full != black.full) ++count;
    }
    return count;
}

void print_frame_diagnostics(const char *stage)
{
    Serial.printf(
        "[render] %s: static=%lu, composited=%lu, flushes=%lu, pixels=%lu, free_psram=%lu\n",
        stage,
        (unsigned long)count_non_black_pixels(static_frame),
        (unsigned long)count_non_black_pixels(composited_frame),
        (unsigned long)diagnostic_flush_count,
        (unsigned long)diagnostic_flush_pixels,
        (unsigned long)ESP.getFreePsram());
}
#endif

void copy_area_to_frame(lv_color_t *destination, const lv_area_t *area,
                        const lv_color_t *source)
{
    if (destination == nullptr) return;

    int32_t width = area->x2 - area->x1 + 1;
    for (int32_t row = 0; row <= area->y2 - area->y1; ++row) {
        lv_color_t *target = destination + (area->y1 + row) * LCD_WIDTH + area->x1;
        memcpy(target, source + row * width, width * sizeof(lv_color_t));
    }
}

void draw_native_area(const lv_area_t &area, const lv_color_t *source)
{
    const int32_t width = area.x2 - area.x1 + 1;
    const int32_t height = area.y2 - area.y1 + 1;
#if ENABLE_PERF_TELEMETRY
    const uint32_t display_started_us = micros();
#endif
    gfx->draw16bitBeRGBBitmap(
        area.x1, area.y1, (uint16_t *)source, width, height);
#if ENABLE_PERF_TELEMETRY
    perf_stats.display_time_us += micros() - display_started_us;
#endif
}

void restore_frame_area(const lv_area_t &requested_area)
{
    if (!area_is_valid(requested_area)) return;

    lv_area_t area = requested_area;
    area.x1 = LV_MAX(area.x1, 0);
    area.y1 = LV_MAX(area.y1, 0);
    area.x2 = LV_MIN(area.x2, LCD_WIDTH - 1);
    area.y2 = LV_MIN(area.y2, LCD_HEIGHT - 1);

    int32_t width = area.x2 - area.x1 + 1;
    for (int32_t y = area.y1; y <= area.y2; ++y) {
        lv_color_t *target = composited_frame + y * LCD_WIDTH + area.x1;
        const lv_color_t *source = static_frame + y * LCD_WIDTH + area.x1;
        memcpy(target, source, width * sizeof(lv_color_t));
    }
}

void flush_composited_area(const lv_area_t &requested_area)
{
    if (!prebaked_enabled || !area_is_valid(requested_area)) return;

    lv_area_t area = requested_area;
    area.x1 = LV_MAX(area.x1, 0);
    area.y1 = LV_MAX(area.y1, 0);
    area.x2 = LV_MIN(area.x2, LCD_WIDTH - 1);
    area.y2 = LV_MIN(area.y2, LCD_HEIGHT - 1);
    if (!area_is_valid(area)) return;

    // SH8601 CASET/PASET require an even start coordinate and an even
    // window size. Expanding to the nearest valid 2x2 grid prevents partial
    // writes from wrapping or leaving stale pixels at specific angles.
    area.x1 &= ~1;
    area.y1 &= ~1;
    area.x2 = LV_MIN(LCD_WIDTH - 1, area.x2 | 1);
    area.y2 = LV_MIN(LCD_HEIGHT - 1, area.y2 | 1);

    const int32_t width = area.x2 - area.x1 + 1;
    const int32_t buffer_pixels = (int32_t)(sizeof(buf) / sizeof(buf[0]));
    int32_t rows_per_transfer = LV_MAX(2, buffer_pixels / width);
    // Every chunk opens its own SH8601 PASET window, so the chunk itself must
    // also start on an even row and contain an even number of rows.
    rows_per_transfer &= ~1;

    // Open CASET/PASET once for the complete dirty area. Subsequent chunks use
    // RAMWRC, keeping one continuous scan instead of exposing intermediate
    // states through several overlapping display windows.
#if ENABLE_PERF_TELEMETRY
    const uint32_t started_us = micros();
#endif
    gfx->startWrite();
    gfx->writeAddrWindow(area.x1, area.y1, width, area.y2 - area.y1 + 1);
    for (int32_t y = area.y1; y <= area.y2; y += rows_per_transfer) {
        int32_t rows = LV_MIN(rows_per_transfer, area.y2 - y + 1);
        for (int32_t row = 0; row < rows; ++row) {
            const lv_color_t *source = composited_frame + (y + row) * LCD_WIDTH + area.x1;
            memcpy(buf + row * width, source, width * sizeof(lv_color_t));
        }
        gfx->writeBytes((uint8_t *)buf, width * rows * sizeof(lv_color_t));
    }
    gfx->endWrite();
#if ENABLE_PERF_TELEMETRY
    const uint32_t elapsed_us = micros() - started_us;
    perf_stats.flush_time_us += elapsed_us;
    perf_stats.display_time_us += elapsed_us;
    perf_stats.flushed_pixels +=
        (uint32_t)width * (uint32_t)(area.y2 - area.y1 + 1);
    ++perf_stats.flush_count;
    perf_stats.max_flush_us = LV_MAX(perf_stats.max_flush_us, elapsed_us);
#endif
}

bool allocate_prebaked_frames()
{
    const size_t frame_bytes = LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t);
    static_frame = (lv_color_t *)heap_caps_malloc(
        frame_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    composited_frame = (lv_color_t *)heap_caps_malloc(
        frame_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (static_frame == nullptr || composited_frame == nullptr) {
        if (static_frame != nullptr) heap_caps_free(static_frame);
        if (composited_frame != nullptr) heap_caps_free(composited_frame);
        static_frame = nullptr;
        composited_frame = nullptr;
        return false;
    }

    memset(static_frame, 0, frame_bytes);
    memset(composited_frame, 0, frame_bytes);
    return true;
}

void apply_prebaked_visual(lv_color_t *frame, const PrebakedVisual &visual)
{
    if (frame == nullptr) return;

    memset(frame, 0, LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t));
    uint32_t color_index = 0;
    for (uint32_t run = 0; run < visual.run_count; ++run) {
        uint32_t offset = visual.run_offsets[run];
        uint16_t length = visual.run_lengths[run];
        memcpy(frame + offset, visual.colors + color_index,
               length * sizeof(uint16_t));
        color_index += length;
    }
}

void display_prebaked_visual(const PrebakedVisual &visual)
{
    apply_prebaked_visual(composited_frame, visual);
    flush_composited_area({0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1});
}

bool prepare_prebaked_value_glyphs()
{
    if (boost_font_90_bold.line_height != VALUE_FONT_LINE_HEIGHT ||
        boost_font_90_bold.base_line != VALUE_FONT_BASE_LINE) {
        Serial.println("Prebaked value renderer: unexpected font metrics");
        return false;
    }

    for (uint8_t index = 0; index < VALUE_GLYPH_COUNT; ++index) {
        PrebakedValueGlyph &glyph = value_glyphs[index];
        glyph.character = VALUE_GLYPH_CHARACTERS[index];
        if (!lv_font_get_glyph_dsc(
                &boost_font_90_bold, &glyph.descriptor,
                glyph.character, '\0')) {
            return false;
        }

        const uint8_t *bitmap = lv_font_get_glyph_bitmap(
            glyph.descriptor.resolved_font, glyph.character);
        if (bitmap == nullptr || glyph.descriptor.bpp != 4) return false;

        glyph.width = glyph.descriptor.box_h;
        glyph.height = glyph.descriptor.box_w;
        size_t pixel_count = glyph.width * glyph.height;
        glyph.opacity = (uint8_t *)heap_caps_malloc(
            pixel_count, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (glyph.opacity == nullptr) return false;
        memset(glyph.opacity, 0, pixel_count);

        for (uint16_t row = 0; row < glyph.descriptor.box_h; ++row) {
            for (uint16_t column = 0; column < glyph.descriptor.box_w; ++column) {
                uint32_t source_pixel =
                    row * glyph.descriptor.box_w + column;
                uint8_t packed = bitmap[source_pixel >> 1];
                uint8_t shade = (source_pixel & 1)
                    ? packed & 0x0F
                    : packed >> 4;
                uint16_t rotated_x = glyph.descriptor.box_h - 1 - row;
                uint16_t rotated_y = column;
                glyph.opacity[rotated_y * glyph.width + rotated_x] = shade * 17;
            }
        }
    }

    value_glyphs_ready = true;
    return true;
}

const PrebakedValueGlyph *find_value_glyph(char character)
{
    for (const PrebakedValueGlyph &glyph : value_glyphs) {
        if (glyph.character == character) return &glyph;
    }
    return nullptr;
}

void render_prebaked_value(const char *text, bool force)
{
    if (!prebaked_enabled || !value_glyphs_ready || brightness_menu_open) return;
    if (!force && strcmp(text, rendered_value_text) == 0) return;

    restore_frame_area(VALUE_PHYSICAL_AREA);

    int text_width = 0;
    for (const char *cursor = text; *cursor; ++cursor) {
        const PrebakedValueGlyph *glyph = find_value_glyph(*cursor);
        if (glyph != nullptr) {
            text_width += lv_font_get_glyph_width(
                &boost_font_90_bold, *cursor, cursor[1]);
        }
    }

    int logical_x = VALUE_LOGICAL_X + (VALUE_LOGICAL_WIDTH - text_width) / 2;
    const lv_color_t white = lv_color_white();
    for (const char *cursor = text; *cursor; ++cursor) {
        const PrebakedValueGlyph *glyph = find_value_glyph(*cursor);
        if (glyph == nullptr) continue;

        int glyph_x = logical_x + glyph->descriptor.ofs_x;
        int glyph_y = VALUE_LOGICAL_Y +
            (VALUE_FONT_LINE_HEIGHT - VALUE_FONT_BASE_LINE) -
            glyph->descriptor.box_h - glyph->descriptor.ofs_y;
        int physical_x = LCD_HEIGHT - glyph_y - glyph->descriptor.box_h;
        int physical_y = glyph_x;

        for (uint16_t y = 0; y < glyph->height; ++y) {
            for (uint16_t x = 0; x < glyph->width; ++x) {
                uint8_t opacity = glyph->opacity[y * glyph->width + x];
                if (opacity == 0) continue;
                int destination_x = physical_x + x;
                int destination_y = physical_y + y;
                if ((unsigned)destination_x >= LCD_WIDTH ||
                    (unsigned)destination_y >= LCD_HEIGHT) continue;
                lv_color_t &pixel =
                    composited_frame[destination_y * LCD_WIDTH + destination_x];
                pixel = opacity == 255
                    ? white
                    : lv_color_mix(white, pixel, opacity);
            }
        }
        logical_x += lv_font_get_glyph_width(
            &boost_font_90_bold, *cursor, cursor[1]);
    }

    strncpy(rendered_value_text, text, sizeof(rendered_value_text) - 1);
    rendered_value_text[sizeof(rendered_value_text) - 1] = '\0';
}

void prepare_prebaked_arc_areas()
{
    const float centerline_radius = (GAUGE_DIAMETER / 2.0f) - (ARC_WIDTH / 2.0f);
    const int arc_margin = (ARC_WIDTH / 2) + 2;
    const int cursor_margin = (int)ceilf(
        centerline_radius - (CURSOR_INNER_RADIUS - 9)) + 1;

    for (int segment = 0; segment < PREBAKED_SEGMENT_COUNT; ++segment) {
        const int start = segment * PREBAKED_SEGMENT_DEG;
        const int end = LV_MIN(start + PREBAKED_SEGMENT_DEG, (int)GAUGE_SWEEP_DEG);
        int min_x = LCD_WIDTH;
        int min_y = LCD_HEIGHT;
        int max_x = 0;
        int max_y = 0;

        for (int offset = start; offset <= end; ++offset) {
            float radians = (GAUGE_START_DEG + offset) * DEG_TO_RAD;
            int x = (int)lroundf(CENTER_X + cosf(radians) * centerline_radius);
            int y = (int)lroundf(CENTER_Y + sinf(radians) * centerline_radius);
            min_x = LV_MIN(min_x, x);
            min_y = LV_MIN(min_y, y);
            max_x = LV_MAX(max_x, x);
            max_y = LV_MAX(max_y, y);
        }

        prebaked_arc_areas[segment] = {
            (lv_coord_t)LV_MAX(0, min_x - arc_margin),
            (lv_coord_t)LV_MAX(0, min_y - arc_margin),
            (lv_coord_t)LV_MIN(LCD_WIDTH - 1, max_x + arc_margin),
            (lv_coord_t)LV_MIN(LCD_HEIGHT - 1, max_y + arc_margin)
        };
        prebaked_cursor_areas[segment] = {
            (lv_coord_t)LV_MAX(0, min_x - cursor_margin),
            (lv_coord_t)LV_MAX(0, min_y - cursor_margin),
            (lv_coord_t)LV_MIN(LCD_WIDTH - 1, max_x + cursor_margin),
            (lv_coord_t)LV_MIN(LCD_HEIGHT - 1, max_y + cursor_margin)
        };
    }
}

bool prepare_prebaked_arc_pixels()
{
    const float inner_radius = (GAUGE_DIAMETER / 2.0f) - ARC_WIDTH;
    const float outer_radius = GAUGE_DIAMETER / 2.0f;
    uint32_t count = 0;
    uint16_t run_count = 0;
    uint32_t previous_offset = UINT32_MAX;

    for (int y = 0; y < LCD_HEIGHT; ++y) {
        for (int x = 0; x < LCD_WIDTH; ++x) {
            float dx = x - CENTER_X;
            float dy = y - CENTER_Y;
            float radius = sqrtf(dx * dx + dy * dy);
            if (radius < inner_radius - 0.5f || radius > outer_radius + 0.5f) continue;

            float angle = atan2f(dy, dx) / DEG_TO_RAD;
            if (angle < GAUGE_START_DEG) angle += 360.0f;
            float relative_angle = angle - GAUGE_START_DEG;
            if (relative_angle >= 0.0f && relative_angle <= GAUGE_SWEEP_DEG) {
                uint32_t offset = (uint32_t)(y * LCD_WIDTH + x);
                if (count == 0 || offset != previous_offset + 1) ++run_count;
                previous_offset = offset;
                ++count;
            }
        }
    }

    prebaked_arc_pixels = (ArcPixel *)heap_caps_malloc(
        count * sizeof(ArcPixel), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    spatial_arc_pixels = (SpatialArcPixel *)heap_caps_malloc(
        count * sizeof(SpatialArcPixel), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    spatial_arc_runs = (ArcPixelRun *)heap_caps_malloc(
        run_count * sizeof(ArcPixelRun), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (spatial_arc_pixels == nullptr) {
        spatial_arc_pixels = (SpatialArcPixel *)heap_caps_malloc(
            count * sizeof(SpatialArcPixel), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (spatial_arc_runs == nullptr) {
        spatial_arc_runs = (ArcPixelRun *)heap_caps_malloc(
            run_count * sizeof(ArcPixelRun), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (prebaked_arc_pixels == nullptr || spatial_arc_pixels == nullptr ||
        spatial_arc_runs == nullptr) {
        if (prebaked_arc_pixels != nullptr) heap_caps_free(prebaked_arc_pixels);
        if (spatial_arc_pixels != nullptr) heap_caps_free(spatial_arc_pixels);
        if (spatial_arc_runs != nullptr) heap_caps_free(spatial_arc_runs);
        prebaked_arc_pixels = nullptr;
        spatial_arc_pixels = nullptr;
        spatial_arc_runs = nullptr;
        return false;
    }

    uint32_t index = 0;
    uint16_t run_index = 0;
    previous_offset = UINT32_MAX;
    for (int y = 0; y < LCD_HEIGHT; ++y) {
        for (int x = 0; x < LCD_WIDTH; ++x) {
            float dx = x - CENTER_X;
            float dy = y - CENTER_Y;
            float radius = sqrtf(dx * dx + dy * dy);
            if (radius < inner_radius - 0.5f || radius > outer_radius + 0.5f) continue;

            float angle = atan2f(dy, dx) / DEG_TO_RAD;
            if (angle < GAUGE_START_DEG) angle += 360.0f;
            float relative_angle = angle - GAUGE_START_DEG;
            if (relative_angle < 0.0f || relative_angle > GAUGE_SWEEP_DEG) continue;

            float inner_coverage = clampf(radius - (inner_radius - 0.5f), 0.0f, 1.0f);
            float outer_coverage = clampf((outer_radius + 0.5f) - radius, 0.0f, 1.0f);
            uint32_t offset = (uint32_t)(y * LCD_WIDTH + x);
            uint16_t angle_tenths = (uint16_t)lroundf(relative_angle * 10.0f);
            uint8_t opacity =
                (uint8_t)lroundf(inner_coverage * outer_coverage * 255.0f);

            if (index == 0 || offset != previous_offset + 1) {
                spatial_arc_runs[run_index++] = {
                    offset, (uint16_t)index, 1
                };
            } else {
                ++spatial_arc_runs[run_index - 1].length;
            }
            previous_offset = offset;
            spatial_arc_pixels[index] = {angle_tenths, opacity};
            prebaked_arc_pixels[index] = {offset, angle_tenths, opacity};
            ++index;
        }
    }

    prebaked_arc_pixel_count = index;
    spatial_arc_run_count = run_index;
    qsort(prebaked_arc_pixels, prebaked_arc_pixel_count, sizeof(ArcPixel),
          [](const void *left, const void *right) {
              const ArcPixel *a = (const ArcPixel *)left;
              const ArcPixel *b = (const ArcPixel *)right;
              if (a->angle_tenths < b->angle_tenths) return -1;
              if (a->angle_tenths > b->angle_tenths) return 1;
              return 0;
          });
    prepare_prebaked_arc_areas();
    return true;
}

uint32_t arc_pixel_upper_bound(int angle_tenths)
{
    uint32_t low = 0;
    uint32_t high = prebaked_arc_pixel_count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        if (prebaked_arc_pixels[middle].angle_tenths <= angle_tenths) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low;
}

void compose_arc_pixel(const ArcPixel &arc_pixel, int end_tenths, lv_color_t color)
{
    if (end_tenths > 0 && arc_pixel.angle_tenths <= end_tenths) {
        if (arc_pixel.opacity == 255) {
            composited_frame[arc_pixel.offset] = color;
        } else {
            composited_frame[arc_pixel.offset] = lv_color_mix(
                color, static_frame[arc_pixel.offset], arc_pixel.opacity);
        }
    } else {
        composited_frame[arc_pixel.offset] = static_frame[arc_pixel.offset];
    }
}

void compose_spatial_arc(int end_tenths, lv_color_t color, bool restore_hidden)
{
    for (uint16_t run_index = 0; run_index < spatial_arc_run_count; ++run_index) {
        const ArcPixelRun &run = spatial_arc_runs[run_index];
        lv_color_t *destination = composited_frame + run.offset;
        const lv_color_t *background = static_frame + run.offset;
        const SpatialArcPixel *metadata = spatial_arc_pixels + run.first_pixel;

        for (uint16_t pixel = 0; pixel < run.length; ++pixel) {
            if (end_tenths > 0 && metadata[pixel].angle_tenths <= end_tenths) {
                uint8_t opacity = metadata[pixel].opacity;
                destination[pixel] = opacity == 255
                    ? color
                    : lv_color_mix(color, background[pixel], opacity);
            } else if (restore_hidden) {
                destination[pixel] = background[pixel];
            }
        }
    }
}

void restore_arc_under_cursor(const lv_area_t &area, int cursor_angle_tenths,
                              int end_tenths, lv_color_t color)
{
    if (!area_is_valid(area)) return;

    const int angular_margin = 150;
    uint32_t first = arc_pixel_upper_bound(cursor_angle_tenths - angular_margin - 1);
    uint32_t last = arc_pixel_upper_bound(cursor_angle_tenths + angular_margin);
    for (uint32_t i = first; i < last; ++i) {
        const ArcPixel &arc_pixel = prebaked_arc_pixels[i];
        int y = arc_pixel.offset / LCD_WIDTH;
        int x = arc_pixel.offset - y * LCD_WIDTH;
        if (x >= area.x1 && x <= area.x2 && y >= area.y1 && y <= area.y2) {
            compose_arc_pixel(arc_pixel, end_tenths, color);
        }
    }
}

void blend_composited_pixel(int x, int y, lv_color_t color, uint8_t opacity)
{
    if ((unsigned)x >= LCD_WIDTH || (unsigned)y >= LCD_HEIGHT || opacity == 0) return;
    lv_color_t &pixel = composited_frame[y * LCD_WIDTH + x];
    pixel = opacity == 255 ? color : lv_color_mix(color, pixel, opacity);
}

lv_area_t draw_prebaked_cursor(const lv_point_t points[2])
{
    const float outline_radius = 9.0f;
    const float cursor_radius = 4.0f;
    const float outline_solid_sq =
        (outline_radius - 0.5f) * (outline_radius - 0.5f);
    const float outline_outer_sq =
        (outline_radius + 0.5f) * (outline_radius + 0.5f);
    const float cursor_solid_sq =
        (cursor_radius - 0.5f) * (cursor_radius - 0.5f);
    const float cursor_outer_sq =
        (cursor_radius + 0.5f) * (cursor_radius + 0.5f);
    const int margin = (int)ceilf(outline_radius) + 1;
    lv_area_t area = {
        (lv_coord_t)(LV_MIN(points[0].x, points[1].x) - margin),
        (lv_coord_t)(LV_MIN(points[0].y, points[1].y) - margin),
        (lv_coord_t)(LV_MAX(points[0].x, points[1].x) + margin),
        (lv_coord_t)(LV_MAX(points[0].y, points[1].y) + margin)
    };

    float vx = points[1].x - points[0].x;
    float vy = points[1].y - points[0].y;
    float length_squared = vx * vx + vy * vy;

    for (int y = area.y1; y <= area.y2; ++y) {
        for (int x = area.x1; x <= area.x2; ++x) {
            float wx = x - points[0].x;
            float wy = y - points[0].y;
            float t = length_squared > 0.0f ? (wx * vx + wy * vy) / length_squared : 0.0f;
            t = clampf(t, 0.0f, 1.0f);
            float closest_x = points[0].x + t * vx;
            float closest_y = points[0].y + t * vy;
            float dx = x - closest_x;
            float dy = y - closest_y;
            float distance_sq = dx * dx + dy * dy;
            if (distance_sq >= outline_outer_sq) continue;

            float outline_coverage = 1.0f;
            float cursor_coverage = 0.0f;
            if (distance_sq > outline_solid_sq) {
                outline_coverage = outline_radius + 0.5f - sqrtf(distance_sq);
            }
            if (distance_sq <= cursor_solid_sq) {
                cursor_coverage = 1.0f;
            } else if (distance_sq < cursor_outer_sq) {
                cursor_coverage = cursor_radius + 0.5f - sqrtf(distance_sq);
            }
            blend_composited_pixel(
                x, y, lv_color_black(),
                (uint8_t)lroundf(outline_coverage * 255.0f));
            blend_composited_pixel(
                x, y, lv_palette_main(LV_PALETTE_RED),
                (uint8_t)lroundf(cursor_coverage * 255.0f));
        }
    }

    return area;
}

float psi_for_gauge_angle_tenths(int angle_tenths)
{
    const int zero_tenths = (int)lroundf((GAUGE_ZERO_DEG - GAUGE_START_DEG) * 10.0f);
    if (angle_tenths <= zero_tenths) {
        return mapf((float)angle_tenths, 0.0f, (float)zero_tenths,
                    BOOST_MIN_PSI, 0.0f);
    }

    return mapf((float)angle_tenths, (float)zero_tenths,
                GAUGE_SWEEP_DEG * 10.0f, 0.0f, BOOST_MAX_PSI);
}

void calculate_baked_cursor_geometry(int angle_tenths, lv_point_t points[2],
                                     lv_area_t &area)
{
    const float angle_rad =
        (GAUGE_START_DEG + angle_tenths * 0.1f) * DEG_TO_RAD;
    const float angle_cos = cosf(angle_rad);
    const float angle_sin = sinf(angle_rad);
    points[0] = {
        (lv_coord_t)(CENTER_X + lroundf(angle_cos * CURSOR_INNER_RADIUS)),
        (lv_coord_t)(CENTER_Y + lroundf(angle_sin * CURSOR_INNER_RADIUS))
    };
    points[1] = {
        (lv_coord_t)(CENTER_X + lroundf(angle_cos * CURSOR_OUTER_RADIUS)),
        (lv_coord_t)(CENTER_Y + lroundf(angle_sin * CURSOR_OUTER_RADIUS))
    };

    const int margin = 10;
    area = {
        (lv_coord_t)(LV_MIN(points[0].x, points[1].x) - margin),
        (lv_coord_t)(LV_MIN(points[0].y, points[1].y) - margin),
        (lv_coord_t)(LV_MAX(points[0].x, points[1].x) + margin),
        (lv_coord_t)(LV_MAX(points[0].y, points[1].y) + margin)
    };
}

bool baked_cursor_pixel_color(int x, int y, const lv_point_t points[2],
                              lv_color_t underlying, lv_color_t &result)
{
    const float outline_radius = 9.0f;
    const float cursor_radius = 4.0f;
    const float outline_solid_sq = 72.25f;
    const float outline_outer_sq = 90.25f;
    const float cursor_solid_sq = 12.25f;
    const float cursor_outer_sq = 20.25f;
    const float vx = points[1].x - points[0].x;
    const float vy = points[1].y - points[0].y;
    const float length_squared = vx * vx + vy * vy;
    const float wx = x - points[0].x;
    const float wy = y - points[0].y;
    float t = length_squared > 0.0f
        ? (wx * vx + wy * vy) / length_squared
        : 0.0f;
    t = clampf(t, 0.0f, 1.0f);
    const float dx = x - (points[0].x + t * vx);
    const float dy = y - (points[0].y + t * vy);
    const float distance_sq = dx * dx + dy * dy;
    if (distance_sq >= outline_outer_sq) return false;

    float outline_coverage = 1.0f;
    float cursor_coverage = 0.0f;
    if (distance_sq > outline_solid_sq) {
        outline_coverage = outline_radius + 0.5f - sqrtf(distance_sq);
    }
    if (distance_sq <= cursor_solid_sq) {
        cursor_coverage = 1.0f;
    } else if (distance_sq < cursor_outer_sq) {
        cursor_coverage = cursor_radius + 0.5f - sqrtf(distance_sq);
    }

    result = lv_color_mix(
        lv_color_black(), underlying,
        (uint8_t)lroundf(outline_coverage * 255.0f));
    if (cursor_coverage > 0.0f) {
        result = lv_color_mix(
            lv_palette_main(LV_PALETTE_RED), result,
            (uint8_t)lroundf(cursor_coverage * 255.0f));
    }
    return true;
}

lv_color_t baked_arc_pixel_color(uint32_t offset,
                                 const SpatialArcPixel &metadata,
                                 int end_tenths, lv_color_t color)
{
    if (end_tenths > 0 && metadata.angle_tenths <= end_tenths) {
        return metadata.opacity == 255
            ? color
            : lv_color_mix(color, static_frame[offset], metadata.opacity);
    }
    return static_frame[offset];
}

void fill_frame_color(lv_color_t *destination, uint16_t length, uint16_t color)
{
    if (length == 0) return;
    if (((uintptr_t)destination & 0x2U) != 0U) {
        destination++->full = color;
        --length;
    }

    const uint32_t packed = (uint32_t)color | ((uint32_t)color << 16);
    uint32_t *wide_destination = (uint32_t *)destination;
    while (length >= 2) {
        *wide_destination++ = packed;
        length -= 2;
    }
    if (length != 0) ((lv_color_t *)wide_destination)->full = color;
}

void apply_baked_arc_state(uint16_t state_index)
{
    const BakedGaugeState &state = baked_gauge_states[state_index];
    uint16_t spatial_run = 0;
    uint16_t used_in_run = 0;

    for (uint16_t command_index = 0;
         command_index < state.arc_command_count; ++command_index) {
        const BakedArcCommand &command =
            baked_arc_commands[state.first_arc_command + command_index];
        uint16_t remaining = command.length;

        while (remaining != 0) {
            const ArcPixelRun &run = spatial_arc_runs[spatial_run];
            const uint16_t available = run.length - used_in_run;
            const uint16_t chunk = LV_MIN(remaining, available);
            fill_frame_color(composited_frame + run.offset + used_in_run,
                             chunk, command.color);
            remaining -= chunk;
            used_in_run += chunk;
            if (used_in_run == run.length) {
                ++spatial_run;
                used_in_run = 0;
            }
        }
    }
}

lv_area_t apply_baked_cursor_state(uint16_t state_index)
{
    const BakedGaugeState &state = baked_gauge_states[state_index];
    const int width = state.cursor_area.x2 - state.cursor_area.x1 + 1;
    for (uint16_t index = 0; index < state.cursor_pixel_count; ++index) {
        const BakedCursorPixel &pixel =
            baked_cursor_pixels[state.first_cursor_pixel + index];
        const int relative_y = pixel.relative_offset / width;
        const int relative_x = pixel.relative_offset - relative_y * width;
        const int x = state.cursor_area.x1 + relative_x;
        const int y = state.cursor_area.y1 + relative_y;
        composited_frame[y * LCD_WIDTH + x].full = pixel.color;
    }
    return state.cursor_area;
}

bool load_prebaked_gauge_cache()
{
    if (PREBAKED_GAUGE_CACHE_RAW_SIZE < sizeof(PrebakedCacheHeader)) {
        return false;
    }

    prebaked_cache_storage = static_cast<uint8_t *>(heap_caps_malloc(
        PREBAKED_GAUGE_CACHE_RAW_SIZE,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (prebaked_cache_storage == nullptr) return false;

    tinfl_decompressor *decompressor =
        static_cast<tinfl_decompressor *>(heap_caps_malloc(
            sizeof(tinfl_decompressor),
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (decompressor == nullptr) {
        heap_caps_free(prebaked_cache_storage);
        prebaked_cache_storage = nullptr;
        return false;
    }

    tinfl_init(decompressor);
    size_t compressed_size = PREBAKED_GAUGE_CACHE_ZLIB_SIZE;
    size_t uncompressed_size = PREBAKED_GAUGE_CACHE_RAW_SIZE;
    const tinfl_status status = tinfl_decompress(
        decompressor,
        PREBAKED_GAUGE_CACHE_ZLIB,
        &compressed_size,
        prebaked_cache_storage,
        prebaked_cache_storage,
        &uncompressed_size,
        TINFL_FLAG_PARSE_ZLIB_HEADER |
            TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF |
            TINFL_FLAG_COMPUTE_ADLER32);
    heap_caps_free(decompressor);
    if (status != TINFL_STATUS_DONE ||
        uncompressed_size != PREBAKED_GAUGE_CACHE_RAW_SIZE) {
        heap_caps_free(prebaked_cache_storage);
        prebaked_cache_storage = nullptr;
        return false;
    }

    const PrebakedCacheHeader &header =
        *reinterpret_cast<const PrebakedCacheHeader *>(prebaked_cache_storage);
    const bool header_valid =
        header.magic == PREBAKED_CACHE_MAGIC &&
        header.format_version == PREBAKED_CACHE_FORMAT_VERSION &&
        header.total_size == PREBAKED_GAUGE_CACHE_RAW_SIZE &&
        header.state_count == BAKED_GAUGE_STATE_COUNT &&
        header.state_size == sizeof(BakedGaugeState) &&
        header.tile_mask_bytes == BAKED_TILE_MASK_BYTES &&
        header.arc_command_size == sizeof(BakedArcCommand) &&
        header.cursor_pixel_size == sizeof(BakedCursorPixel) &&
        header.spatial_run_size == sizeof(ArcPixelRun) &&
        header.spatial_run_count <= UINT16_MAX;
    const mz_ulong payload_crc = mz_crc32(
        MZ_CRC32_INIT,
        prebaked_cache_storage + sizeof(PrebakedCacheHeader),
        PREBAKED_GAUGE_CACHE_RAW_SIZE - sizeof(PrebakedCacheHeader));
    if (!header_valid || payload_crc != header.payload_crc32) {
        heap_caps_free(prebaked_cache_storage);
        prebaked_cache_storage = nullptr;
        return false;
    }

    size_t offset = sizeof(PrebakedCacheHeader);
    const auto section = [&](size_t bytes) -> uint8_t * {
        offset = prebaked_cache_align4(offset);
        if (bytes > PREBAKED_GAUGE_CACHE_RAW_SIZE - offset) return nullptr;
        uint8_t *result = prebaked_cache_storage + offset;
        offset += bytes;
        return result;
    };

    baked_gauge_states = reinterpret_cast<BakedGaugeState *>(section(
        header.state_count * header.state_size));
    baked_arc_tile_masks = section(
        header.state_count * header.tile_mask_bytes);
    baked_arc_commands = reinterpret_cast<BakedArcCommand *>(section(
        header.arc_command_count * header.arc_command_size));
    baked_cursor_pixels = reinterpret_cast<BakedCursorPixel *>(section(
        header.cursor_pixel_count * header.cursor_pixel_size));
    spatial_arc_runs = reinterpret_cast<ArcPixelRun *>(section(
        header.spatial_run_count * header.spatial_run_size));

    if (baked_gauge_states == nullptr || baked_arc_tile_masks == nullptr ||
        baked_arc_commands == nullptr || baked_cursor_pixels == nullptr ||
        spatial_arc_runs == nullptr || offset != header.total_size) {
        heap_caps_free(prebaked_cache_storage);
        prebaked_cache_storage = nullptr;
        baked_gauge_states = nullptr;
        baked_arc_tile_masks = nullptr;
        baked_arc_commands = nullptr;
        baked_cursor_pixels = nullptr;
        spatial_arc_runs = nullptr;
        return false;
    }

    baked_arc_command_count = header.arc_command_count;
    baked_cursor_pixel_count = header.cursor_pixel_count;
    spatial_arc_run_count = static_cast<uint16_t>(header.spatial_run_count);
    prebaked_arc_pixel_count = header.arc_pixel_count;
    return true;
}

bool prepare_baked_gauge_states()
{
    baked_gauge_states = (BakedGaugeState *)heap_caps_calloc(
        BAKED_GAUGE_STATE_COUNT, sizeof(BakedGaugeState),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    baked_arc_tile_masks = (uint8_t *)heap_caps_calloc(
        BAKED_GAUGE_STATE_COUNT, BAKED_TILE_MASK_BYTES,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (baked_gauge_states == nullptr || baked_arc_tile_masks == nullptr) {
        if (baked_gauge_states != nullptr) heap_caps_free(baked_gauge_states);
        if (baked_arc_tile_masks != nullptr) heap_caps_free(baked_arc_tile_masks);
        baked_gauge_states = nullptr;
        baked_arc_tile_masks = nullptr;
        return false;
    }

    uint32_t total_arc_commands = 0;
    uint32_t total_cursor_pixels = 0;
    for (int state_index = 0; state_index < BAKED_GAUGE_STATE_COUNT; ++state_index) {
        BakedGaugeState &state = baked_gauge_states[state_index];
        const int end_tenths = state_index * BAKED_GAUGE_STEP_TENTHS;
        const lv_color_t color = boost_color(psi_for_gauge_angle_tenths(end_tenths));
        state.color = color.full;
        state.first_arc_command = total_arc_commands;
        state.first_cursor_pixel = total_cursor_pixels;
        uint8_t *tile_mask =
            baked_arc_tile_masks + state_index * BAKED_TILE_MASK_BYTES;

        bool first_color = true;
        uint16_t previous_color = 0;
        for (uint16_t run_index = 0; run_index < spatial_arc_run_count; ++run_index) {
            const ArcPixelRun &run = spatial_arc_runs[run_index];
            const SpatialArcPixel *metadata = spatial_arc_pixels + run.first_pixel;
            const int run_y = run.offset / LCD_WIDTH;
            const int run_x = run.offset - run_y * LCD_WIDTH;
            const int tile_row = (run_y / BAKED_TILE_SIZE) * BAKED_TILE_COLUMNS;
            for (uint16_t pixel = 0; pixel < run.length; ++pixel) {
                const uint16_t next_color = baked_arc_pixel_color(
                    run.offset + pixel, metadata[pixel], end_tenths, color).full;
                if (first_color || next_color != previous_color) {
                    ++state.arc_command_count;
                    previous_color = next_color;
                    first_color = false;
                }
                if (end_tenths > 0 && metadata[pixel].angle_tenths <= end_tenths) {
                    const int tile = tile_row +
                        (run_x + pixel) / BAKED_TILE_SIZE;
                    tile_mask[tile >> 3] |= (uint8_t)(1U << (tile & 7));
                }
            }
        }
        total_arc_commands += state.arc_command_count;

        lv_point_t points[2];
        calculate_baked_cursor_geometry(end_tenths, points, state.cursor_area);
        const int min_x = LV_MAX(0, state.cursor_area.x1);
        const int min_y = LV_MAX(0, state.cursor_area.y1);
        const int max_x = LV_MIN(LCD_WIDTH - 1, state.cursor_area.x2);
        const int max_y = LV_MIN(LCD_HEIGHT - 1, state.cursor_area.y2);
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                lv_color_t ignored;
                if (baked_cursor_pixel_color(x, y, points, lv_color_black(), ignored)) {
                    ++state.cursor_pixel_count;
                }
            }
        }
        total_cursor_pixels += state.cursor_pixel_count;

        state.arc_delta_area = {0, 0, -1, -1};
        const int previous_end = state_index == 0
            ? end_tenths
            : end_tenths - BAKED_GAUGE_STEP_TENTHS;
        const uint32_t first = state_index == 0
            ? 0
            : arc_pixel_upper_bound(previous_end);
        const uint32_t last = arc_pixel_upper_bound(end_tenths);
        for (uint32_t pixel = first; pixel < last; ++pixel) {
            const uint32_t offset = prebaked_arc_pixels[pixel].offset;
            const int y = offset / LCD_WIDTH;
            const int x = offset - y * LCD_WIDTH;
            include_dirty_area(state.arc_delta_area, {
                (lv_coord_t)x, (lv_coord_t)y, (lv_coord_t)x, (lv_coord_t)y
            });
        }
    }

    baked_arc_commands = (BakedArcCommand *)heap_caps_malloc(
        total_arc_commands * sizeof(BakedArcCommand),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    baked_cursor_pixels = (BakedCursorPixel *)heap_caps_malloc(
        total_cursor_pixels * sizeof(BakedCursorPixel),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (baked_arc_commands == nullptr || baked_cursor_pixels == nullptr) {
        if (baked_arc_commands != nullptr) heap_caps_free(baked_arc_commands);
        if (baked_cursor_pixels != nullptr) heap_caps_free(baked_cursor_pixels);
        heap_caps_free(baked_gauge_states);
        heap_caps_free(baked_arc_tile_masks);
        baked_gauge_states = nullptr;
        baked_arc_commands = nullptr;
        baked_cursor_pixels = nullptr;
        baked_arc_tile_masks = nullptr;
        return false;
    }

    uint32_t command_write = 0;
    for (int state_index = 0; state_index < BAKED_GAUGE_STATE_COUNT; ++state_index) {
        const BakedGaugeState &state = baked_gauge_states[state_index];
        const int end_tenths = state_index * BAKED_GAUGE_STEP_TENTHS;
        const lv_color_t color = {.full = state.color};
        bool first_color = true;
        uint16_t previous_color = 0;

        for (uint16_t run_index = 0; run_index < spatial_arc_run_count; ++run_index) {
            const ArcPixelRun &run = spatial_arc_runs[run_index];
            const SpatialArcPixel *metadata = spatial_arc_pixels + run.first_pixel;
            for (uint16_t pixel = 0; pixel < run.length; ++pixel) {
                const uint16_t next_color = baked_arc_pixel_color(
                    run.offset + pixel, metadata[pixel], end_tenths, color).full;
                if (first_color || next_color != previous_color) {
                    baked_arc_commands[command_write++] = {1, next_color};
                    previous_color = next_color;
                    first_color = false;
                } else {
                    ++baked_arc_commands[command_write - 1].length;
                }
            }
        }
    }

    uint32_t cursor_write = 0;
    for (int state_index = 0; state_index < BAKED_GAUGE_STATE_COUNT; ++state_index) {
        apply_baked_arc_state((uint16_t)state_index);

        lv_point_t points[2];
        lv_area_t cursor_area;
        calculate_baked_cursor_geometry(
            state_index * BAKED_GAUGE_STEP_TENTHS, points, cursor_area);
        const int width = cursor_area.x2 - cursor_area.x1 + 1;
        const int min_x = LV_MAX(0, cursor_area.x1);
        const int min_y = LV_MAX(0, cursor_area.y1);
        const int max_x = LV_MIN(LCD_WIDTH - 1, cursor_area.x2);
        const int max_y = LV_MIN(LCD_HEIGHT - 1, cursor_area.y2);
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                lv_color_t result;
                if (!baked_cursor_pixel_color(
                        x, y, points, composited_frame[y * LCD_WIDTH + x], result)) {
                    continue;
                }
                const uint16_t relative_offset = (uint16_t)(
                    (y - cursor_area.y1) * width + (x - cursor_area.x1));
                baked_cursor_pixels[cursor_write++] = {
                    relative_offset, result.full
                };
            }
        }
    }

    baked_arc_command_count = command_write;
    baked_cursor_pixel_count = cursor_write;
    memcpy(composited_frame, static_frame,
           LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t));
    const bool cache_complete = command_write == total_arc_commands &&
                                cursor_write == total_cursor_pixels;
    if (cache_complete) {
        heap_caps_free(prebaked_arc_pixels);
        heap_caps_free(spatial_arc_pixels);
        prebaked_arc_pixels = nullptr;
        spatial_arc_pixels = nullptr;
    }
    return cache_complete;
}

#if DUMP_BAKED_CACHE
void write_cache_bytes(const void *source, size_t length)
{
    const uint8_t *bytes = static_cast<const uint8_t *>(source);
    size_t written = 0;
    while (written < length) {
        const size_t chunk = LV_MIN(length - written, size_t(16384));
        const size_t result = Serial.write(bytes + written, chunk);
        if (result == 0) {
            delay(1);
            continue;
        }
        written += result;
        yield();
    }
}

void dump_baked_cache()
{
    const size_t state_bytes =
        BAKED_GAUGE_STATE_COUNT * sizeof(BakedGaugeState);
    const size_t tile_mask_bytes =
        BAKED_GAUGE_STATE_COUNT * BAKED_TILE_MASK_BYTES;
    const size_t arc_command_bytes =
        baked_arc_command_count * sizeof(BakedArcCommand);
    const size_t cursor_pixel_bytes =
        baked_cursor_pixel_count * sizeof(BakedCursorPixel);
    const size_t spatial_run_bytes =
        spatial_arc_run_count * sizeof(ArcPixelRun);

    size_t total_size = sizeof(PrebakedCacheHeader);
    const auto include_section = [&total_size](size_t bytes) {
        total_size = prebaked_cache_align4(total_size);
        total_size += bytes;
    };
    include_section(state_bytes);
    include_section(tile_mask_bytes);
    include_section(arc_command_bytes);
    include_section(cursor_pixel_bytes);
    include_section(spatial_run_bytes);

    const uint8_t zero_padding[4] = {};
    mz_ulong crc = MZ_CRC32_INIT;
    size_t crc_offset = sizeof(PrebakedCacheHeader);
    const auto crc_section = [&](const void *data, size_t bytes) {
        const size_t aligned = prebaked_cache_align4(crc_offset);
        const size_t padding = aligned - crc_offset;
        if (padding != 0) {
            crc = mz_crc32(crc, zero_padding, padding);
            crc_offset += padding;
        }
        crc = mz_crc32(
            crc, static_cast<const unsigned char *>(data), bytes);
        crc_offset += bytes;
    };
    crc_section(baked_gauge_states, state_bytes);
    crc_section(baked_arc_tile_masks, tile_mask_bytes);
    crc_section(baked_arc_commands, arc_command_bytes);
    crc_section(baked_cursor_pixels, cursor_pixel_bytes);
    crc_section(spatial_arc_runs, spatial_run_bytes);

    PrebakedCacheHeader header = {
        PREBAKED_CACHE_MAGIC,
        PREBAKED_CACHE_FORMAT_VERSION,
        static_cast<uint32_t>(total_size),
        static_cast<uint32_t>(crc),
        BAKED_GAUGE_STATE_COUNT,
        sizeof(BakedGaugeState),
        BAKED_TILE_MASK_BYTES,
        baked_arc_command_count,
        sizeof(BakedArcCommand),
        baked_cursor_pixel_count,
        sizeof(BakedCursorPixel),
        spatial_arc_run_count,
        sizeof(ArcPixelRun),
        prebaked_arc_pixel_count,
    };

    Serial.printf("BGCACHE %lu\n", (unsigned long)total_size);
    Serial.flush();
    write_cache_bytes(&header, sizeof(header));

    size_t write_offset = sizeof(header);
    const auto write_section = [&](const void *data, size_t bytes) {
        const size_t aligned = prebaked_cache_align4(write_offset);
        const size_t padding = aligned - write_offset;
        if (padding != 0) {
            write_cache_bytes(zero_padding, padding);
            write_offset += padding;
        }
        write_cache_bytes(data, bytes);
        write_offset += bytes;
    };
    write_section(baked_gauge_states, state_bytes);
    write_section(baked_arc_tile_masks, tile_mask_bytes);
    write_section(baked_arc_commands, arc_command_bytes);
    write_section(baked_cursor_pixels, cursor_pixel_bytes);
    write_section(spatial_arc_runs, spatial_run_bytes);
    Serial.print("\nENDBGCACHE\n");
    Serial.flush();
}
#endif

// =========================
// Display flush
// =========================

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
#if ENABLE_RENDER_DIAGNOSTICS
    ++diagnostic_flush_count;
    diagnostic_flush_pixels +=
        (uint32_t)(area->x2 - area->x1 + 1) *
        (uint32_t)(area->y2 - area->y1 + 1);
#endif
    copy_area_to_frame(composited_frame, area, color_p);
    if (!startup_splash_visible) flush_composited_area(*area);

    lv_disp_flush_ready(disp);
}

#if ENABLE_PERF_TELEMETRY
void print_performance_stats(uint32_t interval_ms)
{
    uint32_t avg_flush_us = perf_stats.flush_count
        ? (uint32_t)(perf_stats.flush_time_us / perf_stats.flush_count)
        : 0;
    uint32_t avg_lvgl_us = perf_stats.lvgl_count
        ? (uint32_t)(perf_stats.lvgl_time_us / perf_stats.lvgl_count)
        : 0;
    uint32_t avg_custom_us = perf_stats.custom_count
        ? (uint32_t)(perf_stats.custom_time_us / perf_stats.custom_count)
        : 0;
    uint32_t qspi_busy_pct = interval_ms
        ? (uint32_t)((perf_stats.flush_time_us * 100ULL) / ((uint64_t)interval_ms * 1000ULL))
        : 0;
    uint32_t display_pct = interval_ms
        ? (uint32_t)((perf_stats.display_time_us * 100ULL) / ((uint64_t)interval_ms * 1000ULL))
        : 0;

    esp_rom_printf(
        "[perf] lvgl=%lu/%lu us, custom=%lu/%lu us, flush=%lu/%lu us, "
        "flushes=%lu, pixels=%lu, flush_busy=%lu%%, display=%lu%%\n",
        (unsigned long)avg_lvgl_us,
        (unsigned long)perf_stats.max_lvgl_us,
        (unsigned long)avg_custom_us,
        (unsigned long)perf_stats.max_custom_us,
        (unsigned long)avg_flush_us,
        (unsigned long)perf_stats.max_flush_us,
        (unsigned long)perf_stats.flush_count,
        (unsigned long)perf_stats.flushed_pixels,
        (unsigned long)qspi_busy_pct,
        (unsigned long)display_pct);

    perf_stats = {};
}
#endif

// =========================
// Touch read
// =========================

void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    (void)indev_driver;
    int fingers = (int)FT3168->IIC_Read_Device_Value(
        FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_FINGER_NUMBER);

    if (fingers > 0) {
        int touch_x = (int)FT3168->IIC_Read_Device_Value(
            FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
        int touch_y = (int)FT3168->IIC_Read_Device_Value(
            FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touch_x;
        data->point.y = touch_y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// =========================
// Crear UI
// =========================

lv_color_t boost_color(float psi_value)
{
    const lv_color_t blue = lv_palette_main(LV_PALETTE_BLUE);
    const lv_color_t green = lv_palette_main(LV_PALETTE_GREEN);
    const lv_color_t yellow = lv_palette_main(LV_PALETTE_YELLOW);
    const lv_color_t red = lv_palette_main(LV_PALETTE_RED);

    const float transition_width = 4.0f;
    const float half_width = transition_width * 0.5f;
    float transition_start;
    float blend;

    if (psi_value < -half_width) return blue;
    if (psi_value < half_width) {
        transition_start = -half_width;
        blend = clampf((psi_value - transition_start) / transition_width, 0.0f, 1.0f);
        blend = blend * blend * (3.0f - 2.0f * blend);
        return lv_color_mix(green, blue, (uint8_t)(blend * 255.0f));
    }

    if (psi_value < 12.0f) return green;
    if (psi_value < 15.0f) {
        blend = clampf((psi_value - 12.0f) / 3.0f, 0.0f, 1.0f);
        blend = blend * blend * (3.0f - 2.0f * blend);
        return lv_color_mix(yellow, green, (uint8_t)(blend * 255.0f));
    }

    if (psi_value < 20.0f) return yellow;
    if (psi_value < 26.0f) {
        blend = clampf((psi_value - 20.0f) / 6.0f, 0.0f, 1.0f);
        blend = blend * blend * (3.0f - 2.0f * blend);
        return lv_color_mix(red, yellow, (uint8_t)(blend * 255.0f));
    }

    return red;
}

void include_dirty_area(lv_area_t &destination, const lv_area_t &source)
{
    if (!area_is_valid(source)) return;
    if (!area_is_valid(destination)) {
        destination = source;
        return;
    }

    destination.x1 = LV_MIN(destination.x1, source.x1);
    destination.y1 = LV_MIN(destination.y1, source.y1);
    destination.x2 = LV_MAX(destination.x2, source.x2);
    destination.y2 = LV_MAX(destination.y2, source.y2);
}

void mark_baked_tiles_for_area(uint8_t *tile_mask, const lv_area_t &requested_area)
{
    if (!area_is_valid(requested_area)) return;
    const int x1 = LV_MAX(0, requested_area.x1) / BAKED_TILE_SIZE;
    const int y1 = LV_MAX(0, requested_area.y1) / BAKED_TILE_SIZE;
    const int x2 = LV_MIN(LCD_WIDTH - 1, requested_area.x2) / BAKED_TILE_SIZE;
    const int y2 = LV_MIN(LCD_HEIGHT - 1, requested_area.y2) / BAKED_TILE_SIZE;
    if (x1 > x2 || y1 > y2) return;

    for (int tile_y = y1; tile_y <= y2; ++tile_y) {
        for (int tile_x = x1; tile_x <= x2; ++tile_x) {
            const int tile = tile_y * BAKED_TILE_COLUMNS + tile_x;
            tile_mask[tile >> 3] |= (uint8_t)(1U << (tile & 7));
        }
    }
}

void flush_baked_tile_mask(const uint8_t *tile_mask)
{
    for (int tile_y = 0; tile_y < BAKED_TILE_ROWS; ++tile_y) {
        int tile_x = 0;
        while (tile_x < BAKED_TILE_COLUMNS) {
            int tile = tile_y * BAKED_TILE_COLUMNS + tile_x;
            if ((tile_mask[tile >> 3] & (1U << (tile & 7))) == 0) {
                ++tile_x;
                continue;
            }

            const int first_x = tile_x;
            do {
                ++tile_x;
                tile = tile_y * BAKED_TILE_COLUMNS + tile_x;
            } while (tile_x < BAKED_TILE_COLUMNS &&
                     (tile_mask[tile >> 3] & (1U << (tile & 7))) != 0);

            flush_composited_area({
                (lv_coord_t)(first_x * BAKED_TILE_SIZE),
                (lv_coord_t)(tile_y * BAKED_TILE_SIZE),
                (lv_coord_t)LV_MIN(LCD_WIDTH - 1,
                    tile_x * BAKED_TILE_SIZE - 1),
                (lv_coord_t)LV_MIN(LCD_HEIGHT - 1,
                    (tile_y + 1) * BAKED_TILE_SIZE - 1)
            });
        }
    }
}

void render_prebaked_gauge(float psi_value, bool update_color)
{
    static int previous_state_index = -1;
    static uint16_t rendered_color = 0;
    static bool color_initialized = false;

    if (!prebaked_enabled || brightness_menu_open) return;
#if ENABLE_PERF_TELEMETRY
    uint32_t render_started_us = micros();
    uint32_t color_compose_us = 0;
#endif

    int end_tenths = (int)lroundf(
        (gauge_angle_for_psi(psi_value) - GAUGE_START_DEG) * 10.0f);
    end_tenths = LV_MAX(0, LV_MIN((int)(GAUGE_SWEEP_DEG * 10.0f), end_tenths));
    const int state_index = LV_MIN(
        BAKED_GAUGE_STATE_COUNT - 1,
        (end_tenths + BAKED_GAUGE_STEP_TENTHS / 2) /
            BAKED_GAUGE_STEP_TENTHS);
    if (!prebaked_force_full && state_index == previous_state_index) {
        if (update_color) flush_composited_area(VALUE_PHYSICAL_AREA);
        return;
    }

    const BakedGaugeState &state = baked_gauge_states[state_index];
    const bool color_changed = !color_initialized || state.color != rendered_color;
    rendered_color = state.color;
    color_initialized = true;

    lv_area_t changed_arc_area = {0, 0, -1, -1};
    restore_frame_area(previous_cursor_area);

#if ENABLE_PERF_TELEMETRY
    uint32_t color_started_us = micros();
#endif
    apply_baked_arc_state((uint16_t)state_index);
#if ENABLE_PERF_TELEMETRY
    color_compose_us = micros() - color_started_us;
#endif

    if (previous_state_index >= 0 && state_index != previous_state_index) {
        const int first_state = LV_MIN(state_index, previous_state_index) + 1;
        const int last_state = LV_MAX(state_index, previous_state_index);
        for (int changed_state = first_state;
             changed_state <= last_state; ++changed_state) {
            include_dirty_area(
                changed_arc_area,
                baked_gauge_states[changed_state].arc_delta_area);
        }
    }

    const lv_area_t next_cursor_area =
        apply_baked_cursor_state((uint16_t)state_index);

    if (!prebaked_force_full && !color_changed) {
        uint8_t dirty_tiles[BAKED_TILE_MASK_BYTES] = {};
        mark_baked_tiles_for_area(dirty_tiles, previous_cursor_area);
        mark_baked_tiles_for_area(dirty_tiles, next_cursor_area);
        mark_baked_tiles_for_area(dirty_tiles, changed_arc_area);
        if (update_color) mark_baked_tiles_for_area(dirty_tiles, VALUE_PHYSICAL_AREA);
        flush_baked_tile_mask(dirty_tiles);
    } else {
        uint8_t dirty_tiles[BAKED_TILE_MASK_BYTES];
        memcpy(dirty_tiles,
               baked_arc_tile_masks + state_index * BAKED_TILE_MASK_BYTES,
               sizeof(dirty_tiles));
        if (previous_state_index >= 0) {
            const uint8_t *previous_tiles = baked_arc_tile_masks +
                previous_state_index * BAKED_TILE_MASK_BYTES;
            for (int index = 0; index < BAKED_TILE_MASK_BYTES; ++index) {
                dirty_tiles[index] |= previous_tiles[index];
            }
        }
        mark_baked_tiles_for_area(dirty_tiles, previous_cursor_area);
        mark_baked_tiles_for_area(dirty_tiles, next_cursor_area);
        if (update_color) mark_baked_tiles_for_area(dirty_tiles, VALUE_PHYSICAL_AREA);
        flush_baked_tile_mask(dirty_tiles);
    }

    previous_cursor_area = next_cursor_area;
    previous_state_index = state_index;
    prebaked_force_full = false;
#if ENABLE_PERF_TELEMETRY
    uint32_t render_elapsed_us = micros() - render_started_us;
    perf_stats.custom_time_us += render_elapsed_us;
    ++perf_stats.custom_count;
    perf_stats.max_custom_us = LV_MAX(perf_stats.max_custom_us, render_elapsed_us);
    if (render_elapsed_us > 12000) {
        esp_rom_printf("[slow] psi_x100=%d total=%lu us color=%lu us changed=%u\n",
                       (int)lroundf(psi_value * 100.0f),
                       (unsigned long)render_elapsed_us,
                       (unsigned long)color_compose_us,
                       color_changed ? 1U : 0U);
    }
#endif
}

void prepare_arc_color_areas()
{
    const float centerline_radius = (GAUGE_DIAMETER / 2.0f) - (ARC_WIDTH / 2.0f);
    const int margin = (ARC_WIDTH / 2) + 2;

    for (int segment = 0; segment < ARC_COLOR_SEGMENT_COUNT; ++segment) {
        int start = segment * ARC_COLOR_SEGMENT_DEG;
        int end = start + ARC_COLOR_SEGMENT_DEG;
        if (end > (int)GAUGE_SWEEP_DEG) end = (int)GAUGE_SWEEP_DEG;

        int min_x = LCD_WIDTH;
        int min_y = LCD_HEIGHT;
        int max_x = 0;
        int max_y = 0;

        for (int offset = start; offset <= end; ++offset) {
            float radians = (GAUGE_START_DEG + offset) * DEG_TO_RAD;
            int x = (int)lroundf(CENTER_X + cosf(radians) * centerline_radius);
            int y = (int)lroundf(CENTER_Y + sinf(radians) * centerline_radius);
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }

        arc_color_areas[segment].x1 = LV_MAX(0, min_x - margin);
        arc_color_areas[segment].y1 = LV_MAX(0, min_y - margin);
        arc_color_areas[segment].x2 = LV_MIN(LCD_WIDTH - 1, max_x + margin);
        arc_color_areas[segment].y2 = LV_MIN(LCD_HEIGHT - 1, max_y + margin);
    }
}

void invalidate_visible_arc_color(int end_angle)
{
    int segment_count = (end_angle + ARC_COLOR_SEGMENT_DEG - 1) / ARC_COLOR_SEGMENT_DEG;
    if (segment_count > ARC_COLOR_SEGMENT_COUNT) segment_count = ARC_COLOR_SEGMENT_COUNT;

    for (int i = 0; i < segment_count; ++i) {
        lv_obj_invalidate_area(arc_fill, &arc_color_areas[i]);
    }
}

void create_arc_fill()
{
    arc_fill = lv_arc_create(lv_scr_act());
    lv_obj_set_size(arc_fill, GAUGE_DIAMETER, GAUGE_DIAMETER);
    lv_obj_set_pos(arc_fill, CENTER_X - GAUGE_DIAMETER / 2, CENTER_Y - GAUGE_DIAMETER / 2);
    lv_arc_set_bg_angles(arc_fill, 0, (int)GAUGE_SWEEP_DEG);
    lv_arc_set_rotation(arc_fill, (int)GAUGE_START_DEG);
    lv_arc_set_range(arc_fill, 0, (int)GAUGE_SWEEP_DEG);
    lv_arc_set_value(arc_fill, 0);
    lv_obj_remove_style(arc_fill, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_fill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_opa(arc_fill, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_style_init(&arc_indicator_style);
    lv_style_set_arc_width(&arc_indicator_style, ARC_WIDTH);
    lv_style_set_arc_color(&arc_indicator_style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_arc_rounded(&arc_indicator_style, false);
    lv_obj_add_style(arc_fill, &arc_indicator_style, LV_PART_INDICATOR);

    prepare_arc_color_areas();
    lv_obj_add_flag(arc_fill, LV_OBJ_FLAG_HIDDEN);
}

void update_arc_reveal(float psi_value, bool update_color)
{
    static int last_end_angle = -1;
    static lv_color_t last_color;
    static bool color_initialized = false;
    static bool arc_visible = false;
    int end_angle = (int)(gauge_angle_for_psi(psi_value) - GAUGE_START_DEG);

    if (end_angle <= 0) {
        if (arc_visible) {
            lv_obj_add_flag(arc_fill, LV_OBJ_FLAG_HIDDEN);
            arc_visible = false;
        }
        last_end_angle = 0;
        return;
    }

    if (end_angle != last_end_angle) {
        lv_arc_set_value(arc_fill, end_angle);
        last_end_angle = end_angle;
    }

    if (update_color || !color_initialized) {
        lv_color_t color = boost_color(psi_value);
        if (!color_initialized || color.full != last_color.full) {
            lv_style_set_arc_color(&arc_indicator_style, color);
            invalidate_visible_arc_color(end_angle);
            last_color = color;
            color_initialized = true;
        }
    }

    if (!arc_visible) {
        lv_obj_clear_flag(arc_fill, LV_OBJ_FLAG_HIDDEN);
        arc_visible = true;
    }
}

void create_ticks()
{
    static const float label_values[BOOST_LABEL_COUNT] = {
        -15.0f, -10.0f, -5.0f, 0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f
    };
    static const char *label_text[BOOST_LABEL_COUNT] = {
        "-15", "-10", "-5", "0", "5", "10", "15", "20", "25", "30"
    };

    for (uint8_t i = 0; i < BOOST_TICK_COUNT; ++i) {
        float value = BOOST_MIN_PSI + i;
        float angle = gauge_angle_for_psi(value);
        float radians = angle * (3.14159265f / 180.0f);
        bool major_tick = (i % 5 == 0) || (i == BOOST_TICK_COUNT - 1);
        int inner_radius = major_tick ? TICK_MAJOR_INNER_RADIUS : TICK_MINOR_INNER_RADIUS;

        tick_points[i][0].x = CENTER_X + (int)(cos(radians) * inner_radius);
        tick_points[i][0].y = CENTER_Y + (int)(sin(radians) * inner_radius);
        tick_points[i][1].x = CENTER_X + (int)(cos(radians) * TICK_OUTER_RADIUS);
        tick_points[i][1].y = CENTER_Y + (int)(sin(radians) * TICK_OUTER_RADIUS);

        tick_lines[i] = lv_line_create(lv_scr_act());
        lv_line_set_points(tick_lines[i], tick_points[i], 2);
        lv_obj_set_style_line_width(tick_lines[i], major_tick ? 4 : 2, 0);
        lv_obj_set_style_line_color(tick_lines[i], boost_color(value), 0);
        lv_obj_set_style_line_rounded(tick_lines[i], true, 0);
    }

    for (uint8_t i = 0; i < BOOST_LABEL_COUNT; ++i) {
        float angle = gauge_angle_for_psi(label_values[i]);
        float radians = angle * (3.14159265f / 180.0f);
        int x = CENTER_X + (int)(cos(radians) * LABEL_RADIUS);
        int y = CENTER_Y + (int)(sin(radians) * LABEL_RADIUS);

        scale_labels[i] = lv_label_create(lv_scr_act());
        lv_label_set_text(scale_labels[i], label_text[i]);
        lv_obj_set_style_text_font(scale_labels[i], &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(scale_labels[i], boost_color(label_values[i]), 0);
        lv_obj_align(scale_labels[i], LV_ALIGN_CENTER, x - LCD_WIDTH / 2, y - LCD_HEIGHT / 2);
        lv_obj_set_style_transform_pivot_x(scale_labels[i], LV_PCT(50), 0);
        lv_obj_set_style_transform_pivot_y(scale_labels[i], LV_PCT(50), 0);
        lv_obj_set_style_transform_angle(scale_labels[i], 900, 0);
    }
}

void update_brightness_label()
{
    int percentage = map(screen_brightness, 10, 255, 4, 100);
    lv_label_set_text_fmt(brightness_label, "BRIGHTNESS %d%%", percentage);
}

void brightness_slider_event(lv_event_t *event)
{
    (void)event;
    screen_brightness = (uint8_t)lv_slider_get_value(brightness_slider);
    gfx->Display_Brightness(screen_brightness);
    update_brightness_label();
}

void brightness_step_event(lv_event_t *event)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(event);
    int value = lv_slider_get_value(brightness_slider) + delta;
    value = value < 10 ? 10 : (value > 255 ? 255 : value);

    lv_slider_set_value(brightness_slider, value, LV_ANIM_OFF);
    screen_brightness = (uint8_t)value;
    gfx->Display_Brightness(screen_brightness);
    update_brightness_label();
}

void zero_feedback_reset(lv_timer_t *timer)
{
    (void)timer;
    lv_label_set_text(zero_calibrate_label, "CALIBRAR 0");
    lv_obj_set_style_text_color(zero_calibrate_label, lv_color_white(), 0);
    zero_feedback_timer = nullptr;
}

void zero_calibrate_event(lv_event_t *event)
{
    (void)event;
    bool calibrated = calibrate_pressure_zero();

    lv_label_set_text(zero_calibrate_label, calibrated ? "OK" : "ERROR");
    lv_obj_set_style_text_color(
        zero_calibrate_label,
        calibrated ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED),
        0);

    if (zero_feedback_timer != nullptr) lv_timer_del(zero_feedback_timer);
    zero_feedback_timer = lv_timer_create(zero_feedback_reset, 1200, nullptr);
    lv_timer_set_repeat_count(zero_feedback_timer, 1);
}

void brightness_close_event(lv_event_t *event)
{
    (void)event;
    brightness_menu_open = false;
    lv_obj_add_flag(brightness_panel, LV_OBJ_FLAG_HIDDEN);
    prebaked_force_full = true;
    gauge_restore_pending = true;
}

void brightness_menu_event(lv_event_t *event)
{
    (void)event;
    if (startup_phase != STARTUP_COMPLETE) return;
    suppress_show_click = true;
    brightness_menu_open = !brightness_menu_open;

    if (brightness_menu_open) {
        lv_obj_clear_flag(brightness_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(brightness_panel);
    } else {
        lv_obj_add_flag(brightness_panel, LV_OBJ_FLAG_HIDDEN);
        prebaked_force_full = true;
        gauge_restore_pending = true;
    }
}

void show_mode_event(lv_event_t *event)
{
    (void)event;
    if (startup_phase != STARTUP_COMPLETE) return;
    if (suppress_show_click) {
        suppress_show_click = false;
        return;
    }

    gauge_mode = (gauge_mode == GAUGE_MODE_LIVE) ? GAUGE_MODE_SHOW : GAUGE_MODE_LIVE;
    show_started_ms = millis();

    if (gauge_mode == GAUGE_MODE_SHOW) {
        Serial.println("Mode: SHOW");
    } else {
        Serial.println("Mode: LIVE");
    }
}

float show_boost_psi(uint32_t now)
{
    float phase = ((now - show_started_ms) % SHOW_CYCLE_MS) / (float)SHOW_CYCLE_MS;
    float triangle = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
    float smooth = triangle * triangle * (3.0f - 2.0f * triangle);
    return mapf(smooth, 0.0f, 1.0f, BOOST_MIN_PSI, BOOST_MAX_PSI);
}

void create_ui()
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(lv_scr_act(), LV_SCROLLBAR_MODE_OFF);

    label_value = nullptr;
    label_unit = nullptr;

    civic_logo_image = lv_obj_create(lv_scr_act());
    lv_obj_set_size(civic_logo_image, 70, 254);
    lv_obj_align(civic_logo_image, LV_ALIGN_CENTER, -192, 0);
    lv_obj_set_style_bg_opa(civic_logo_image, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(civic_logo_image, 0, 0);
    lv_obj_set_style_pad_all(civic_logo_image, 0, 0);
    lv_obj_add_flag(civic_logo_image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(civic_logo_image, show_mode_event, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(civic_logo_image, brightness_menu_event, LV_EVENT_LONG_PRESSED, NULL);

    brightness_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(brightness_panel, 390, 250);
    lv_obj_align(brightness_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_transform_pivot_x(brightness_panel, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(brightness_panel, LV_PCT(50), 0);
    lv_obj_set_style_transform_angle(brightness_panel, 900, 0);
    lv_obj_set_style_bg_color(brightness_panel, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(brightness_panel, LV_OPA_90, 0);
    lv_obj_set_style_border_color(brightness_panel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_border_width(brightness_panel, 2, 0);
    lv_obj_set_style_radius(brightness_panel, 8, 0);
    lv_obj_set_style_pad_all(brightness_panel, 0, 0);
    lv_obj_add_flag(brightness_panel, LV_OBJ_FLAG_HIDDEN);

    brightness_label = lv_label_create(brightness_panel);
    lv_obj_set_style_text_color(brightness_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_16, 0);
    lv_obj_align(brightness_label, LV_ALIGN_TOP_LEFT, 18, 18);

    brightness_slider = lv_slider_create(brightness_panel);
    lv_obj_set_size(brightness_slider, 190, 22);
    lv_obj_align(brightness_slider, LV_ALIGN_CENTER, 0, 25);
    lv_slider_set_range(brightness_slider, 10, 255);
    lv_slider_set_value(brightness_slider, screen_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightness_slider, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
    lv_obj_set_style_radius(brightness_slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(brightness_slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_radius(brightness_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_width(brightness_slider, 40, LV_PART_KNOB);
    lv_obj_set_style_height(brightness_slider, 40, LV_PART_KNOB);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *brightness_down = lv_btn_create(brightness_panel);
    lv_obj_set_size(brightness_down, 58, 58);
    lv_obj_align(brightness_down, LV_ALIGN_CENTER, -145, 25);
    lv_obj_set_style_radius(brightness_down, 8, 0);
    lv_obj_set_style_bg_color(brightness_down, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_bg_color(brightness_down, lv_palette_main(LV_PALETTE_RED), LV_STATE_PRESSED);
    lv_obj_add_event_cb(brightness_down, brightness_step_event, LV_EVENT_CLICKED, (void *)(intptr_t)-15);
    lv_obj_t *brightness_down_label = lv_label_create(brightness_down);
    lv_label_set_text(brightness_down_label, "-");
    lv_obj_set_style_text_font(brightness_down_label, &lv_font_montserrat_32, 0);
    lv_obj_center(brightness_down_label);

    lv_obj_t *brightness_up = lv_btn_create(brightness_panel);
    lv_obj_set_size(brightness_up, 58, 58);
    lv_obj_align(brightness_up, LV_ALIGN_CENTER, 145, 25);
    lv_obj_set_style_radius(brightness_up, 8, 0);
    lv_obj_set_style_bg_color(brightness_up, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_bg_color(brightness_up, lv_palette_main(LV_PALETTE_RED), LV_STATE_PRESSED);
    lv_obj_add_event_cb(brightness_up, brightness_step_event, LV_EVENT_CLICKED, (void *)(intptr_t)15);
    lv_obj_t *brightness_up_label = lv_label_create(brightness_up);
    lv_label_set_text(brightness_up_label, "+");
    lv_obj_set_style_text_font(brightness_up_label, &lv_font_montserrat_32, 0);
    lv_obj_center(brightness_up_label);

    lv_obj_t *brightness_close = lv_btn_create(brightness_panel);
    lv_obj_set_size(brightness_close, 64, 48);
    lv_obj_align(brightness_close, LV_ALIGN_TOP_RIGHT, -12, 10);
    lv_obj_set_ext_click_area(brightness_close, 14);
    lv_obj_set_style_radius(brightness_close, 8, 0);
    lv_obj_set_style_bg_color(brightness_close, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_bg_color(brightness_close, lv_palette_main(LV_PALETTE_RED), LV_STATE_PRESSED);
    lv_obj_add_event_cb(brightness_close, brightness_close_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *brightness_close_label = lv_label_create(brightness_close);
    lv_label_set_text(brightness_close_label, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(brightness_close_label, &lv_font_montserrat_32, 0);
    lv_obj_center(brightness_close_label);

    zero_calibrate_button = lv_btn_create(brightness_panel);
    lv_obj_set_size(zero_calibrate_button, 260, 56);
    lv_obj_align(zero_calibrate_button, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_ext_click_area(zero_calibrate_button, 8);
    lv_obj_set_style_radius(zero_calibrate_button, 8, 0);
    lv_obj_set_style_bg_color(
        zero_calibrate_button, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_bg_color(
        zero_calibrate_button, lv_palette_darken(LV_PALETTE_GREEN, 2), LV_STATE_PRESSED);
    lv_obj_add_event_cb(
        zero_calibrate_button, zero_calibrate_event, LV_EVENT_CLICKED, NULL);

    zero_calibrate_label = lv_label_create(zero_calibrate_button);
    lv_label_set_text(zero_calibrate_label, "CALIBRAR 0");
    lv_obj_set_style_text_color(zero_calibrate_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(zero_calibrate_label, &lv_font_montserrat_18, 0);
    lv_obj_center(zero_calibrate_label);
    update_brightness_label();

    arc_fill = nullptr;
    indicator_outline = nullptr;
    indicator_cursor = nullptr;
    for (lv_obj_t *&tick : tick_lines) tick = nullptr;
    for (lv_obj_t *&scale_label : scale_labels) scale_label = nullptr;
    lv_obj_move_foreground(civic_logo_image);
}

// =========================
// Sensor -> PSI
// =========================

float read_boost_psi()
{
    uint32_t mv_sum = 0;
    for (uint8_t sample = 0; sample < SENSOR_SAMPLE_COUNT; ++sample) {
        mv_sum += analogReadMilliVolts(PRESSURE_SENSOR_PIN);
        delayMicroseconds(100);
    }

    float measured_mv = mv_sum / (float)SENSOR_SAMPLE_COUNT;
    last_sensor_sample_mv = (uint16_t)measured_mv;
    if (!sensor_filter_initialized) {
        filtered_sensor_mv = measured_mv;
        sensor_filter_initialized = true;
    } else {
        filtered_sensor_mv += SENSOR_FILTER_ALPHA * (measured_mv - filtered_sensor_mv);
    }
    last_sensor_mv = (uint16_t)lroundf(filtered_sensor_mv);

    float psi;
    if (last_sensor_mv <= sensor_atmosphere_mv) {
        psi = mapf(last_sensor_mv,
                   sensor_vacuum_mv, sensor_atmosphere_mv,
                   BOOST_MIN_PSI, 0.0f);
    } else {
        psi = mapf(last_sensor_mv,
                   sensor_atmosphere_mv, sensor_30_psi_mv,
                   0.0f, BOOST_MAX_PSI);
    }

    if (fabsf(psi) < SENSOR_ZERO_DEADBAND_PSI) psi = 0.0f;
    return clampf(psi, BOOST_MIN_PSI, BOOST_MAX_PSI);
}

void prime_pressure_sensor()
{
    // Descarta la carga residual del ADC tras permanecer inactivo durante el splash.
    for (int sample = 0; sample < 12; ++sample) {
        (void)analogRead(PRESSURE_SENSOR_PIN);
        delayMicroseconds(150);
    }
}

// =========================
// Actualizar gauge
// =========================

void set_indicator_position(float psi_value)
{
    // 135° izquierda -> 405° derecha (recorrido visual 270°)
    float angle_deg = gauge_angle_for_psi(psi_value);
    float angle_rad = angle_deg * (3.14159265f / 180.0f);
    double angle_cos = cos(angle_rad);
    double angle_sin = sin(angle_rad);

    lv_point_t next_points[2];
    next_points[0].x = CENTER_X + (int)(angle_cos * CURSOR_INNER_RADIUS);
    next_points[0].y = CENTER_Y + (int)(angle_sin * CURSOR_INNER_RADIUS);
    next_points[1].x = CENTER_X + (int)(angle_cos * CURSOR_OUTER_RADIUS);
    next_points[1].y = CENTER_Y + (int)(angle_sin * CURSOR_OUTER_RADIUS);

    if (next_points[0].x == indicator_outline_points[0].x &&
        next_points[0].y == indicator_outline_points[0].y &&
        next_points[1].x == indicator_outline_points[1].x &&
        next_points[1].y == indicator_outline_points[1].y) {
        return;
    }

    // Invalida cada geometria antes de sustituir sus puntos.
    lv_obj_invalidate(indicator_outline);
    lv_obj_invalidate(indicator_cursor);

    indicator_outline_points[0] = next_points[0];
    indicator_outline_points[1] = next_points[1];
    indicator_cursor_points[0] = next_points[0];
    indicator_cursor_points[1] = next_points[1];

    lv_line_set_points(indicator_outline, indicator_outline_points, 2);
    lv_line_set_points(indicator_cursor, indicator_cursor_points, 2);
}

void update_boost_ui(float psi_value)
{
    static uint32_t last_value_update_ms = 0;
    uint32_t now = millis();
    bool update_value = now - last_value_update_ms >= VALUE_UPDATE_MS;

    if (update_value) {
        last_value_update_ms = now;

        char txt[16];
        snprintf(txt, sizeof(txt), "%.1f", psi_value);
        render_prebaked_value(txt, false);
    }

    render_prebaked_gauge(psi_value, update_value);

    // Cambiar color según presión
}

float smoothstep01(float value)
{
    value = clampf(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

bool update_startup_sequence(uint32_t now)
{
    if (startup_phase == STARTUP_COMPLETE) return false;

    uint32_t elapsed = now - startup_phase_started_ms;
    if (startup_phase == STARTUP_SPLASH) {
        if (elapsed < STARTUP_SPLASH_MS) return true;

        startup_splash_visible = false;
        memcpy(composited_frame, static_frame,
               LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t));
        flush_composited_area({0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1});
        rendered_value_text[0] = '\0';
        prebaked_force_full = true;
        startup_phase = STARTUP_SWEEP_UP;
        startup_phase_started_ms = now;
        filtered_bar = BOOST_MIN_PSI;
        update_boost_ui(filtered_bar);
        return true;
    }

    if (startup_phase == STARTUP_SWEEP_UP) {
        float progress = elapsed / (float)STARTUP_SWEEP_UP_MS;
        filtered_bar = mapf(smoothstep01(progress), 0.0f, 1.0f,
                            BOOST_MIN_PSI, BOOST_MAX_PSI);
        update_boost_ui(filtered_bar);

        if (elapsed >= STARTUP_SWEEP_UP_MS) {
            startup_sweep_target_psi = BOOST_MIN_PSI;
            startup_phase = STARTUP_SWEEP_DOWN;
            startup_phase_started_ms = now;
        }
        return true;
    }

    float progress = elapsed / (float)STARTUP_SWEEP_DOWN_MS;
    filtered_bar = mapf(smoothstep01(progress), 0.0f, 1.0f,
                        BOOST_MAX_PSI, startup_sweep_target_psi);
    update_boost_ui(filtered_bar);

    if (elapsed >= STARTUP_SWEEP_DOWN_MS) {
        filtered_bar = startup_sweep_target_psi;
        update_boost_ui(filtered_bar);
        prime_pressure_sensor();
        startup_phase = STARTUP_COMPLETE;
    }
    return true;
}

// =========================
// Setup / Loop
// =========================

void setup()
{
    Serial.begin(115200);
    Serial.println("Boot...");
#if ENABLE_PERF_TELEMETRY
    esp_rom_printf(
        "Memory: flash=%lu MB, psram=%lu MB, free_psram=%lu KB\n",
        (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)),
        (unsigned long)(ESP.getPsramSize() / (1024UL * 1024UL)),
        (unsigned long)(ESP.getFreePsram() / 1024UL));
#endif

    pinMode(LCD_EN, OUTPUT);
    digitalWrite(LCD_EN, HIGH);

    // ADC
    analogReadResolution(12);
    analogSetPinAttenuation(PRESSURE_SENSOR_PIN, ADC_11db);
    delay(20);
    prime_pressure_sensor();
    calibrate_pressure_zero();

    // Display
    gfx->begin(80000000);
    gfx->fillScreen(BLACK);
    gfx->Display_Brightness(0);

    prebaked_enabled = allocate_prebaked_frames();
    if (!prebaked_enabled) {
        Serial.println("Prebaked renderer: PSRAM allocation failed");
        while (true) delay(1000);
    }

    display_prebaked_visual(PREBAKED_STARTUP_VISUAL);
    gfx->Display_Brightness(screen_brightness);
    startup_splash_visible = true;
    startup_phase_started_ms = millis();

    // LVGL
    lv_init();
#if ENABLE_RENDER_DIAGNOSTICS
    lv_log_register_print_cb(lvgl_log_print);
#endif
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LCD_WIDTH * LCD_HEIGHT / 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Touch
    if (FT3168->begin()) {
        FT3168->IIC_Write_Device_State(
            FT3168->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
            FT3168->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_ACTIVE);
        Serial.printf("FT3168 ready, ID: %#X\n", (int)FT3168->IIC_Read_Device_ID());
    } else {
        Serial.println("FT3168 initialization failed");
    }

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);

    create_ui();
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);

    apply_prebaked_visual(static_frame, PREBAKED_GAUGE_VISUAL);
    memcpy(composited_frame, static_frame,
           LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t));

    if (!load_prebaked_gauge_cache() ||
        !prepare_prebaked_value_glyphs()) {
        Serial.println("Prebaked renderer: asset preparation failed");
        while (true) delay(1000);
    }
    Serial.printf(
        "Prebaked renderer ready: %lu arc pixels, %lu arc commands, "
        "%lu cursor pixels, %u glyphs, free PSRAM=%lu KB\n",
                  (unsigned long)prebaked_arc_pixel_count,
                  (unsigned long)baked_arc_command_count,
                  (unsigned long)baked_cursor_pixel_count,
                  (unsigned)VALUE_GLYPH_COUNT,
                  (unsigned long)(ESP.getFreePsram() / 1024UL));
#if DUMP_BAKED_CACHE
    dump_baked_cache();
    while (true) delay(1000);
#endif
}

void loop()
{
    static uint32_t next_gauge_frame_us = 0;
    static uint32_t last_serial_ms = 0;
#if ENABLE_PERF_TELEMETRY
    static uint32_t last_perf_ms = 0;

    uint32_t lvgl_started_us = micros();
#endif
    lv_timer_handler();
#if ENABLE_PERF_TELEMETRY
    uint32_t lvgl_elapsed_us = micros() - lvgl_started_us;
    perf_stats.lvgl_time_us += lvgl_elapsed_us;
    ++perf_stats.lvgl_count;
    if (lvgl_elapsed_us > perf_stats.max_lvgl_us) {
        perf_stats.max_lvgl_us = lvgl_elapsed_us;
    }
#endif

    if (gauge_restore_pending) {
        lv_refr_now(NULL);
        memcpy(composited_frame, static_frame,
               LCD_WIDTH * LCD_HEIGHT * sizeof(lv_color_t));
        flush_composited_area({0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1});
        rendered_value_text[0] = '\0';
        prebaked_force_full = true;
        gauge_restore_pending = false;
        update_boost_ui(filtered_bar);
    }

    const uint32_t now = millis();
    const uint32_t now_us = micros();
    if (next_gauge_frame_us == 0) next_gauge_frame_us = now_us;
    const bool gauge_frame_due =
        (int32_t)(now_us - next_gauge_frame_us) >= 0;
    if (gauge_frame_due) {
        next_gauge_frame_us += GAUGE_FRAME_PERIOD_US;
        if ((int32_t)(now_us - next_gauge_frame_us) >=
            (int32_t)GAUGE_FRAME_PERIOD_US) {
            next_gauge_frame_us = now_us + GAUGE_FRAME_PERIOD_US;
        }
    }

    if (startup_phase != STARTUP_COMPLETE) {
        if (gauge_frame_due) update_startup_sequence(now);
        delay(1);
        return;
    }

#if ENABLE_PERF_TELEMETRY
    if (now - last_perf_ms >= PERF_UPDATE_MS) {
        uint32_t interval_ms = now - last_perf_ms;
        last_perf_ms = now;
        print_performance_stats(interval_ms);
    }
#endif

    if (gauge_frame_due) {
        float bar;
        if (gauge_mode == GAUGE_MODE_SHOW) {
            bar = show_boost_psi(now);
            filtered_bar = bar;
        } else {
            bar = read_boost_psi();
            // Modo real: representar la lectura del sensor sin interpolacion visual.
            filtered_bar = bar;
        }

        update_boost_ui(filtered_bar);

        if (gauge_mode == GAUGE_MODE_LIVE && now - last_serial_ms >= SERIAL_UPDATE_MS) {
            last_serial_ms = now;
            Serial.printf("sensor: sample_mv=%u, filtered_mv=%u, psi=%.3f\n",
                          last_sensor_sample_mv, last_sensor_mv, filtered_bar);
        }
    }

    delay(1);
}

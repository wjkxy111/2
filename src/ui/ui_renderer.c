#include "ui_renderer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../display/display_port.h"
#include "../vibration/vibration_features.h"

#define RGB565(r, g, b) ((uint16_t) ((((r) & 0xF8U) << 8) | (((g) & 0xFCU) << 3) | ((b) >> 3)))

static uint16_t const COLOR_BACKGROUND = RGB565(8, 15, 28);
static uint16_t const COLOR_PANEL      = RGB565(17, 29, 48);
static uint16_t const COLOR_PANEL_2    = RGB565(23, 40, 64);
static uint16_t const COLOR_TEXT       = RGB565(230, 238, 248);
static uint16_t const COLOR_MUTED      = RGB565(126, 148, 176);
static uint16_t const COLOR_ACCENT     = RGB565(36, 196, 220);
static uint16_t const COLOR_OK         = RGB565(47, 201, 123);
static uint16_t const COLOR_WARN       = RGB565(255, 180, 45);
static uint16_t const COLOR_ALARM      = RGB565(245, 78, 92);
static uint16_t const COLOR_GRID       = RGB565(42, 61, 83);
static uint16_t const COLOR_WHITE      = RGB565(255, 255, 255);

/* 5x7 glyphs for ASCII 0x20 through 0x5A. Lower-case text is rendered upper-case. */
static uint8_t const g_font[][5] =
{
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
};

static uint16_t * framebuffer(void)
{
    return display_port_framebuffer();
}

static void pixel(int x, int y, uint16_t color)
{
    if ((x >= 0) && (x < (int) DISPLAY_PORT_WIDTH) && (y >= 0) && (y < (int) DISPLAY_PORT_HEIGHT))
    {
        framebuffer()[(y * (int) DISPLAY_PORT_WIDTH) + x] = color;
    }
}

static void fill_rect(int x, int y, int width, int height, uint16_t color)
{
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = ((x + width) > (int) DISPLAY_PORT_WIDTH) ? (int) DISPLAY_PORT_WIDTH : (x + width);
    int y1 = ((y + height) > (int) DISPLAY_PORT_HEIGHT) ? (int) DISPLAY_PORT_HEIGHT : (y + height);
    for (int row = y0; row < y1; row++)
    {
        uint16_t * destination = &framebuffer()[(row * (int) DISPLAY_PORT_WIDTH) + x0];
        for (int column = x0; column < x1; column++)
        {
            *destination++ = color;
        }
    }
}

static void frame_rect(int x, int y, int width, int height, uint16_t color)
{
    fill_rect(x, y, width, 1, color);
    fill_rect(x, y + height - 1, width, 1, color);
    fill_rect(x, y, 1, height, color);
    fill_rect(x + width - 1, y, 1, height, color);
}

static void line(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy_abs = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int dy = -dy_abs;
    int sy = (y0 < y1) ? 1 : -1;
    int error = dx + dy;
    while (true)
    {
        pixel(x0, y0, color);
        if ((x0 == x1) && (y0 == y1))
        {
            break;
        }
        int const twice_error = 2 * error;
        if (twice_error >= dy)
        {
            error += dy;
            x0 += sx;
        }
        if (twice_error <= dx)
        {
            error += dx;
            y0 += sy;
        }
    }
}

static void glyph(int x, int y, char character, uint16_t color, uint8_t scale)
{
    if ('_' == character)
    {
        character = '-';
    }
    if ((character >= 'a') && (character <= 'z'))
    {
        character = (char) (character - ('a' - 'A'));
    }
    if ((character < ' ') || (character > 'Z'))
    {
        character = '?';
    }
    uint8_t const * columns = g_font[(uint8_t) character - (uint8_t) ' '];
    for (uint8_t column = 0U; column < 5U; column++)
    {
        for (uint8_t row = 0U; row < 7U; row++)
        {
            if (0U != (columns[column] & (1U << row)))
            {
                fill_rect(x + ((int) column * scale), y + ((int) row * scale), scale, scale, color);
            }
        }
    }
}

static void text(int x, int y, char const * value, uint16_t color, uint8_t scale)
{
    while ((NULL != value) && ('\0' != *value))
    {
        glyph(x, y, *value++, color, scale);
        x += (int) (6U * scale);
    }
}

static int text_width(char const * value, uint8_t scale)
{
    int width = 0;
    while ((NULL != value) && ('\0' != *value++))
    {
        width += (int) (6U * scale);
    }
    return width;
}

static void centered_text(int x, int y, int width, char const * value, uint16_t color, uint8_t scale)
{
    int const rendered_width = text_width(value, scale);
    int const left = x + ((width - rendered_width) / 2);
    text((left > x) ? left : x, y, value, color, scale);
}

static uint16_t health_color(edge_ai_health_t health)
{
    if (EDGE_AI_HEALTH_ALARM == health || EDGE_AI_HEALTH_SENSOR_ERROR == health)
    {
        return COLOR_ALARM;
    }
    if (EDGE_AI_HEALTH_WARN == health)
    {
        return COLOR_WARN;
    }
    return COLOR_OK;
}

static void header(ui_state_machine_t const * state, char const * title)
{
    fill_rect(0, 0, DISPLAY_PORT_WIDTH, DISPLAY_PORT_HEIGHT, COLOR_BACKGROUND);
    fill_rect(0, 0, DISPLAY_PORT_WIDTH, 56, COLOR_PANEL);
    text(10, 10, title, COLOR_TEXT, 2);
    text(10, 36, "RA8D1 + BMI088", COLOR_MUTED, 1);

    uint16_t const status_color = health_color(state->telemetry.health);
    fill_rect(184, 11, 62, 27, status_color);
    centered_text(184, 20, 62, edge_ai_health_name(state->telemetry.health), COLOR_BACKGROUND, 1);
}

static void footer(ui_page_t page)
{
    static char const * const labels[] = {"HOME", "WAVE", "AI", "LOG"};
    fill_rect(0, 442, DISPLAY_PORT_WIDTH, 38, COLOR_PANEL);
    for (uint8_t i = 0U; i < 4U; i++)
    {
        int const x = 4 + ((int) i * 63);
        bool const selected = ((uint8_t) page == (uint8_t) UI_PAGE_DASHBOARD + i);
        if (selected)
        {
            fill_rect(x, 446, 59, 28, COLOR_ACCENT);
        }
        centered_text(x, 456, 59, labels[i], selected ? COLOR_BACKGROUND : COLOR_MUTED, 1);
    }
}

static void format_milli(char output[16], int32_t value)
{
    bool const negative = value < 0;
    uint32_t const magnitude = (uint32_t) (negative ? -(int64_t) value : (int64_t) value);
    (void) snprintf(output, 16, "%s%lu.%01lu", negative ? "-" : "",
                    (unsigned long) (magnitude / 1000U),
                    (unsigned long) ((magnitude % 1000U) / 100U));
}

static void draw_progress(int x, int y, int width, uint16_t value, uint16_t maximum, uint16_t color)
{
    fill_rect(x, y, width, 12, COLOR_GRID);
    if (maximum > 0U)
    {
        uint32_t fill = ((uint32_t) width * value) / maximum;
        if (fill > (uint32_t) width)
        {
            fill = (uint32_t) width;
        }
        fill_rect(x, y, (int) fill, 12, color);
    }
}

static void status_banner(ui_state_machine_t const * state)
{
    char const * message = NULL;
    uint16_t color = COLOR_ALARM;

    if (!state->telemetry.sensor_ready)
    {
        message = "BMI088 SENSOR ERROR";
    }
    else if (state->calibration_failed)
    {
        message = "CALIBRATION FAILED";
    }
    else if ((!state->telemetry.model_trained) && (!state->telemetry.anomaly_ready))
    {
        message = "BASELINE REQUIRED - HOLD S1";
        color = COLOR_WARN;
    }

    if (NULL != message)
    {
        fill_rect(10, 408, 236, 27, color);
        centered_text(10, 418, 236, message, COLOR_BACKGROUND, 1);
    }
}

static void dashboard(ui_state_machine_t const * state)
{
    char buffer[32];
    char temperature[16];
    char rise[16];
    ui_telemetry_t const * data = &state->telemetry;
    header(state, "EDGE AI");

    fill_rect(10, 70, 236, 82, COLOR_PANEL);
    frame_rect(10, 70, 236, 82, COLOR_GRID);
    text(20, 80, "THERMAL CONTEXT", COLOR_MUTED, 1);
    format_milli(temperature, data->temperature_millideg_c);
    (void) snprintf(buffer, sizeof(buffer), "%s C", temperature);
    text(20, 101, buffer, COLOR_TEXT, 2);
    format_milli(rise, data->temperature_rise_millideg_c);
    (void) snprintf(buffer, sizeof(buffer), "DELTA %s C", rise);
    text(20, 132, buffer, COLOR_ACCENT, 1);

    fill_rect(10, 162, 236, 104, COLOR_PANEL);
    frame_rect(10, 162, 236, 104, COLOR_GRID);
    text(20, 174, "ANOMALY SCORE", COLOR_MUTED, 1);
    (void) snprintf(buffer, sizeof(buffer), "%u.%02u / 9.00",
                    data->anomaly_score_x100 / 100U, data->anomaly_score_x100 % 100U);
    text(20, 196, buffer, health_color(data->health), 2);
    draw_progress(20, 230, 216, data->anomaly_score_x100, 1200U, health_color(data->health));
    if (data->model_trained)
    {
        text(20, 248, "CLASSIFIER READY", COLOR_MUTED, 1);
    }
    else
    {
        text(20, 248, data->anomaly_ready ? "BASELINE READY" : "HOLD S1: CALIBRATE", COLOR_MUTED, 1);
    }

    fill_rect(10, 276, 236, 126, COLOR_PANEL);
    frame_rect(10, 276, 236, 126, COLOR_GRID);
    text(20, 288, "PREDICTION", COLOR_MUTED, 1);
    text(20, 310, data->class_name, COLOR_TEXT, 2);
    (void) snprintf(buffer, sizeof(buffer), "CONF %u.%u%%", data->confidence_per_mille / 10U,
                    data->confidence_per_mille % 10U);
    text(20, 340, buffer, COLOR_ACCENT, 1);
    (void) snprintf(buffer, sizeof(buffer), "INFERENCE %lu US", (unsigned long) data->inference_us);
    text(20, 359, buffer, COLOR_MUTED, 1);
    (void) snprintf(buffer, sizeof(buffer), "WINDOW %lu  %s", (unsigned long) data->window_id,
                    data->monitoring ? "MONITOR ON" : "MONITOR OFF");
    text(20, 379, buffer, COLOR_MUTED, 1);
    status_banner(state);
    footer(state->page);
}

static void waveform(ui_state_machine_t const * state)
{
    char buffer[32];
    header(state, "VIBRATION");
    text(12, 68, "ACCEL MAG / 0.64 S WINDOW", COLOR_MUTED, 1);
    fill_rect(10, 88, 236, 184, COLOR_PANEL);
    frame_rect(10, 88, 236, 184, COLOR_GRID);
    for (int y = 110; y < 272; y += 40)
    {
        line(14, y, 241, y, COLOR_GRID);
    }
    for (int x = 14; x < 242; x += 56)
    {
        line(x, 92, x, 267, COLOR_GRID);
    }
    for (uint8_t i = 1U; i < UI_WAVEFORM_POINTS; i++)
    {
        int const x0 = 14 + (((int) i - 1) * 224 / ((int) UI_WAVEFORM_POINTS - 1));
        int const x1 = 14 + ((int) i * 224 / ((int) UI_WAVEFORM_POINTS - 1));
        int const y0 = 180 - (state->telemetry.waveform[i - 1U] * 78 / 100);
        int const y1 = 180 - (state->telemetry.waveform[i] * 78 / 100);
        line(x0, y0, x1, y1, COLOR_ACCENT);
    }

    fill_rect(10, 286, 236, 116, COLOR_PANEL);
    frame_rect(10, 286, 236, 116, COLOR_GRID);
    text(20, 298, "LIVE PIPELINE", COLOR_MUTED, 1);
    text(20, 321, "BMI088 > FEATURES > MODEL", COLOR_TEXT, 1);
    text(20, 343, "800HZ / 512 / 26 FEATURES", COLOR_TEXT, 1);
    (void) snprintf(buffer, sizeof(buffer), "LATENCY %lu US", (unsigned long) state->telemetry.inference_us);
    text(20, 365, buffer, COLOR_ACCENT, 1);
    footer(state->page);
}

static void diagnostics(ui_state_machine_t const * state)
{
    char buffer[40];
    header(state, "AI DIAG");
    text(12, 69, "TOP ANOMALY CONTRIBUTORS", COLOR_MUTED, 1);
    if (!state->telemetry.anomaly_ready)
    {
        fill_rect(10, 91, 236, 180, COLOR_PANEL);
        frame_rect(10, 91, 236, 180, COLOR_GRID);
        centered_text(10, 154, 236, "NO BASELINE", COLOR_WARN, 2);
        centered_text(10, 194, 236, "HOLD S1 TO CALIBRATE", COLOR_MUTED, 1);
    }
    else
    {
        for (uint8_t rank = 0U; rank < EDGE_AI_EXPLANATION_COUNT; rank++)
        {
            int const y = 91 + ((int) rank * 64);
            edge_ai_explanation_t const * item = &state->telemetry.explanations[rank];
            fill_rect(10, y, 236, 52, COLOR_PANEL);
            frame_rect(10, y, 236, 52, COLOR_GRID);
            (void) snprintf(buffer, sizeof(buffer), "%u  %s", rank + 1U,
                            vibration_feature_name(item->feature_index));
            text(18, y + 10, buffer, COLOR_TEXT, 1);
            draw_progress(18, y + 31, 174, item->contribution_x100, 2500U, COLOR_ACCENT);
            (void) snprintf(buffer, sizeof(buffer), "%u.%u", item->contribution_x100 / 100U,
                            (item->contribution_x100 % 100U) / 10U);
            text(199, y + 31, buffer, COLOR_MUTED, 1);
        }
    }

    fill_rect(10, 296, 236, 106, COLOR_PANEL);
    frame_rect(10, 296, 236, 106, COLOR_GRID);
    text(20, 308, "MODEL METADATA", COLOR_MUTED, 1);
    (void) snprintf(buffer, sizeof(buffer), "VERSION %s", EDGE_AI_MODEL_VERSION);
    text(20, 330, buffer, COLOR_TEXT, 1);
    (void) snprintf(buffer, sizeof(buffer), "SCHEMA %s", EDGE_AI_TELEMETRY_SCHEMA);
    text(20, 350, buffer, COLOR_TEXT, 1);
    text(20, 370, state->telemetry.model_trained ? "CLASSIFIER TRAINED" : "ONE-CLASS FALLBACK", COLOR_ACCENT, 1);
    (void) snprintf(buffer, sizeof(buffer), "BASE WINDOWS %lu",
                    (unsigned long) state->telemetry.baseline_windows);
    text(20, 387, buffer, COLOR_MUTED, 1);
    footer(state->page);
}

static void event_log(ui_state_machine_t const * state)
{
    char buffer[40];
    header(state, "EVENT LOG");
    if (0U == state->telemetry.event_count)
    {
        fill_rect(10, 78, 236, 76, COLOR_PANEL);
        text(28, 100, "NO STATE CHANGES YET", COLOR_MUTED, 1);
    }
    else
    {
        for (uint8_t i = 0U; (i < state->telemetry.event_count) && (i < 6U); i++)
        {
            edge_ai_event_t const * event = &state->telemetry.events[i];
            int const y = 72 + ((int) i * 55);
            fill_rect(10, y, 236, 44, COLOR_PANEL);
            frame_rect(10, y, 236, 44, COLOR_GRID);
            fill_rect(10, y, 6, 44, health_color(event->health));
            (void) snprintf(buffer, sizeof(buffer), "#%lu  %s", (unsigned long) event->window_id,
                            edge_ai_health_name(event->health));
            text(25, y + 8, buffer, COLOR_TEXT, 1);
            (void) snprintf(buffer, sizeof(buffer), "SCORE %u.%02u", event->score_x100 / 100U,
                            event->score_x100 % 100U);
            text(25, y + 25, buffer, COLOR_MUTED, 1);
        }
    }
    (void) snprintf(buffer, sizeof(buffer), "STREAK SET %u/3 CLEAR %u/5",
                    state->telemetry.abnormal_streak, state->telemetry.normal_streak);
    text(12, 414, buffer, COLOR_MUTED, 1);
    footer(state->page);
}

static void calibration_overlay(ui_state_machine_t const * state)
{
    char buffer[32];
    fill_rect(18, 154, 220, 160, COLOR_PANEL_2);
    frame_rect(18, 154, 220, 160, COLOR_WHITE);
    fill_rect(18, 154, 220, 5, COLOR_ACCENT);
    centered_text(18, 178, 220, "CALIBRATING", COLOR_TEXT, 2);
    text(34, 212, "KEEP MACHINE NORMAL", COLOR_MUTED, 1);
    (void) snprintf(buffer, sizeof(buffer), "%u / %u", state->calibration_current,
                    state->calibration_total);
    centered_text(18, 237, 220, buffer, COLOR_ACCENT, 2);
    draw_progress(34, 276, 188, state->calibration_current, state->calibration_total, COLOR_ACCENT);
}

static void boot_screen(void)
{
    fill_rect(0, 0, DISPLAY_PORT_WIDTH, DISPLAY_PORT_HEIGHT, COLOR_BACKGROUND);
    fill_rect(38, 122, 180, 180, COLOR_PANEL);
    fill_rect(38, 122, 180, 6, COLOR_ACCENT);
    text(64, 160, "EDGE AI", COLOR_TEXT, 2);
    text(58, 199, "RA8D1 + BMI088", COLOR_ACCENT, 1);
    text(72, 229, "DISPLAY INIT", COLOR_MUTED, 1);
    draw_progress(58, 263, 140, 3U, 4U, COLOR_ACCENT);
}

void ui_renderer_render(ui_state_machine_t const * state)
{
    if ((NULL == state) || (!display_port_ready()))
    {
        return;
    }

    switch (state->page)
    {
        case UI_PAGE_DASHBOARD:      dashboard(state); break;
        case UI_PAGE_WAVEFORM:       waveform(state); break;
        case UI_PAGE_AI_DIAGNOSTICS: diagnostics(state); break;
        case UI_PAGE_EVENT_LOG:      event_log(state); break;
        case UI_PAGE_BOOT:
        default:                     boot_screen(); break;
    }
    if (state->calibrating)
    {
        calibration_overlay(state);
    }
    display_port_keep_awake();
    display_port_flush();
}

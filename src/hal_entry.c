#include "hal_data.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bmi088/bmi088_ra8d1.h"
#include "display/display_port.h"
#include "edge_ai/edge_ai_monitor.h"
#include "temperature/temperature_monitor.h"
#include "ui/ui_renderer.h"
#include "ui/ui_state_machine.h"
#include "vibration/vibration_anomaly.h"
#include "vibration/vibration_features.h"
#include "vibration/vibration_model.h"

#define LED_PIN                 BSP_IO_PORT_06_PIN_00 /* Core-board blue LED, active low */
#define KEY_PIN                 BSP_IO_PORT_00_PIN_06 /* Expansion-board S1 */
#define LED_ALARM_LEVEL         BSP_IO_LEVEL_LOW
#define LED_IDLE_LEVEL          BSP_IO_LEVEL_HIGH
#define UART_BUFFER_SIZE        (384U)
#define CAPTURE_CLASS_COUNT     (5U)
#define KEY_LONG_PRESS_MS       (800U)
#define HOST_PROTOCOL_VERSION   (1U)

static volatile bool g_uart_tx_complete;
static volatile bool g_uart_rx_ready;
static volatile uint8_t g_uart_rx_char;
static bmi088_sample_t g_samples[VIBRATION_WINDOW_SAMPLES];
static vibration_anomaly_model_t g_anomaly_model;
static temperature_monitor_t g_temperature_monitor;
static edge_ai_monitor_t g_edge_monitor;
static ui_state_machine_t g_ui;
static ui_telemetry_t g_ui_telemetry;
static uint32_t g_window_id;
static bool g_cycle_counter_available;
static bool g_monitor_enabled;
static uint32_t g_uptime_last_cycles;
static uint64_t g_uptime_cycles;

static void print_host_snapshot(void);

static void uart_send(char const * text_value)
{
    if (NULL == text_value)
    {
        return;
    }

    g_uart_tx_complete = false;
    if (FSP_SUCCESS != g_uart0.p_api->write(g_uart0.p_ctrl,
                                             (uint8_t const *) text_value,
                                             (uint32_t) strlen(text_value)))
    {
        return;
    }
    while (!g_uart_tx_complete)
    {
        __NOP();
    }
}

static void uart_sendf(char const * format, ...)
{
    char buffer[UART_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    (void) vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    uart_send(buffer);
}

void uart3_callback(uart_callback_args_t * p_args)
{
    if (UART_EVENT_TX_COMPLETE == p_args->event)
    {
        g_uart_tx_complete = true;
    }
    else if (UART_EVENT_RX_CHAR == p_args->event)
    {
        g_uart_rx_char = (uint8_t) p_args->data;
        g_uart_rx_ready = true;
    }
}

static void render_ui(void)
{
    if (ui_state_machine_take_dirty(&g_ui))
    {
        ui_renderer_render(&g_ui);
    }
}

static void sample_clock_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t const before = DWT->CYCCNT;
    for (volatile uint32_t i = 0U; i < 32U; i++)
    {
        __NOP();
    }
    g_cycle_counter_available = (DWT->CYCCNT != before);
    g_uptime_last_cycles = DWT->CYCCNT;
    g_uptime_cycles = 0U;
}

static void uptime_update(void)
{
    if (g_cycle_counter_available)
    {
        uint32_t const now = DWT->CYCCNT;
        g_uptime_cycles += (uint32_t) (now - g_uptime_last_cycles);
        g_uptime_last_cycles = now;
    }
}

static uint32_t uptime_milliseconds(void)
{
    uptime_update();
    if ((!g_cycle_counter_available) || (0U == SystemCoreClock))
    {
        return 0U;
    }
    uint64_t const whole_seconds = g_uptime_cycles / SystemCoreClock;
    uint64_t const remaining_cycles = g_uptime_cycles % SystemCoreClock;
    return (uint32_t) ((whole_seconds * 1000ULL) +
                       ((remaining_cycles * 1000ULL) / SystemCoreClock));
}

static uint32_t cycles_to_microseconds(uint32_t cycles)
{
    if ((!g_cycle_counter_available) || (0U == SystemCoreClock))
    {
        return 0U;
    }
    return (uint32_t) ((((uint64_t) cycles) * 1000000ULL) / SystemCoreClock);
}

static bool read_temperature(void)
{
    int32_t temperature_millideg_c;
    if (BMI088_OK != bmi088_ra8d1_read_temperature(&temperature_millideg_c))
    {
        return false;
    }
    temperature_monitor_update(&g_temperature_monitor, temperature_millideg_c);
    return true;
}

static void print_temperature_now(void)
{
    temperature_level_t const level = temperature_monitor_level(&g_temperature_monitor);
    uart_sendf("TEMP_NOW,%ld,%ld,%ld,%s\r\n",
               (long) temperature_monitor_current(&g_temperature_monitor),
               (long) temperature_monitor_baseline(&g_temperature_monitor),
               (long) temperature_monitor_rise(&g_temperature_monitor),
               temperature_monitor_level_name(level));
}

static bool capture_window(void)
{
    uint32_t const cycles_per_sample = SystemCoreClock / VIBRATION_SAMPLE_RATE_HZ;
    uint32_t next_sample = DWT->CYCCNT;

    for (size_t i = 0U; i < VIBRATION_WINDOW_SAMPLES; i++)
    {
        if (BMI088_OK != bmi088_ra8d1_read(&g_samples[i]))
        {
            return false;
        }

        if (g_cycle_counter_available)
        {
            next_sample += cycles_per_sample;
            while ((int32_t) (DWT->CYCCNT - next_sample) < 0)
            {
                __NOP();
            }
        }
        else
        {
            R_BSP_SoftwareDelay(1000000U / VIBRATION_SAMPLE_RATE_HZ, BSP_DELAY_UNITS_MICROSECONDS);
        }
    }

    if (!read_temperature())
    {
        return false;
    }
    g_window_id++;
    uptime_update();
    return true;
}

static void update_waveform(void)
{
    int32_t values[UI_WAVEFORM_POINTS];
    int64_t sum = 0;
    for (uint8_t point = 0U; point < UI_WAVEFORM_POINTS; point++)
    {
        int64_t bucket = 0;
        for (uint8_t offset = 0U; offset < (VIBRATION_WINDOW_SAMPLES / UI_WAVEFORM_POINTS); offset++)
        {
            size_t const index = ((size_t) point * (VIBRATION_WINDOW_SAMPLES / UI_WAVEFORM_POINTS)) + offset;
            int32_t const ax = g_samples[index].ax;
            int32_t const ay = g_samples[index].ay;
            int32_t const az = g_samples[index].az;
            bucket += ((ax < 0) ? -ax : ax) + ((ay < 0) ? -ay : ay) + ((az < 0) ? -az : az);
        }
        values[point] = (int32_t) (bucket / (VIBRATION_WINDOW_SAMPLES / UI_WAVEFORM_POINTS));
        sum += values[point];
    }

    int32_t const average = (int32_t) (sum / UI_WAVEFORM_POINTS);
    int32_t maximum = 1;
    for (uint8_t point = 0U; point < UI_WAVEFORM_POINTS; point++)
    {
        int32_t const centered = values[point] - average;
        int32_t const magnitude = (centered < 0) ? -centered : centered;
        if (magnitude > maximum)
        {
            maximum = magnitude;
        }
    }
    for (uint8_t point = 0U; point < UI_WAVEFORM_POINTS; point++)
    {
        g_ui_telemetry.waveform[point] = (int16_t) (((values[point] - average) * 100) / maximum);
    }
}

static void refresh_ui_common(void)
{
    g_ui_telemetry.sensor_ready = true;
    g_ui_telemetry.model_trained = vibration_model_is_trained();
    g_ui_telemetry.anomaly_ready = vibration_anomaly_ready(&g_anomaly_model);
    g_ui_telemetry.temperature_millideg_c = temperature_monitor_current(&g_temperature_monitor);
    g_ui_telemetry.temperature_rise_millideg_c = temperature_monitor_rise(&g_temperature_monitor);
    g_ui_telemetry.baseline_windows = g_anomaly_model.count;
    g_ui_telemetry.abnormal_streak = g_edge_monitor.abnormal_streak;
    g_ui_telemetry.normal_streak = g_edge_monitor.normal_streak;
    g_ui_telemetry.event_count = edge_ai_monitor_copy_events(&g_edge_monitor, g_ui_telemetry.events);
    ui_state_machine_set_telemetry(&g_ui, &g_ui_telemetry);
}

static void mark_sensor_error(void)
{
    g_ui_telemetry.health = edge_ai_monitor_update(&g_edge_monitor, false, false, true,
                                                   g_window_id, g_ui_telemetry.anomaly_score_x100);
    g_ui_telemetry.sensor_ready = false;
    g_ui_telemetry.event_count = edge_ai_monitor_copy_events(&g_edge_monitor, g_ui_telemetry.events);
    ui_state_machine_set_telemetry(&g_ui, &g_ui_telemetry);
    ui_state_machine_dispatch(&g_ui, UI_EVENT_SENSOR_ERROR);
    R_IOPORT_PinWrite(&g_ioport_ctrl, LED_PIN, LED_ALARM_LEVEL);
    render_ui();
}

static void dump_window(uint8_t label)
{
    uart_sendf("BEGIN,%lu,%u,%u,%u\r\n", (unsigned long) g_window_id, (unsigned int) label,
               (unsigned int) VIBRATION_SAMPLE_RATE_HZ, (unsigned int) VIBRATION_WINDOW_SAMPLES);
    uart_sendf("TEMP,%lu,%ld,%ld,%s\r\n", (unsigned long) g_window_id,
               (long) temperature_monitor_current(&g_temperature_monitor),
               (long) temperature_monitor_rise(&g_temperature_monitor),
               temperature_monitor_level_name(temperature_monitor_level(&g_temperature_monitor)));
    uart_send("COLUMNS,window,label,sample,ax,ay,az,gx,gy,gz\r\n");

    for (size_t i = 0U; i < VIBRATION_WINDOW_SAMPLES; i++)
    {
        bmi088_sample_t const * sample = &g_samples[i];
        uart_sendf("DATA,%lu,%u,%u,%d,%d,%d,%d,%d,%d\r\n", (unsigned long) g_window_id,
                   (unsigned int) label, (unsigned int) i, sample->ax, sample->ay, sample->az,
                   sample->gx, sample->gy, sample->gz);
    }
    uart_sendf("END,%lu\r\n", (unsigned long) g_window_id);
}

static bool extract_current_features(float features[VIBRATION_FEATURE_COUNT])
{
    return vibration_features_extract(g_samples, temperature_monitor_current(&g_temperature_monitor),
                                      temperature_monitor_rise(&g_temperature_monitor), features);
}

static void print_feature_vector(float const features[VIBRATION_FEATURE_COUNT])
{
    uart_sendf("FEATURES,%lu", (unsigned long) g_window_id);
    for (size_t i = 0U; i < VIBRATION_FEATURE_COUNT; i++)
    {
        int32_t const scaled = (int32_t) (features[i] * 1000000.0f);
        uart_sendf(",%ld", (long) scaled);
    }
    uart_send("\r\n");
}

static void calibrate_normal_baseline(void)
{
    float features[VIBRATION_FEATURE_COUNT];
    vibration_anomaly_reset(&g_anomaly_model);
    temperature_monitor_begin_baseline(&g_temperature_monitor);
    ui_state_machine_dispatch(&g_ui, UI_EVENT_CALIBRATION_BEGIN);
    ui_state_machine_set_calibration_progress(&g_ui, 0U, VIBRATION_BASELINE_MIN_WINDOWS);
    render_ui();
    uart_send("CALIBRATION,START,keep_machine_in_normal_state\r\n");

    for (uint32_t window = 0U; window < VIBRATION_BASELINE_MIN_WINDOWS; window++)
    {
        if ((!capture_window()) || (!extract_current_features(features)))
        {
            uart_send("ERROR,CALIBRATION,SENSOR_READ\r\n");
            vibration_anomaly_reset(&g_anomaly_model);
            temperature_monitor_finish_baseline(&g_temperature_monitor);
            mark_sensor_error();
            ui_state_machine_dispatch(&g_ui, UI_EVENT_CALIBRATION_FAILED);
            render_ui();
            return;
        }
        vibration_anomaly_update(&g_anomaly_model, features);
        update_waveform();
        ui_state_machine_set_calibration_progress(&g_ui, (uint8_t) (window + 1U),
                                                  VIBRATION_BASELINE_MIN_WINDOWS);
        render_ui();
        uart_sendf("CALIBRATION,%lu,%u\r\n", (unsigned long) (window + 1U),
                   (unsigned int) VIBRATION_BASELINE_MIN_WINDOWS);
        if (g_uart_rx_ready && ((uint8_t) 'q' == g_uart_rx_char))
        {
            g_uart_rx_ready = false;
            print_host_snapshot();
        }
    }

    temperature_monitor_finish_baseline(&g_temperature_monitor);
    refresh_ui_common();
    ui_state_machine_dispatch(&g_ui, UI_EVENT_CALIBRATION_DONE);
    render_ui();
    uart_sendf("CALIBRATION,DONE,TEMP_BASELINE,%ld\r\n",
               (long) temperature_monitor_baseline(&g_temperature_monitor));
}

static void run_inference_window(void)
{
    float features[VIBRATION_FEATURE_COUNT];
    float probabilities[VIBRATION_CLASS_COUNT];
    vibration_class_t predicted_class = VIBRATION_CLASS_NORMAL;

    if ((!capture_window()) || (!extract_current_features(features)))
    {
        uart_send("ERROR,INFERENCE,SENSOR_READ\r\n");
        mark_sensor_error();
        return;
    }

    uint32_t const inference_start = DWT->CYCCNT;
    float const anomaly_score = vibration_anomaly_score(&g_anomaly_model, features);
    bool const class_valid = vibration_model_predict(features, &predicted_class, probabilities);
    uint32_t const inference_cycles = DWT->CYCCNT - inference_start;

    bool vibration_alarm;
    uint32_t confidence_per_mille = 0U;
    if (class_valid)
    {
        confidence_per_mille = (uint32_t) (probabilities[predicted_class] * 1000.0f);
        vibration_alarm = (VIBRATION_CLASS_NORMAL != predicted_class) && (confidence_per_mille >= 700U);
    }
    else
    {
        vibration_alarm = vibration_anomaly_ready(&g_anomaly_model) &&
                          (anomaly_score >= VIBRATION_ANOMALY_THRESHOLD);
    }

    temperature_level_t const temperature_level = temperature_monitor_level(&g_temperature_monitor);
    bool const raw_alarm = vibration_alarm || (TEMPERATURE_LEVEL_ALARM == temperature_level);
    bool const raw_warning = (!raw_alarm) && (TEMPERATURE_LEVEL_WARN == temperature_level);
    uint32_t score_scaled = (uint32_t) (anomaly_score * 100.0f);
    if (score_scaled > UINT16_MAX)
    {
        score_scaled = UINT16_MAX;
    }
    edge_ai_health_t const health = edge_ai_monitor_update(&g_edge_monitor, raw_alarm, raw_warning,
                                                            false, g_window_id, (uint16_t) score_scaled);

    g_ui_telemetry.health = health;
    g_ui_telemetry.window_id = g_window_id;
    g_ui_telemetry.anomaly_score_x100 = (uint16_t) score_scaled;
    g_ui_telemetry.confidence_per_mille = (uint16_t) confidence_per_mille;
    g_ui_telemetry.inference_us = cycles_to_microseconds(inference_cycles);
    (void) snprintf(g_ui_telemetry.class_name, sizeof(g_ui_telemetry.class_name), "%s",
                    class_valid ? vibration_model_class_name(predicted_class) : "anomaly only");
    edge_ai_explain(&g_anomaly_model, features, g_ui_telemetry.explanations);
    update_waveform();
    refresh_ui_common();
    render_ui();

    R_IOPORT_PinWrite(&g_ioport_ctrl, LED_PIN,
                      (EDGE_AI_HEALTH_OK == health) ? LED_IDLE_LEVEL : LED_ALARM_LEVEL);
    uart_sendf("RESULT,%lu,%lu,%s,%lu,%ld,%ld,%s,%s,%lu,%s\r\n",
               (unsigned long) g_window_id, (unsigned long) score_scaled,
               class_valid ? vibration_model_class_name(predicted_class) : "anomaly_only",
               (unsigned long) confidence_per_mille,
               (long) temperature_monitor_current(&g_temperature_monitor),
               (long) temperature_monitor_rise(&g_temperature_monitor),
               temperature_monitor_level_name(temperature_level), edge_ai_health_name(health),
               (unsigned long) g_ui_telemetry.inference_us, EDGE_AI_MODEL_VERSION);
}

static void print_diagnostics(void)
{
    uart_sendf("DIAG,MODEL,%s,VERSION,%s,SCHEMA,%s,BASELINE_WINDOWS,%lu,INFERENCE_US,%lu,HEALTH,%s\r\n",
               vibration_model_is_trained() ? "TRAINED" : "ONE_CLASS_FALLBACK", EDGE_AI_MODEL_VERSION,
               EDGE_AI_TELEMETRY_SCHEMA, (unsigned long) g_anomaly_model.count,
               (unsigned long) g_ui_telemetry.inference_us, edge_ai_health_name(g_ui_telemetry.health));
    for (uint8_t rank = 0U; rank < EDGE_AI_EXPLANATION_COUNT; rank++)
    {
        edge_ai_explanation_t const * explanation = &g_ui_telemetry.explanations[rank];
        uart_sendf("EXPLAIN,%u,%s,%u\r\n", (unsigned int) (rank + 1U),
                   vibration_feature_name(explanation->feature_index), explanation->contribution_x100);
    }
}

static char const * host_mode_name(void)
{
    if (g_ui.calibrating)
    {
        return "CALIBRATE";
    }
    return g_monitor_enabled ? "MONITOR" : "IDLE";
}

static char const * host_class_name(void)
{
    return (0 == strcmp(g_ui_telemetry.class_name, "anomaly only")) ?
           "anomaly_only" : g_ui_telemetry.class_name;
}

static void print_host_waveform(void)
{
    static char const hex_digits[] = "0123456789ABCDEF";
    char encoded[(UI_WAVEFORM_POINTS * 2U) + 1U];

    for (uint8_t point = 0U; point < UI_WAVEFORM_POINTS; point++)
    {
        int16_t value = g_ui_telemetry.waveform[point];
        if (value > INT8_MAX)
        {
            value = INT8_MAX;
        }
        else if (value < INT8_MIN)
        {
            value = INT8_MIN;
        }

        uint8_t const byte = (uint8_t) ((int8_t) value);
        encoded[(size_t) point * 2U] = hex_digits[(byte >> 4U) & 0x0FU];
        encoded[((size_t) point * 2U) + 1U] = hex_digits[byte & 0x0FU];
    }
    encoded[UI_WAVEFORM_POINTS * 2U] = '\0';

    uart_sendf("BMI_WAVE,WINDOW,%lu,ENC,S8HEX,POINTS,%u,DATA,%s\r\n",
               (unsigned long) g_window_id, (unsigned int) UI_WAVEFORM_POINTS, encoded);
}

static void print_host_snapshot(void)
{
    temperature_level_t const temperature_level = temperature_monitor_level(&g_temperature_monitor);

    /* q is deliberately read-only: it reports the last completed window and never samples or infers. */
    uart_sendf("BMI_TELEM,T_MS,%lu,WINDOW,%lu,MODE,%s,MONITOR,%s,SENSOR,%s,MODEL,%s,"
               "HEALTH,%s,SCORE_X100,%u,CLASS,%s,CONF_PM,%u,TEMP_MC,%ld,RISE_MC,%ld,"
               "TEMP_LEVEL,%s,INFER_US,%lu,BASE_WINDOWS,%lu,EVENTS,%u\r\n",
               (unsigned long) uptime_milliseconds(), (unsigned long) g_window_id,
               host_mode_name(), g_monitor_enabled ? "ON" : "OFF",
               g_ui_telemetry.sensor_ready ? "READY" : "ERROR",
               vibration_model_is_trained() ? "TRAINED" : "ONE_CLASS_FALLBACK",
               edge_ai_health_name(g_ui_telemetry.health),
               (unsigned int) g_ui_telemetry.anomaly_score_x100, host_class_name(),
               (unsigned int) g_ui_telemetry.confidence_per_mille,
               (long) temperature_monitor_current(&g_temperature_monitor),
               (long) temperature_monitor_rise(&g_temperature_monitor),
               temperature_monitor_level_name(temperature_level),
               (unsigned long) g_ui_telemetry.inference_us,
               (unsigned long) g_anomaly_model.count,
               (unsigned int) g_ui_telemetry.event_count);

    print_host_waveform();
    for (uint8_t rank = 0U; rank < EDGE_AI_EXPLANATION_COUNT; rank++)
    {
        edge_ai_explanation_t const * explanation = &g_ui_telemetry.explanations[rank];
        uart_sendf("BMI_EXPLAIN,WINDOW,%lu,RANK,%u,FEATURE,%s,CONTRIB_X100,%u\r\n",
                   (unsigned long) g_window_id, (unsigned int) (rank + 1U),
                   vibration_feature_name(explanation->feature_index),
                   (unsigned int) explanation->contribution_x100);
    }
}

static void print_help(void)
{
    uart_send("\r\nRA8D1 + BMI088 vibration-temperature edge AI + MIPI UI\r\n");
    uart_send("Commands:\r\n");
    uart_send("  0..4 capture labelled data: normal/imbalance/loose/rub/bearing\r\n");
    uart_send("  b    calibrate 16 normal windows for anomaly detection\r\n");
    uart_send("  m    toggle continuous monitoring\r\n");
    uart_send("  p    capture and print the 26-feature vector\r\n");
    uart_send("  t    read BMI088 temperature now\r\n");
    uart_send("  u    switch display page\r\n");
    uart_send("  i    print model, latency and Top-3 explanations\r\n");
    uart_send("  q    read host telemetry, waveform and Top-3 (no new inference)\r\n");
    uart_send("  h    print this help\r\n");
    uart_send("S1 short press: next page; hold 0.8 s: baseline calibration.\r\n\r\n");
}

void hal_entry(void)
{
    bsp_io_level_t previous_key = BSP_IO_LEVEL_HIGH;
    uint32_t key_press_cycle = 0U;

    R_IOPORT_PinWrite(&g_ioport_ctrl, LED_PIN, LED_IDLE_LEVEL);
    if (FSP_SUCCESS != g_uart0.p_api->open(g_uart0.p_ctrl, g_uart0.p_cfg))
    {
        while (1)
        {
            R_IOPORT_PinWrite(&g_ioport_ctrl, LED_PIN, LED_ALARM_LEVEL);
        }
    }

    sample_clock_init();
    uart_sendf("PROTO,NAME,RA8D1_BMI088_EDGE_AI,VERSION,%u,TRANSPORT,UART,BAUD,115200\r\n",
               (unsigned int) HOST_PROTOCOL_VERSION);
    edge_ai_monitor_init(&g_edge_monitor);
    vibration_anomaly_reset(&g_anomaly_model);
    temperature_monitor_reset(&g_temperature_monitor);
    ui_state_machine_init(&g_ui);
    memset(&g_ui_telemetry, 0, sizeof(g_ui_telemetry));
    g_ui_telemetry.health = EDGE_AI_HEALTH_OK;
    (void) snprintf(g_ui_telemetry.class_name, sizeof(g_ui_telemetry.class_name), "starting");

    bool const display_ok = display_port_init();
    uart_sendf("DISPLAY,ST7796U,%s,256x480,RGB565\r\n", display_ok ? "READY" : "ERROR");
    render_ui();
    print_help();

    int8_t const sensor_result = bmi088_ra8d1_init();
    uart_sendf("BMI088,INIT,%d,ACC_ID,0x%02X,GYRO_ID,0x%02X\r\n", sensor_result,
               bmi088_ra8d1_accel_chip_id(), bmi088_ra8d1_gyro_chip_id());
    if (BMI088_OK != sensor_result)
    {
        uart_send("ERROR,BMI088_INIT,check_SPI_and_CS_D8_D3\r\n");
        mark_sensor_error();
        while (1)
        {
            if (g_uart_rx_ready)
            {
                uint8_t const command = g_uart_rx_char;
                g_uart_rx_ready = false;
                if ((uint8_t) 'q' == command)
                {
                    print_host_snapshot();
                }
                else if ((uint8_t) 'h' == command)
                {
                    print_help();
                }
            }
            __WFI();
        }
    }

    if (read_temperature())
    {
        print_temperature_now();
    }
    else
    {
        uart_send("ERROR,TEMPERATURE_READ\r\n");
    }
    g_ui_telemetry.sensor_ready = true;
    g_ui_telemetry.model_trained = vibration_model_is_trained();
    (void) snprintf(g_ui_telemetry.class_name, sizeof(g_ui_telemetry.class_name), "ready");
    refresh_ui_common();
    ui_state_machine_dispatch(&g_ui, UI_EVENT_BOOT_OK);
    render_ui();
    uart_sendf("MODEL,%s,VERSION,%s\r\n", vibration_model_is_trained() ? "TRAINED" : "NOT_TRAINED",
               EDGE_AI_MODEL_VERSION);

    while (1)
    {
        uptime_update();
        bsp_io_level_t key;
        (void) R_IOPORT_PinRead(&g_ioport_ctrl, KEY_PIN, &key);
        if ((BSP_IO_LEVEL_LOW == key) && (BSP_IO_LEVEL_HIGH == previous_key))
        {
            key_press_cycle = DWT->CYCCNT;
        }
        else if ((BSP_IO_LEVEL_HIGH == key) && (BSP_IO_LEVEL_LOW == previous_key))
        {
            uint32_t const held_cycles = DWT->CYCCNT - key_press_cycle;
            uint32_t const held_ms = g_cycle_counter_available ?
                                     (uint32_t) ((((uint64_t) held_cycles) * 1000ULL) / SystemCoreClock) : 0U;
            if (held_ms >= KEY_LONG_PRESS_MS)
            {
                calibrate_normal_baseline();
            }
            else
            {
                ui_state_machine_dispatch(&g_ui, UI_EVENT_BUTTON_SHORT);
                render_ui();
            }
        }
        previous_key = key;

        if (g_uart_rx_ready)
        {
            uint8_t const command = g_uart_rx_char;
            g_uart_rx_ready = false;

            if ((command >= (uint8_t) '0') && (command < (uint8_t) ('0' + CAPTURE_CLASS_COUNT)))
            {
                uint8_t const label = (uint8_t) (command - (uint8_t) '0');
                if (capture_window())
                {
                    update_waveform();
                    refresh_ui_common();
                    render_ui();
                    dump_window(label);
                }
            }
            else if ((uint8_t) 'b' == command)
            {
                calibrate_normal_baseline();
            }
            else if ((uint8_t) 'm' == command)
            {
                g_monitor_enabled = !g_monitor_enabled;
                g_ui_telemetry.monitoring = g_monitor_enabled;
                refresh_ui_common();
                render_ui();
                uart_sendf("MONITOR,%s\r\n", g_monitor_enabled ? "ON" : "OFF");
            }
            else if ((uint8_t) 'p' == command)
            {
                float features[VIBRATION_FEATURE_COUNT];
                if (capture_window() && extract_current_features(features))
                {
                    update_waveform();
                    refresh_ui_common();
                    render_ui();
                    print_feature_vector(features);
                }
            }
            else if ((uint8_t) 't' == command)
            {
                if (read_temperature())
                {
                    refresh_ui_common();
                    render_ui();
                    print_temperature_now();
                }
                else
                {
                    uart_send("ERROR,TEMPERATURE_READ\r\n");
                }
            }
            else if ((uint8_t) 'u' == command)
            {
                ui_state_machine_dispatch(&g_ui, UI_EVENT_BUTTON_SHORT);
                render_ui();
            }
            else if ((uint8_t) 'i' == command)
            {
                print_diagnostics();
            }
            else if ((uint8_t) 'q' == command)
            {
                print_host_snapshot();
            }
            else if ((uint8_t) 'h' == command)
            {
                print_help();
            }
        }

        if (g_monitor_enabled)
        {
            run_inference_window();
        }
        else
        {
            R_BSP_SoftwareDelay(10U, BSP_DELAY_UNITS_MILLISECONDS);
        }
    }
}

#if BSP_TZ_SECURE_BUILD
FSP_CPP_HEADER
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable(void);
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable(void)
{
}
FSP_CPP_FOOTER
#endif

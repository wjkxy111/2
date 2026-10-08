#ifndef UI_STATE_MACHINE_H
#define UI_STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

#include "../edge_ai/edge_ai_monitor.h"

#define UI_WAVEFORM_POINTS (64U)

typedef enum
{
    UI_PAGE_BOOT = 0,
    UI_PAGE_DASHBOARD,
    UI_PAGE_WAVEFORM,
    UI_PAGE_AI_DIAGNOSTICS,
    UI_PAGE_EVENT_LOG,
    UI_PAGE_COUNT
} ui_page_t;

typedef enum
{
    UI_EVENT_BOOT_OK = 0,
    UI_EVENT_SENSOR_ERROR,
    UI_EVENT_BUTTON_SHORT,
    UI_EVENT_CALIBRATION_BEGIN,
    UI_EVENT_CALIBRATION_DONE,
    UI_EVENT_CALIBRATION_FAILED,
    UI_EVENT_TELEMETRY_UPDATED
} ui_event_t;

typedef struct
{
    bool sensor_ready;
    bool model_trained;
    bool anomaly_ready;
    bool monitoring;
    edge_ai_health_t health;
    uint32_t window_id;
    int32_t temperature_millideg_c;
    int32_t temperature_rise_millideg_c;
    uint16_t anomaly_score_x100;
    uint16_t confidence_per_mille;
    uint32_t inference_us;
    uint32_t baseline_windows;
    uint8_t abnormal_streak;
    uint8_t normal_streak;
    char class_name[16];
    int16_t waveform[UI_WAVEFORM_POINTS];
    edge_ai_explanation_t explanations[EDGE_AI_EXPLANATION_COUNT];
    edge_ai_event_t events[EDGE_AI_EVENT_CAPACITY];
    uint8_t event_count;
} ui_telemetry_t;

typedef struct
{
    ui_page_t page;
    ui_telemetry_t telemetry;
    bool calibrating;
    bool calibration_failed;
    uint8_t calibration_current;
    uint8_t calibration_total;
    bool dirty;
} ui_state_machine_t;

void ui_state_machine_init(ui_state_machine_t * state);
void ui_state_machine_dispatch(ui_state_machine_t * state, ui_event_t event);
void ui_state_machine_set_telemetry(ui_state_machine_t * state, ui_telemetry_t const * telemetry);
void ui_state_machine_set_calibration_progress(ui_state_machine_t * state, uint8_t current, uint8_t total);
bool ui_state_machine_take_dirty(ui_state_machine_t * state);

#endif /* UI_STATE_MACHINE_H */

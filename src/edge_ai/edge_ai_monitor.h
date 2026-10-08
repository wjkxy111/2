#ifndef EDGE_AI_MONITOR_H
#define EDGE_AI_MONITOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../vibration/vibration_anomaly.h"

#define EDGE_AI_EVENT_CAPACITY       (8U)
#define EDGE_AI_EXPLANATION_COUNT    (3U)
#define EDGE_AI_MODEL_VERSION        "vib-temp-1.1"
#define EDGE_AI_TELEMETRY_SCHEMA     "edge-ai/1"

typedef enum
{
    EDGE_AI_HEALTH_OK = 0,
    EDGE_AI_HEALTH_WARN,
    EDGE_AI_HEALTH_ALARM,
    EDGE_AI_HEALTH_SENSOR_ERROR
} edge_ai_health_t;

typedef struct
{
    uint32_t window_id;
    edge_ai_health_t health;
    uint16_t score_x100;
} edge_ai_event_t;

typedef struct
{
    uint8_t feature_index;
    uint16_t contribution_x100;
} edge_ai_explanation_t;

typedef struct
{
    edge_ai_health_t health;
    uint8_t abnormal_streak;
    uint8_t alarm_streak; /* Consecutive severe windows, excluding warnings. */
    uint8_t normal_streak;
    edge_ai_event_t events[EDGE_AI_EVENT_CAPACITY];
    uint8_t event_head;
    uint8_t event_count;
} edge_ai_monitor_t;

void edge_ai_monitor_init(edge_ai_monitor_t * monitor);
edge_ai_health_t edge_ai_monitor_update(edge_ai_monitor_t * monitor,
                                        bool raw_alarm,
                                        bool raw_warning,
                                        bool sensor_error,
                                        uint32_t window_id,
                                        uint16_t score_x100);
uint8_t edge_ai_monitor_copy_events(edge_ai_monitor_t const * monitor,
                                    edge_ai_event_t output[EDGE_AI_EVENT_CAPACITY]);
void edge_ai_explain(vibration_anomaly_model_t const * model,
                     float const features[VIBRATION_FEATURE_COUNT],
                     edge_ai_explanation_t output[EDGE_AI_EXPLANATION_COUNT]);
char const * edge_ai_health_name(edge_ai_health_t health);

#endif /* EDGE_AI_MONITOR_H */

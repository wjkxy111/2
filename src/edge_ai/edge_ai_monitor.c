#include "edge_ai_monitor.h"

#include <string.h>

#define EDGE_AI_ALARM_CONFIRM_WINDOWS (3U)
#define EDGE_AI_WARN_CONFIRM_WINDOWS  (2U)
#define EDGE_AI_CLEAR_WINDOWS         (5U)

static void add_event(edge_ai_monitor_t * monitor, uint32_t window_id, uint16_t score_x100)
{
    monitor->events[monitor->event_head].window_id = window_id;
    monitor->events[monitor->event_head].health = monitor->health;
    monitor->events[monitor->event_head].score_x100 = score_x100;
    monitor->event_head = (uint8_t) ((monitor->event_head + 1U) % EDGE_AI_EVENT_CAPACITY);
    if (monitor->event_count < EDGE_AI_EVENT_CAPACITY)
    {
        monitor->event_count++;
    }
}

void edge_ai_monitor_init(edge_ai_monitor_t * monitor)
{
    if (NULL != monitor)
    {
        memset(monitor, 0, sizeof(*monitor));
        monitor->health = EDGE_AI_HEALTH_OK;
    }
}

edge_ai_health_t edge_ai_monitor_update(edge_ai_monitor_t * monitor,
                                        bool raw_alarm,
                                        bool raw_warning,
                                        bool sensor_error,
                                        uint32_t window_id,
                                        uint16_t score_x100)
{
    if (NULL == monitor)
    {
        return EDGE_AI_HEALTH_SENSOR_ERROR;
    }

    edge_ai_health_t const previous = monitor->health;
    if (sensor_error)
    {
        monitor->health = EDGE_AI_HEALTH_SENSOR_ERROR;
        monitor->abnormal_streak = 0U;
        monitor->normal_streak = 0U;
    }
    else if (raw_alarm)
    {
        monitor->normal_streak = 0U;
        if (monitor->abnormal_streak < UINT8_MAX)
        {
            monitor->abnormal_streak++;
        }
        if (monitor->abnormal_streak >= EDGE_AI_ALARM_CONFIRM_WINDOWS)
        {
            monitor->health = EDGE_AI_HEALTH_ALARM;
        }
    }
    else if (raw_warning)
    {
        monitor->normal_streak = 0U;
        if (monitor->abnormal_streak < UINT8_MAX)
        {
            monitor->abnormal_streak++;
        }
        if ((EDGE_AI_HEALTH_ALARM != monitor->health) &&
            (monitor->abnormal_streak >= EDGE_AI_WARN_CONFIRM_WINDOWS))
        {
            monitor->health = EDGE_AI_HEALTH_WARN;
        }
    }
    else
    {
        monitor->abnormal_streak = 0U;
        if (monitor->normal_streak < UINT8_MAX)
        {
            monitor->normal_streak++;
        }
        if (monitor->normal_streak >= EDGE_AI_CLEAR_WINDOWS)
        {
            monitor->health = EDGE_AI_HEALTH_OK;
        }
    }

    if (previous != monitor->health)
    {
        add_event(monitor, window_id, score_x100);
    }
    return monitor->health;
}

uint8_t edge_ai_monitor_copy_events(edge_ai_monitor_t const * monitor,
                                    edge_ai_event_t output[EDGE_AI_EVENT_CAPACITY])
{
    if ((NULL == monitor) || (NULL == output))
    {
        return 0U;
    }

    uint8_t const count = monitor->event_count;
    for (uint8_t i = 0U; i < count; i++)
    {
        uint8_t const index = (uint8_t) ((monitor->event_head + EDGE_AI_EVENT_CAPACITY - 1U - i) %
                                         EDGE_AI_EVENT_CAPACITY);
        output[i] = monitor->events[index];
    }
    return count;
}

void edge_ai_explain(vibration_anomaly_model_t const * model,
                     float const features[VIBRATION_FEATURE_COUNT],
                     edge_ai_explanation_t output[EDGE_AI_EXPLANATION_COUNT])
{
    memset(output, 0, sizeof(edge_ai_explanation_t) * EDGE_AI_EXPLANATION_COUNT);
    if ((!vibration_anomaly_ready(model)) || (NULL == features))
    {
        return;
    }

    for (size_t i = 0U; i < VIBRATION_FEATURE_COUNT; i++)
    {
        float variance = model->m2[i] / (float) (model->count - 1U);
        float const floor = (model->mean[i] * model->mean[i] * 0.0001f) + 1.0e-8f;
        if (variance < floor)
        {
            variance = floor;
        }
        float const delta = features[i] - model->mean[i];
        float contribution = (delta * delta) / variance;
        if (contribution > 25.0f)
        {
            contribution = 25.0f;
        }
        uint16_t const scaled = (uint16_t) (contribution * 100.0f);

        for (uint8_t rank = 0U; rank < EDGE_AI_EXPLANATION_COUNT; rank++)
        {
            if (scaled > output[rank].contribution_x100)
            {
                for (uint8_t move = EDGE_AI_EXPLANATION_COUNT - 1U; move > rank; move--)
                {
                    output[move] = output[move - 1U];
                }
                output[rank].feature_index = (uint8_t) i;
                output[rank].contribution_x100 = scaled;
                break;
            }
        }
    }
}

char const * edge_ai_health_name(edge_ai_health_t health)
{
    switch (health)
    {
        case EDGE_AI_HEALTH_OK:           return "OK";
        case EDGE_AI_HEALTH_WARN:         return "WARN";
        case EDGE_AI_HEALTH_ALARM:        return "ALARM";
        case EDGE_AI_HEALTH_SENSOR_ERROR: return "SENSOR";
        default:                          return "UNKNOWN";
    }
}

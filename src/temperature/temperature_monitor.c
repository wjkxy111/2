#include "temperature_monitor.h"

#include <stddef.h>

void temperature_monitor_reset(temperature_monitor_t * monitor)
{
    if (NULL == monitor)
    {
        return;
    }

    monitor->current_millideg_c  = 0;
    monitor->baseline_millideg_c = 0;
    monitor->baseline_sum        = 0;
    monitor->baseline_count      = 0U;
    monitor->has_value           = false;
    monitor->calibrating         = false;
}

void temperature_monitor_begin_baseline(temperature_monitor_t * monitor)
{
    if (NULL == monitor)
    {
        return;
    }

    monitor->baseline_sum   = 0;
    monitor->baseline_count = 0U;
    monitor->calibrating    = true;
}

void temperature_monitor_update(temperature_monitor_t * monitor, int32_t temperature_millideg_c)
{
    if (NULL == monitor)
    {
        return;
    }

    if (!monitor->has_value)
    {
        monitor->current_millideg_c  = temperature_millideg_c;
        monitor->baseline_millideg_c = temperature_millideg_c;
        monitor->has_value           = true;
    }
    else
    {
        /* A small integer EMA removes single-reading quantization steps. */
        monitor->current_millideg_c =
            (int32_t) (((int64_t) monitor->current_millideg_c * 3 + temperature_millideg_c) / 4);
    }

    if (monitor->calibrating)
    {
        monitor->baseline_sum += monitor->current_millideg_c;
        monitor->baseline_count++;
        monitor->baseline_millideg_c =
            (int32_t) (monitor->baseline_sum / (int64_t) monitor->baseline_count);
    }
}

void temperature_monitor_finish_baseline(temperature_monitor_t * monitor)
{
    if (NULL == monitor)
    {
        return;
    }

    if (monitor->baseline_count > 0U)
    {
        monitor->baseline_millideg_c =
            (int32_t) (monitor->baseline_sum / (int64_t) monitor->baseline_count);
    }
    monitor->calibrating = false;
}

int32_t temperature_monitor_current(temperature_monitor_t const * monitor)
{
    return ((NULL != monitor) && monitor->has_value) ? monitor->current_millideg_c : 0;
}

int32_t temperature_monitor_baseline(temperature_monitor_t const * monitor)
{
    return ((NULL != monitor) && monitor->has_value) ? monitor->baseline_millideg_c : 0;
}

int32_t temperature_monitor_rise(temperature_monitor_t const * monitor)
{
    return ((NULL != monitor) && monitor->has_value) ?
           (monitor->current_millideg_c - monitor->baseline_millideg_c) : 0;
}

temperature_level_t temperature_monitor_level(temperature_monitor_t const * monitor)
{
    int32_t const current = temperature_monitor_current(monitor);
    int32_t const rise = temperature_monitor_rise(monitor);

    if ((current >= TEMPERATURE_ALARM_MILLIDEG_C) ||
        (rise >= TEMPERATURE_RISE_ALARM_MILLIDEG_C))
    {
        return TEMPERATURE_LEVEL_ALARM;
    }
    if ((current >= TEMPERATURE_WARN_MILLIDEG_C) ||
        (rise >= TEMPERATURE_RISE_WARN_MILLIDEG_C))
    {
        return TEMPERATURE_LEVEL_WARN;
    }
    return TEMPERATURE_LEVEL_NORMAL;
}

char const * temperature_monitor_level_name(temperature_level_t level)
{
    static char const * const names[] = {"NORMAL", "WARN", "ALARM"};
    return ((uint32_t) level < (sizeof(names) / sizeof(names[0]))) ? names[level] : "UNKNOWN";
}

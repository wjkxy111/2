#ifndef TEMPERATURE_MONITOR_H
#define TEMPERATURE_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

/* Project defaults. Tune these limits for the actual machine and mounting. */
#define TEMPERATURE_WARN_MILLIDEG_C        (60000)
#define TEMPERATURE_ALARM_MILLIDEG_C       (70000)
#define TEMPERATURE_RISE_WARN_MILLIDEG_C   (15000)
#define TEMPERATURE_RISE_ALARM_MILLIDEG_C  (25000)

typedef enum
{
    TEMPERATURE_LEVEL_NORMAL = 0,
    TEMPERATURE_LEVEL_WARN,
    TEMPERATURE_LEVEL_ALARM
} temperature_level_t;

typedef struct
{
    int32_t current_millideg_c;
    int32_t baseline_millideg_c;
    int64_t baseline_sum;
    uint32_t baseline_count;
    bool has_value;
    bool calibrating;
} temperature_monitor_t;

void temperature_monitor_reset(temperature_monitor_t * monitor);
void temperature_monitor_begin_baseline(temperature_monitor_t * monitor);
void temperature_monitor_update(temperature_monitor_t * monitor, int32_t temperature_millideg_c);
void temperature_monitor_finish_baseline(temperature_monitor_t * monitor);
int32_t temperature_monitor_current(temperature_monitor_t const * monitor);
int32_t temperature_monitor_baseline(temperature_monitor_t const * monitor);
int32_t temperature_monitor_rise(temperature_monitor_t const * monitor);
temperature_level_t temperature_monitor_level(temperature_monitor_t const * monitor);
char const * temperature_monitor_level_name(temperature_level_t level);

#endif /* TEMPERATURE_MONITOR_H */

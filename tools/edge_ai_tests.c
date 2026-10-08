#include "../src/edge_ai/edge_ai_monitor.h"
#include <assert.h>
#include <stdio.h>

static edge_ai_health_t step(edge_ai_monitor_t *m, bool alarm, bool warning, bool fault)
{
    return edge_ai_monitor_update(m, alarm, warning, fault, 1U, 100U);
}

int main(void)
{
    edge_ai_monitor_t m;
    edge_ai_monitor_init(&m);
    assert(step(&m, false, true, false) == EDGE_AI_HEALTH_OK);
    assert(step(&m, false, true, false) == EDGE_AI_HEALTH_WARN);
    /* Two warnings must not count as two severe alarms. */
    assert(step(&m, true, false, false) == EDGE_AI_HEALTH_WARN);
    assert(m.alarm_streak == 1U);
    assert(step(&m, false, true, false) == EDGE_AI_HEALTH_WARN);
    assert(m.alarm_streak == 0U);
    assert(step(&m, true, false, false) == EDGE_AI_HEALTH_WARN);
    assert(step(&m, true, false, false) == EDGE_AI_HEALTH_WARN);
    assert(step(&m, true, false, false) == EDGE_AI_HEALTH_ALARM);
    assert(step(&m, false, true, false) == EDGE_AI_HEALTH_ALARM);
    for (unsigned int i = 0U; i < 4U; ++i)
    {
        assert(step(&m, false, false, false) == EDGE_AI_HEALTH_ALARM);
    }
    assert(step(&m, false, false, false) == EDGE_AI_HEALTH_OK);
    step(&m, true, false, false);
    step(&m, true, false, false);
    assert(step(&m, true, true, true) == EDGE_AI_HEALTH_SENSOR_ERROR);
    assert(m.alarm_streak == 0U && m.abnormal_streak == 0U);
    for (unsigned int i = 0U; i < 4U; ++i)
    {
        assert(step(&m, false, false, false) == EDGE_AI_HEALTH_SENSOR_ERROR);
    }
    assert(step(&m, false, false, false) == EDGE_AI_HEALTH_OK);
    for (unsigned int i = 0U; i < 300U; ++i)
    {
        step(&m, true, false, false);
    }
    assert(m.alarm_streak == UINT8_MAX && m.abnormal_streak == UINT8_MAX);
    edge_ai_event_t events[EDGE_AI_EVENT_CAPACITY];
    assert(edge_ai_monitor_copy_events(&m, events) > 0U);
    assert(events[0].health == EDGE_AI_HEALTH_ALARM);
    puts("BMI088 alarm confirmation, warning interruption, recovery and saturation passed");
    return 0;
}

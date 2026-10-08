#include "ui_state_machine.h"

#include <stddef.h>
#include <string.h>

void ui_state_machine_init(ui_state_machine_t * state)
{
    if (NULL == state)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->page = UI_PAGE_BOOT;
    state->telemetry.health = EDGE_AI_HEALTH_OK;
    state->dirty = true;
}

void ui_state_machine_dispatch(ui_state_machine_t * state, ui_event_t event)
{
    if (NULL == state)
    {
        return;
    }

    switch (event)
    {
        case UI_EVENT_BOOT_OK:
            state->page = UI_PAGE_DASHBOARD;
            state->telemetry.sensor_ready = true;
            break;

        case UI_EVENT_SENSOR_ERROR:
            state->telemetry.sensor_ready = false;
            state->telemetry.health = EDGE_AI_HEALTH_SENSOR_ERROR;
            break;

        case UI_EVENT_BUTTON_SHORT:
            if (!state->calibrating)
            {
                if ((state->page < UI_PAGE_DASHBOARD) || (state->page >= UI_PAGE_EVENT_LOG))
                {
                    state->page = UI_PAGE_DASHBOARD;
                }
                else
                {
                    state->page = (ui_page_t) (state->page + 1);
                }
            }
            break;

        case UI_EVENT_CALIBRATION_BEGIN:
            state->calibrating = true;
            state->calibration_failed = false;
            state->calibration_current = 0U;
            break;

        case UI_EVENT_CALIBRATION_DONE:
            state->calibrating = false;
            state->calibration_failed = false;
            state->page = UI_PAGE_AI_DIAGNOSTICS;
            break;

        case UI_EVENT_CALIBRATION_FAILED:
            state->calibrating = false;
            state->calibration_failed = true;
            state->page = UI_PAGE_DASHBOARD;
            break;

        case UI_EVENT_TELEMETRY_UPDATED:
        default:
            break;
    }
    state->dirty = true;
}

void ui_state_machine_set_telemetry(ui_state_machine_t * state, ui_telemetry_t const * telemetry)
{
    if ((NULL != state) && (NULL != telemetry))
    {
        state->telemetry = *telemetry;
        state->dirty = true;
    }
}

void ui_state_machine_set_calibration_progress(ui_state_machine_t * state, uint8_t current, uint8_t total)
{
    if (NULL != state)
    {
        state->calibration_current = current;
        state->calibration_total = total;
        state->dirty = true;
    }
}

bool ui_state_machine_take_dirty(ui_state_machine_t * state)
{
    if ((NULL != state) && state->dirty)
    {
        state->dirty = false;
        return true;
    }
    return false;
}

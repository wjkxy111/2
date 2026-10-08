#include "display_port.h"

#include <stddef.h>
#include <string.h>

#include "hal_data.h"
#include "r_mipi_dsi.h"
#include "board_sdram.h"

#define PANEL_RESET_PIN      BSP_IO_PORT_00_PIN_00
#define PANEL_BACKLIGHT_PIN  BSP_IO_PORT_00_PIN_01
#define DISPLAY_CMD_DELAY    ((mipi_dsi_cmd_id_t) 0xFE)
#define DISPLAY_CMD_END      ((mipi_dsi_cmd_id_t) 0xFD)
#define DISPLAY_CMD_TIMEOUT  (8000000UL)
#define DISPLAY_SWAP_TIMEOUT (50000000UL)
#define DISPLAY_BUFFER_COUNT (2U)

typedef struct
{
    uint8_t size;
    uint8_t data[15];
    mipi_dsi_cmd_id_t command;
    mipi_dsi_cmd_flag_t flags;
} panel_command_t;

static volatile bool g_command_complete;
static volatile uint32_t g_frame_epoch;
static bool g_display_ready;
static uint8_t g_draw_buffer_index;

/* ST7796U setup supplied with the CPKEXP-EKRA8X1 H0233S001 display example. */
static panel_command_t const g_panel_init[] =
{
    {2,  {0x11, 0x00}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {120, {0},          DISPLAY_CMD_DELAY,                       (mipi_dsi_cmd_flag_t) 0},
    {2,  {0xF0, 0xC3}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0xF0, 0x96}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0x36, 0x48}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0x3A, 0x55}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0xB4, 0x01}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {4,  {0xB6, 0x8A, 0x07, 0x3B}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0xB7, 0xC6}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {3,  {0xB9, 0x02, 0xE0}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {3,  {0xC0, 0xC0, 0x64}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0xC1, 0x1D}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0xC2, 0xA7}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0xC5, 0x18}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {9,  {0xE8, 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33},
         MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {15, {0xE0, 0xF0, 0x0B, 0x12, 0x09, 0x0A, 0x26, 0x39, 0x54, 0x4E, 0x38, 0x13, 0x13, 0x2E, 0x34},
         MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {15, {0xE1, 0xF0, 0x10, 0x15, 0x0D, 0x0C, 0x07, 0x38, 0x43, 0x4D, 0x3A, 0x16, 0x15, 0x30, 0x35},
         MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0xF0, 0x3C}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0xF0, 0x69}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0x35, 0x00}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_1_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0x29, 0x00}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0x21, 0x00}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {5,  {0x2A, 0x00, 0x31, 0x01, 0x0E}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {5,  {0x2B, 0x00, 0x00, 0x01, 0xDF}, MIPI_DSI_CMD_ID_DCS_LONG_WRITE, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {2,  {0x2C, 0x00}, MIPI_DSI_CMD_ID_DCS_SHORT_WRITE_0_PARAM, MIPI_DSI_CMD_FLAG_LOW_POWER},
    {0,  {0}, DISPLAY_CMD_END, (mipi_dsi_cmd_flag_t) 0}
};

static void panel_hardware_reset(void)
{
    R_IOPORT_PinWrite(&g_ioport_ctrl, PANEL_BACKLIGHT_PIN, BSP_IO_LEVEL_LOW);
    R_IOPORT_PinWrite(&g_ioport_ctrl, PANEL_RESET_PIN, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(100U, BSP_DELAY_UNITS_MILLISECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, PANEL_RESET_PIN, BSP_IO_LEVEL_LOW);
    R_BSP_SoftwareDelay(5U, BSP_DELAY_UNITS_MILLISECONDS);
    R_IOPORT_PinWrite(&g_ioport_ctrl, PANEL_RESET_PIN, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(120U, BSP_DELAY_UNITS_MILLISECONDS);
}

static bool panel_push_table(void)
{
    for (panel_command_t const * entry = g_panel_init; DISPLAY_CMD_END != entry->command; entry++)
    {
        if (DISPLAY_CMD_DELAY == entry->command)
        {
            R_BSP_SoftwareDelay(entry->size, BSP_DELAY_UNITS_MILLISECONDS);
            continue;
        }

        mipi_dsi_cmd_t command =
        {
            .channel     = 0,
            .cmd_id      = (mipi_cmd_id_t) entry->command,
            .flags       = entry->flags,
            .tx_len      = entry->size,
            .p_tx_buffer = entry->data
        };

        g_command_complete = false;
        if (FSP_SUCCESS != R_MIPI_DSI_Command(&g_mipi_dsi0_ctrl, &command))
        {
            return false;
        }

        uint32_t timeout = DISPLAY_CMD_TIMEOUT;
        while ((!g_command_complete) && (timeout > 0U))
        {
            timeout--;
            __NOP();
        }
        if (0U == timeout)
        {
            return false;
        }
    }
    return true;
}

bool display_port_init(void)
{
    g_display_ready = false;
    g_frame_epoch = 0U;
    g_draw_buffer_index = 1U;
    bsp_sdram_init();
    panel_hardware_reset();

    memset(fb_background, 0, sizeof(fb_background));
#if BSP_CFG_DCACHE_ENABLED
    SCB_CleanDCache_by_Addr(fb_background[0], sizeof(fb_background));
#endif
    if (FSP_SUCCESS != R_GLCDC_Open(&g_display_ctrl, &g_display_cfg))
    {
        return false;
    }
    if (!panel_push_table())
    {
        return false;
    }
    if (FSP_SUCCESS != R_GLCDC_Start(&g_display_ctrl))
    {
        return false;
    }

    R_IOPORT_PinWrite(&g_ioport_ctrl, PANEL_BACKLIGHT_PIN, BSP_IO_LEVEL_HIGH);
    g_display_ready = true;
    return true;
}

bool display_port_ready(void)
{
    return g_display_ready;
}

uint16_t * display_port_framebuffer(void)
{
    return (uint16_t *) &fb_background[g_draw_buffer_index][0];
}

void display_port_flush(void)
{
    if (!g_display_ready)
    {
        return;
    }

#if BSP_CFG_DCACHE_ENABLED
    SCB_CleanDCache_by_Addr(fb_background[g_draw_buffer_index], sizeof(fb_background[0]));
#endif

    /* Draw into the hidden buffer and only expose the complete frame at VSync.
     * This avoids GLCDC scanning a buffer while the CPU is updating it. */
    uint32_t const previous_epoch = g_frame_epoch;
    uint32_t timeout = DISPLAY_SWAP_TIMEOUT;
    fsp_err_t result;
    do
    {
        result = R_GLCDC_BufferChange(&g_display_ctrl,
                                      fb_background[g_draw_buffer_index],
                                      DISPLAY_FRAME_LAYER_1);
        if (FSP_ERR_INVALID_UPDATE_TIMING == result)
        {
            __NOP();
        }
    } while ((FSP_ERR_INVALID_UPDATE_TIMING == result) && (--timeout > 0U));

    if (FSP_SUCCESS == result)
    {
        timeout = DISPLAY_SWAP_TIMEOUT;
        while ((g_frame_epoch == previous_epoch) && (--timeout > 0U))
        {
            __NOP();
        }
        g_draw_buffer_index = (uint8_t) ((g_draw_buffer_index + 1U) % DISPLAY_BUFFER_COUNT);
    }
}

void display_port_keep_awake(void)
{
    if (g_display_ready)
    {
        R_IOPORT_PinWrite(&g_ioport_ctrl, PANEL_BACKLIGHT_PIN, BSP_IO_LEVEL_HIGH);
    }
}

void glcdc_callback(display_callback_args_t * p_args)
{
    if ((NULL != p_args) && (DISPLAY_EVENT_LINE_DETECTION == p_args->event))
    {
        g_frame_epoch++;
    }
}

void mipi_dsi_callback(mipi_dsi_callback_args_t * p_args)
{
    if ((MIPI_DSI_EVENT_SEQUENCE_0 == p_args->event) &&
        (MIPI_DSI_SEQUENCE_STATUS_DESCRIPTORS_FINISHED == p_args->tx_status))
    {
        g_command_complete = true;
    }
}

void gpt_callback(timer_callback_args_t * p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
}

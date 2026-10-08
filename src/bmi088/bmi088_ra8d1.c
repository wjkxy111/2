#include "bmi088_ra8d1.h"

#include <stdbool.h>
#include <string.h>

#include "hal_data.h"
#include "../vendor/bmi08x/bmi08x.h"

#define BMI088_ACC_CS_PIN          BSP_IO_PORT_05_PIN_04 /* Arduino D8 */
#define BMI088_GYRO_CS_PIN         BSP_IO_PORT_09_PIN_07 /* Arduino D3; D9/PA07 is SDRAM DQ21 */
#define BMI088_SPI_BUFFER_SIZE     (64U)
#define BMI088_SPI_TIMEOUT_LOOPS   (10000000UL)
#define BMI088_PORT_COM_ERROR      ((int8_t) -32)

typedef enum
{
    BMI088_PORT_ACCEL = 0,
    BMI088_PORT_GYRO  = 1
} bmi088_port_device_t;

static struct bmi08_dev g_bmi088;
static bmi088_port_device_t g_accel_device = BMI088_PORT_ACCEL;
static bmi088_port_device_t g_gyro_device  = BMI088_PORT_GYRO;
static volatile bool g_spi_complete;
static volatile bool g_spi_error;

static bsp_io_port_pin_t bmi088_cs_pin(void const * intf_ptr)
{
    bmi088_port_device_t const device = *((bmi088_port_device_t const *) intf_ptr);
    return (BMI088_PORT_ACCEL == device) ? BMI088_ACC_CS_PIN : BMI088_GYRO_CS_PIN;
}

static int8_t bmi088_transfer(uint8_t const * tx, uint8_t * rx, uint32_t length, void const * intf_ptr)
{
    fsp_err_t err;
    uint32_t timeout = BMI088_SPI_TIMEOUT_LOOPS;
    bsp_io_port_pin_t const cs_pin = bmi088_cs_pin(intf_ptr);

    g_spi_complete = false;
    g_spi_error    = false;

    R_IOPORT_PinWrite(&g_ioport_ctrl, cs_pin, BSP_IO_LEVEL_LOW);
    err = g_spi1.p_api->writeRead(g_spi1.p_ctrl, tx, rx, length, SPI_BIT_WIDTH_8_BITS);
    if (FSP_SUCCESS != err)
    {
        R_IOPORT_PinWrite(&g_ioport_ctrl, cs_pin, BSP_IO_LEVEL_HIGH);
        return BMI088_PORT_COM_ERROR;
    }

    while ((!g_spi_complete) && (timeout > 0U))
    {
        timeout--;
        __NOP();
    }

    R_IOPORT_PinWrite(&g_ioport_ctrl, cs_pin, BSP_IO_LEVEL_HIGH);
    return ((timeout > 0U) && (!g_spi_error)) ? BMI08_INTF_RET_SUCCESS : BMI088_PORT_COM_ERROR;
}

static BMI08_INTF_RET_TYPE bmi088_spi_read(uint8_t reg_addr,
                                           uint8_t * reg_data,
                                           uint32_t length,
                                           void * intf_ptr)
{
    uint8_t tx[BMI088_SPI_BUFFER_SIZE];
    uint8_t rx[BMI088_SPI_BUFFER_SIZE];

    if ((NULL == reg_data) || (NULL == intf_ptr) || (0U == length) ||
        ((length + 1U) > BMI088_SPI_BUFFER_SIZE))
    {
        return BMI088_PORT_COM_ERROR;
    }

    memset(tx, 0, length + 1U);
    tx[0] = (uint8_t) (reg_addr | 0x80U);

    int8_t const result = bmi088_transfer(tx, rx, length + 1U, intf_ptr);
    if (BMI08_INTF_RET_SUCCESS == result)
    {
        /* rx[0] was shifted in while the register address was sent.  For the
         * accelerometer, rx[1] is the additional dummy byte expected by the
         * Bosch driver; the gyroscope returns data immediately at rx[1]. */
        memcpy(reg_data, &rx[1], length);
    }

    return result;
}

static BMI08_INTF_RET_TYPE bmi088_spi_write(uint8_t reg_addr,
                                            uint8_t const * reg_data,
                                            uint32_t length,
                                            void * intf_ptr)
{
    uint8_t tx[BMI088_SPI_BUFFER_SIZE];
    uint8_t rx[BMI088_SPI_BUFFER_SIZE];

    if ((NULL == reg_data) || (NULL == intf_ptr) || (0U == length) ||
        ((length + 1U) > BMI088_SPI_BUFFER_SIZE))
    {
        return BMI088_PORT_COM_ERROR;
    }

    tx[0] = (uint8_t) (reg_addr & 0x7FU);
    memcpy(&tx[1], reg_data, length);
    return bmi088_transfer(tx, rx, length + 1U, intf_ptr);
}

static void bmi088_delay_us(uint32_t period_us, void * intf_ptr)
{
    FSP_PARAMETER_NOT_USED(intf_ptr);
    R_BSP_SoftwareDelay(period_us, BSP_DELAY_UNITS_MICROSECONDS);
}

void bmi088_spi_callback(spi_callback_args_t * p_args)
{
    if (SPI_EVENT_TRANSFER_COMPLETE == p_args->event)
    {
        g_spi_complete = true;
    }
    else
    {
        g_spi_error    = true;
        g_spi_complete = true;
    }
}

int8_t bmi088_ra8d1_init(void)
{
    int8_t result;

    R_IOPORT_PinWrite(&g_ioport_ctrl, BMI088_ACC_CS_PIN, BSP_IO_LEVEL_HIGH);
    R_IOPORT_PinWrite(&g_ioport_ctrl, BMI088_GYRO_CS_PIN, BSP_IO_LEVEL_HIGH);

    if (FSP_SUCCESS != g_spi1.p_api->open(g_spi1.p_ctrl, g_spi1.p_cfg))
    {
        return BMI088_PORT_COM_ERROR;
    }

    memset(&g_bmi088, 0, sizeof(g_bmi088));
    g_bmi088.intf           = BMI08_SPI_INTF;
    g_bmi088.variant        = BMI088_VARIANT;
    g_bmi088.intf_ptr_accel = &g_accel_device;
    g_bmi088.intf_ptr_gyro  = &g_gyro_device;
    g_bmi088.read           = bmi088_spi_read;
    g_bmi088.write          = bmi088_spi_write;
    g_bmi088.delay_us       = bmi088_delay_us;
    g_bmi088.read_write_len = 32U;

    /* bmi08xa_init performs the dummy read that switches the accelerometer
     * from its power-on I2C state into SPI mode. */
    result = bmi08xa_init(&g_bmi088);
    if (BMI08_OK == result)
    {
        result = bmi08g_init(&g_bmi088);
    }
    if (BMI08_OK == result)
    {
        result = bmi08a_load_config_file(&g_bmi088);
    }

    if (BMI08_OK == result)
    {
        g_bmi088.accel_cfg.odr   = BMI08_ACCEL_ODR_800_HZ;
        g_bmi088.accel_cfg.range = BMI088_ACCEL_RANGE_6G;
        g_bmi088.accel_cfg.bw    = BMI08_ACCEL_BW_NORMAL;
        g_bmi088.accel_cfg.power = BMI08_ACCEL_PM_ACTIVE;
        result = bmi08a_set_power_mode(&g_bmi088);
    }
    if (BMI08_OK == result)
    {
        result = bmi08xa_set_meas_conf(&g_bmi088);
    }

    if (BMI08_OK == result)
    {
        g_bmi088.gyro_cfg.odr   = BMI08_GYRO_BW_116_ODR_1000_HZ;
        g_bmi088.gyro_cfg.bw    = BMI08_GYRO_BW_116_ODR_1000_HZ;
        g_bmi088.gyro_cfg.range = BMI08_GYRO_RANGE_500_DPS;
        g_bmi088.gyro_cfg.power = BMI08_GYRO_PM_NORMAL;
        result = bmi08g_set_power_mode(&g_bmi088);
    }
    if (BMI08_OK == result)
    {
        result = bmi08g_set_meas_conf(&g_bmi088);
    }

    return result;
}

int8_t bmi088_ra8d1_read(bmi088_sample_t * sample)
{
    struct bmi08_sensor_data accel;
    struct bmi08_sensor_data gyro;
    int8_t result;

    if (NULL == sample)
    {
        return BMI08_E_NULL_PTR;
    }

    result = bmi08a_get_data(&accel, &g_bmi088);
    if (BMI08_OK == result)
    {
        result = bmi08g_get_data(&gyro, &g_bmi088);
    }
    if (BMI08_OK == result)
    {
        sample->ax = accel.x;
        sample->ay = accel.y;
        sample->az = accel.z;
        sample->gx = gyro.x;
        sample->gy = gyro.y;
        sample->gz = gyro.z;
    }

    return result;
}

int8_t bmi088_ra8d1_read_temperature(int32_t * temperature_millideg_c)
{
    if (NULL == temperature_millideg_c)
    {
        return BMI08_E_NULL_PTR;
    }

    return bmi08a_get_sensor_temperature(&g_bmi088, temperature_millideg_c);
}

uint8_t bmi088_ra8d1_accel_chip_id(void)
{
    return g_bmi088.accel_chip_id;
}

uint8_t bmi088_ra8d1_gyro_chip_id(void)
{
    return g_bmi088.gyro_chip_id;
}

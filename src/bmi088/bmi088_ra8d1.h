#ifndef BMI088_RA8D1_H
#define BMI088_RA8D1_H

#include <stdint.h>

#define BMI088_OK (0)

typedef struct
{
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
} bmi088_sample_t;

/** Initialize SPI1 and both dies inside the BMI088. */
int8_t bmi088_ra8d1_init(void);

/** Read one raw accelerometer/gyroscope sample. */
int8_t bmi088_ra8d1_read(bmi088_sample_t * sample);

/** Read the BMI088 accelerometer-die temperature in millidegrees Celsius. */
int8_t bmi088_ra8d1_read_temperature(int32_t * temperature_millideg_c);

uint8_t bmi088_ra8d1_accel_chip_id(void);
uint8_t bmi088_ra8d1_gyro_chip_id(void);

/** Bosch BMI088 configuration selected by this port. */
#define BMI088_ACCEL_RANGE_G       (6.0f)
#define BMI088_GYRO_RANGE_DPS      (500.0f)
#define BMI088_ACCEL_LSB_TO_G      (BMI088_ACCEL_RANGE_G / 32768.0f)
#define BMI088_GYRO_LSB_TO_DPS     (BMI088_GYRO_RANGE_DPS / 32768.0f)

#endif /* BMI088_RA8D1_H */

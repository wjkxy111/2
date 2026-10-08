#include "vibration_features.h"

#include <math.h>

#define FEATURE_EPSILON (1.0e-12f)
#define GOERTZEL_COUNT  (12U)

/* 2*cos(2*pi*f/800), for 12.5, 25, 50, 75, 100, 125, 150, 200,
 * 250, 300, 350 and 390 Hz. */
static float const g_goertzel_coefficients[GOERTZEL_COUNT] =
{
    1.990369453f,  1.961570561f,  1.847759065f,  1.662939225f,
    1.414213562f,  1.111140466f,  0.765366865f,  0.000000000f,
   -0.765366865f, -1.414213562f, -1.847759065f, -1.993834667f
};

static char const * const g_feature_names[VIBRATION_FEATURE_COUNT] =
{
    "acc_x_rms_g", "acc_y_rms_g", "acc_z_rms_g", "acc_mag_rms_g",
    "acc_mag_peak_g", "acc_crest", "acc_kurtosis", "acc_delta_rms_g",
    "gyro_x_rms_dps", "gyro_y_rms_dps", "gyro_z_rms_dps", "gyro_peak_dps",
    "spec_12p5", "spec_25", "spec_50", "spec_75", "spec_100", "spec_125",
    "spec_150", "spec_200", "spec_250", "spec_300", "spec_350", "spec_390",
    "temperature_c", "temperature_rise_c"
};

bool vibration_features_extract(bmi088_sample_t const samples[VIBRATION_WINDOW_SAMPLES],
                                int32_t temperature_millideg_c,
                                int32_t temperature_rise_millideg_c,
                                float features[VIBRATION_FEATURE_COUNT])
{
    float mean_ax = 0.0f;
    float mean_ay = 0.0f;
    float mean_az = 0.0f;
    float mean_mag = 0.0f;
    float mag[VIBRATION_WINDOW_SAMPLES];
    float s1[GOERTZEL_COUNT] = {0.0f};
    float s2[GOERTZEL_COUNT] = {0.0f};

    if ((NULL == samples) || (NULL == features))
    {
        return false;
    }

    for (size_t i = 0; i < VIBRATION_WINDOW_SAMPLES; i++)
    {
        float const ax = (float) samples[i].ax * BMI088_ACCEL_LSB_TO_G;
        float const ay = (float) samples[i].ay * BMI088_ACCEL_LSB_TO_G;
        float const az = (float) samples[i].az * BMI088_ACCEL_LSB_TO_G;
        mag[i] = sqrtf((ax * ax) + (ay * ay) + (az * az));
        mean_ax += ax;
        mean_ay += ay;
        mean_az += az;
        mean_mag += mag[i];
    }

    float const inv_count = 1.0f / (float) VIBRATION_WINDOW_SAMPLES;
    mean_ax  *= inv_count;
    mean_ay  *= inv_count;
    mean_az  *= inv_count;
    mean_mag *= inv_count;

    float axis_energy_x = 0.0f;
    float axis_energy_y = 0.0f;
    float axis_energy_z = 0.0f;
    float mag_energy = 0.0f;
    float mag_fourth = 0.0f;
    float mag_peak = 0.0f;
    float delta_energy = 0.0f;
    float gyro_energy_x = 0.0f;
    float gyro_energy_y = 0.0f;
    float gyro_energy_z = 0.0f;
    float gyro_peak = 0.0f;
    float previous_ax = 0.0f;
    float previous_ay = 0.0f;
    float previous_az = 0.0f;

    for (size_t i = 0; i < VIBRATION_WINDOW_SAMPLES; i++)
    {
        float const ax = ((float) samples[i].ax * BMI088_ACCEL_LSB_TO_G) - mean_ax;
        float const ay = ((float) samples[i].ay * BMI088_ACCEL_LSB_TO_G) - mean_ay;
        float const az = ((float) samples[i].az * BMI088_ACCEL_LSB_TO_G) - mean_az;
        float const centered_mag = mag[i] - mean_mag;
        float const abs_mag = fabsf(centered_mag);
        float const gx = (float) samples[i].gx * BMI088_GYRO_LSB_TO_DPS;
        float const gy = (float) samples[i].gy * BMI088_GYRO_LSB_TO_DPS;
        float const gz = (float) samples[i].gz * BMI088_GYRO_LSB_TO_DPS;
        float const gyro_magnitude = sqrtf((gx * gx) + (gy * gy) + (gz * gz));

        axis_energy_x += ax * ax;
        axis_energy_y += ay * ay;
        axis_energy_z += az * az;
        mag_energy += centered_mag * centered_mag;
        mag_fourth += centered_mag * centered_mag * centered_mag * centered_mag;
        if (abs_mag > mag_peak)
        {
            mag_peak = abs_mag;
        }

        if (i > 0U)
        {
            float const dx = ax - previous_ax;
            float const dy = ay - previous_ay;
            float const dz = az - previous_az;
            delta_energy += (dx * dx) + (dy * dy) + (dz * dz);
        }
        previous_ax = ax;
        previous_ay = ay;
        previous_az = az;

        gyro_energy_x += gx * gx;
        gyro_energy_y += gy * gy;
        gyro_energy_z += gz * gz;
        if (gyro_magnitude > gyro_peak)
        {
            gyro_peak = gyro_magnitude;
        }

        for (size_t band = 0; band < GOERTZEL_COUNT; band++)
        {
            float const s0 = centered_mag + (g_goertzel_coefficients[band] * s1[band]) - s2[band];
            s2[band] = s1[band];
            s1[band] = s0;
        }
    }

    features[0] = sqrtf(axis_energy_x * inv_count);
    features[1] = sqrtf(axis_energy_y * inv_count);
    features[2] = sqrtf(axis_energy_z * inv_count);
    features[3] = sqrtf(mag_energy * inv_count);
    features[4] = mag_peak;
    features[5] = mag_peak / (features[3] + FEATURE_EPSILON);
    features[6] = (mag_fourth * inv_count) /
                  ((features[3] * features[3] * features[3] * features[3]) + FEATURE_EPSILON);
    features[7] = sqrtf(delta_energy / (float) (VIBRATION_WINDOW_SAMPLES - 1U));
    features[8] = sqrtf(gyro_energy_x * inv_count);
    features[9] = sqrtf(gyro_energy_y * inv_count);
    features[10] = sqrtf(gyro_energy_z * inv_count);
    features[11] = gyro_peak;

    float const spectral_scale = inv_count * inv_count;
    for (size_t band = 0; band < GOERTZEL_COUNT; band++)
    {
        float power = (s1[band] * s1[band]) + (s2[band] * s2[band]) -
                      (g_goertzel_coefficients[band] * s1[band] * s2[band]);
        if (power < 0.0f)
        {
            power = 0.0f;
        }
        features[12U + band] = power * spectral_scale;
    }

    features[24] = (float) temperature_millideg_c / 1000.0f;
    features[25] = (float) temperature_rise_millideg_c / 1000.0f;

    return true;
}

char const * vibration_feature_name(size_t index)
{
    return (index < VIBRATION_FEATURE_COUNT) ? g_feature_names[index] : "invalid";
}

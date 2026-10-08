#ifndef VIBRATION_FEATURES_H
#define VIBRATION_FEATURES_H

#include <stdbool.h>
#include <stddef.h>

#include "../bmi088/bmi088_ra8d1.h"

#define VIBRATION_SAMPLE_RATE_HZ      (800U)
#define VIBRATION_WINDOW_SAMPLES      (512U)
#define VIBRATION_FEATURE_COUNT       (26U)

bool vibration_features_extract(bmi088_sample_t const samples[VIBRATION_WINDOW_SAMPLES],
                                int32_t temperature_millideg_c,
                                int32_t temperature_rise_millideg_c,
                                float features[VIBRATION_FEATURE_COUNT]);

char const * vibration_feature_name(size_t index);

#endif /* VIBRATION_FEATURES_H */

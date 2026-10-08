#ifndef VIBRATION_ANOMALY_H
#define VIBRATION_ANOMALY_H

#include <stdbool.h>
#include <stdint.h>

#include "vibration_features.h"

#define VIBRATION_BASELINE_MIN_WINDOWS (16U)
#define VIBRATION_ANOMALY_THRESHOLD     (9.0f)

typedef struct
{
    uint32_t count;
    float mean[VIBRATION_FEATURE_COUNT];
    float m2[VIBRATION_FEATURE_COUNT];
} vibration_anomaly_model_t;

void vibration_anomaly_reset(vibration_anomaly_model_t * model);
void vibration_anomaly_update(vibration_anomaly_model_t * model,
                              float const features[VIBRATION_FEATURE_COUNT]);
bool vibration_anomaly_ready(vibration_anomaly_model_t const * model);
float vibration_anomaly_score(vibration_anomaly_model_t const * model,
                              float const features[VIBRATION_FEATURE_COUNT]);

#endif /* VIBRATION_ANOMALY_H */

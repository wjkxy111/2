#include "vibration_anomaly.h"

#include <stddef.h>
#include <string.h>

void vibration_anomaly_reset(vibration_anomaly_model_t * model)
{
    if (NULL != model)
    {
        memset(model, 0, sizeof(*model));
    }
}

void vibration_anomaly_update(vibration_anomaly_model_t * model,
                              float const features[VIBRATION_FEATURE_COUNT])
{
    if ((NULL == model) || (NULL == features))
    {
        return;
    }

    model->count++;
    float const count = (float) model->count;
    for (size_t i = 0; i < VIBRATION_FEATURE_COUNT; i++)
    {
        float const delta = features[i] - model->mean[i];
        model->mean[i] += delta / count;
        float const delta2 = features[i] - model->mean[i];
        model->m2[i] += delta * delta2;
    }
}

bool vibration_anomaly_ready(vibration_anomaly_model_t const * model)
{
    return (NULL != model) && (model->count >= VIBRATION_BASELINE_MIN_WINDOWS);
}

float vibration_anomaly_score(vibration_anomaly_model_t const * model,
                              float const features[VIBRATION_FEATURE_COUNT])
{
    if ((!vibration_anomaly_ready(model)) || (NULL == features))
    {
        return 0.0f;
    }

    float score = 0.0f;
    for (size_t i = 0; i < VIBRATION_FEATURE_COUNT; i++)
    {
        float variance = model->m2[i] / (float) (model->count - 1U);
        float const relative_floor = (model->mean[i] * model->mean[i] * 0.0001f) + 1.0e-8f;
        if (variance < relative_floor)
        {
            variance = relative_floor;
        }

        float const delta = features[i] - model->mean[i];
        float z2 = (delta * delta) / variance;
        if (z2 > 25.0f)
        {
            z2 = 25.0f;
        }
        score += z2;
    }

    return score / (float) VIBRATION_FEATURE_COUNT;
}

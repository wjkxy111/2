#include "vibration_model.h"

#include <math.h>
#include <stddef.h>

#include "vibration_model_data.h"

#if VIBRATION_MODEL_INPUTS != VIBRATION_FEATURE_COUNT
#error "Vibration model input count must match the firmware feature count"
#endif

bool vibration_model_is_trained(void)
{
    return (0 != VIBRATION_MODEL_TRAINED);
}

bool vibration_model_predict(float const features[VIBRATION_FEATURE_COUNT],
                             vibration_class_t * predicted_class,
                             float probabilities[VIBRATION_CLASS_COUNT])
{
    float hidden[VIBRATION_MODEL_HIDDEN];
    float logits[VIBRATION_MODEL_OUTPUTS];

    if ((!vibration_model_is_trained()) || (NULL == features) ||
        (NULL == predicted_class) || (NULL == probabilities))
    {
        return false;
    }

    for (size_t h = 0; h < VIBRATION_MODEL_HIDDEN; h++)
    {
        float value = g_vibration_hidden_bias[h];
        for (size_t i = 0; i < VIBRATION_MODEL_INPUTS; i++)
        {
            float const normalized = (features[i] - g_vibration_input_mean[i]) /
                                     g_vibration_input_scale[i];
            value += normalized * g_vibration_hidden_weights[(i * VIBRATION_MODEL_HIDDEN) + h];
        }
        hidden[h] = (value > 0.0f) ? value : 0.0f;
    }

    float max_logit = -3.402823466e+38f;
    for (size_t output = 0; output < VIBRATION_MODEL_OUTPUTS; output++)
    {
        float value = g_vibration_output_bias[output];
        for (size_t h = 0; h < VIBRATION_MODEL_HIDDEN; h++)
        {
            value += hidden[h] * g_vibration_output_weights[(h * VIBRATION_MODEL_OUTPUTS) + output];
        }
        logits[output] = value;
        if (value > max_logit)
        {
            max_logit = value;
        }
    }

    float probability_sum = 0.0f;
    size_t best = 0U;
    for (size_t output = 0; output < VIBRATION_MODEL_OUTPUTS; output++)
    {
        probabilities[output] = expf(logits[output] - max_logit);
        probability_sum += probabilities[output];
    }
    for (size_t output = 0; output < VIBRATION_MODEL_OUTPUTS; output++)
    {
        probabilities[output] /= probability_sum;
        if (probabilities[output] > probabilities[best])
        {
            best = output;
        }
    }

    *predicted_class = (vibration_class_t) best;
    return true;
}

char const * vibration_model_class_name(vibration_class_t class_id)
{
    static char const * const names[VIBRATION_CLASS_COUNT] =
    {
        "normal", "imbalance", "loose", "rub", "bearing"
    };

    return ((uint32_t) class_id < VIBRATION_CLASS_COUNT) ? names[class_id] : "unknown";
}

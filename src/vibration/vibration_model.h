#ifndef VIBRATION_MODEL_H
#define VIBRATION_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#include "vibration_features.h"

#define VIBRATION_CLASS_COUNT (5U)

typedef enum
{
    VIBRATION_CLASS_NORMAL = 0,
    VIBRATION_CLASS_IMBALANCE,
    VIBRATION_CLASS_LOOSE,
    VIBRATION_CLASS_RUB,
    VIBRATION_CLASS_BEARING
} vibration_class_t;

bool vibration_model_is_trained(void);
bool vibration_model_predict(float const features[VIBRATION_FEATURE_COUNT],
                             vibration_class_t * predicted_class,
                             float probabilities[VIBRATION_CLASS_COUNT]);
char const * vibration_model_class_name(vibration_class_t class_id);

#endif /* VIBRATION_MODEL_H */

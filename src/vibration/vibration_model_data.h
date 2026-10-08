#ifndef VIBRATION_MODEL_DATA_H
#define VIBRATION_MODEL_DATA_H

/* This seed file deliberately contains no trained weights.  Run
 * tools/train_vibration_model.py after collecting BMI088 logs; the script
 * replaces this file with a trained five-class MLP. */
#define VIBRATION_MODEL_TRAINED  (0)
#define VIBRATION_MODEL_INPUTS   (26)
#define VIBRATION_MODEL_HIDDEN   (12)
#define VIBRATION_MODEL_OUTPUTS  (5)

static const float g_vibration_input_mean[VIBRATION_MODEL_INPUTS] = {0.0f};
static const float g_vibration_input_scale[VIBRATION_MODEL_INPUTS] =
{
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f
};
static const float g_vibration_hidden_weights[VIBRATION_MODEL_INPUTS * VIBRATION_MODEL_HIDDEN] = {0.0f};
static const float g_vibration_hidden_bias[VIBRATION_MODEL_HIDDEN] = {0.0f};
static const float g_vibration_output_weights[VIBRATION_MODEL_HIDDEN * VIBRATION_MODEL_OUTPUTS] = {0.0f};
static const float g_vibration_output_bias[VIBRATION_MODEL_OUTPUTS] = {0.0f};

#endif /* VIBRATION_MODEL_DATA_H */

#include "uim_input_scale.h"

#include <math.h>

static bool scale_finite_positive(float value) {
    return isfinite(value) && value > 0.0f;
}

static uim_Result scale_store(float value, float minimum, float maximum, float *slot) {
    float clamped = value;
    uim_Result result = uim_OK;
    if (!scale_finite_positive(value)) return uim_ERR_ARGUMENT;
    if (clamped < minimum) {
        clamped = minimum;
        result = uim_ERR_RANGE;
    } else if (clamped > maximum) {
        clamped = maximum;
        result = uim_ERR_RANGE;
    }
    *slot = clamped;
    return result;
}

static uim_Result scale_validate(const uim_InputScale *scale) {
    if (!scale) return uim_ERR_ARGUMENT;
    if (!scale_finite_positive(scale->dpi_scale)) return uim_ERR_STATE;
    if (!scale_finite_positive(scale->sensitivity)) return uim_ERR_STATE;
    return uim_OK;
}

uim_Result uim_input_scale_init(uim_InputScale *scale, float dpi_scale, float sensitivity) {
    uim_Result dpi_result;
    uim_Result sensitivity_result;
    if (!scale) return uim_ERR_ARGUMENT;
    scale->dpi_scale = 1.0f;
    scale->sensitivity = 1.0f;
    dpi_result = uim_input_scale_set_dpi_scale(scale, dpi_scale);
    sensitivity_result = uim_input_scale_set_sensitivity(scale, sensitivity);
    if (dpi_result == uim_ERR_ARGUMENT || sensitivity_result == uim_ERR_ARGUMENT) {
        scale->dpi_scale = 1.0f;
        scale->sensitivity = 1.0f;
        return uim_ERR_ARGUMENT;
    }
    return (dpi_result == uim_OK && sensitivity_result == uim_OK) ? uim_OK : uim_ERR_RANGE;
}

uim_Result uim_input_scale_init_default(uim_InputScale *scale) {
    if (!scale) return uim_ERR_ARGUMENT;
    scale->dpi_scale = 1.0f;
    scale->sensitivity = 1.0f;
    return uim_OK;
}

uim_Result uim_input_scale_set_dpi_scale(uim_InputScale *scale, float dpi_scale) {
    if (!scale) return uim_ERR_ARGUMENT;
    return scale_store(dpi_scale, uim_INPUT_SCALE_DPI_MIN, uim_INPUT_SCALE_DPI_MAX,
                       &scale->dpi_scale);
}

uim_Result uim_input_scale_set_sensitivity(uim_InputScale *scale, float sensitivity) {
    if (!scale) return uim_ERR_ARGUMENT;
    return scale_store(sensitivity, uim_INPUT_SCALE_SENSITIVITY_MIN,
                       uim_INPUT_SCALE_SENSITIVITY_MAX, &scale->sensitivity);
}

uim_Result uim_input_scale_point(const uim_InputScale *scale,
                                 float raw_x, float raw_y,
                                 float *out_x, float *out_y) {
    uim_Result rc;
    if (!out_x || !out_y) return uim_ERR_ARGUMENT;
    rc = scale_validate(scale);
    if (rc != uim_OK) return rc;
    if (!isfinite(raw_x) || !isfinite(raw_y)) return uim_ERR_ARGUMENT;
    *out_x = (float)((double)raw_x / (double)scale->dpi_scale);
    *out_y = (float)((double)raw_y / (double)scale->dpi_scale);
    return uim_OK;
}

uim_Result uim_input_scale_point_inverse(const uim_InputScale *scale,
                                         float logical_x, float logical_y,
                                         float *out_x, float *out_y) {
    uim_Result rc;
    if (!out_x || !out_y) return uim_ERR_ARGUMENT;
    rc = scale_validate(scale);
    if (rc != uim_OK) return rc;
    if (!isfinite(logical_x) || !isfinite(logical_y)) return uim_ERR_ARGUMENT;
    *out_x = (float)((double)logical_x * (double)scale->dpi_scale);
    *out_y = (float)((double)logical_y * (double)scale->dpi_scale);
    return uim_OK;
}

uim_Result uim_input_scale_delta(const uim_InputScale *scale,
                                 float raw_dx, float raw_dy,
                                 float *out_dx, float *out_dy) {
    uim_Result rc;
    double gain;
    if (!out_dx || !out_dy) return uim_ERR_ARGUMENT;
    rc = scale_validate(scale);
    if (rc != uim_OK) return rc;
    if (!isfinite(raw_dx) || !isfinite(raw_dy)) return uim_ERR_ARGUMENT;
    gain = (double)scale->sensitivity / (double)scale->dpi_scale;
    *out_dx = (float)((double)raw_dx * gain);
    *out_dy = (float)((double)raw_dy * gain);
    return uim_OK;
}

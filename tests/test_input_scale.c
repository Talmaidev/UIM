/* Contract test for the UIM input scale: window-API physical pointer values are
 * mapped into UI logical units without touching any OS pointer setting, and the
 * game-input sensitivity is applied to deltas only. */
#include "uim_input_scale.h"

#include <math.h>
#include <stdio.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

#define CHECK_NEAR(actual, expected, eps) do { \
    double a_ = (double)(actual), e_ = (double)(expected); \
    if (fabs(a_ - e_) > (double)(eps)) { \
        printf("FAIL %d: %s = %.6f, expected %.6f\n", __LINE__, #actual, a_, e_); \
        failures++; \
    } \
} while (0)

int main(void) {
    uim_InputScale scale;
    float x = 0.0f, y = 0.0f;

    CHECK(uim_input_scale_init_default(&scale) == uim_OK);
    CHECK(scale.dpi_scale == 1.0f);
    CHECK(scale.sensitivity == 1.0f);

    /* 1:1 baseline: logical == physical. */
    CHECK(uim_input_scale_point(&scale, 100.0f, 50.0f, &x, &y) == uim_OK);
    CHECK_NEAR(x, 100.0f, 1e-5f);
    CHECK_NEAR(y, 50.0f, 1e-5f);
    CHECK(uim_input_scale_delta(&scale, 10.0f, -4.0f, &x, &y) == uim_OK);
    CHECK_NEAR(x, 10.0f, 1e-5f);
    CHECK_NEAR(y, -4.0f, 1e-5f);

    /* Window DPI 150%: 150 physical px == 100 logical units. */
    CHECK(uim_input_scale_set_dpi_scale(&scale, 1.5f) == uim_OK);
    CHECK(uim_input_scale_point(&scale, 150.0f, 75.0f, &x, &y) == uim_OK);
    CHECK_NEAR(x, 100.0f, 1e-5f);
    CHECK_NEAR(y, 50.0f, 1e-5f);
    CHECK(uim_input_scale_point_inverse(&scale, 100.0f, 50.0f, &x, &y) == uim_OK);
    CHECK_NEAR(x, 150.0f, 1e-5f);
    CHECK_NEAR(y, 75.0f, 1e-5f);

    /* Sensitivity is a delta gain only: position mapping stays 1:1. */
    CHECK(uim_input_scale_set_sensitivity(&scale, 2.0f) == uim_OK);
    CHECK(uim_input_scale_delta(&scale, 10.0f, -4.0f, &x, &y) == uim_OK);
    CHECK_NEAR(x, 13.333333f, 1e-3f);
    CHECK_NEAR(y, -5.333333f, 1e-3f);
    CHECK(uim_input_scale_point(&scale, 150.0f, 75.0f, &x, &y) == uim_OK);
    CHECK_NEAR(x, 100.0f, 1e-5f);
    CHECK_NEAR(y, 50.0f, 1e-5f);

    /* Zero motion must stay exactly zero. */
    CHECK(uim_input_scale_delta(&scale, 0.0f, 0.0f, &x, &y) == uim_OK);
    CHECK(x == 0.0f && y == 0.0f);

    /* Clamping is reported, rejection leaves the stored value untouched. */
    CHECK(uim_input_scale_set_dpi_scale(&scale, 0.1f) == uim_ERR_RANGE);
    CHECK_NEAR(scale.dpi_scale, uim_INPUT_SCALE_DPI_MIN, 1e-6f);
    CHECK(uim_input_scale_set_dpi_scale(&scale, 9.0f) == uim_ERR_RANGE);
    CHECK_NEAR(scale.dpi_scale, uim_INPUT_SCALE_DPI_MAX, 1e-6f);
    CHECK(uim_input_scale_set_dpi_scale(&scale, NAN) == uim_ERR_ARGUMENT);
    CHECK_NEAR(scale.dpi_scale, uim_INPUT_SCALE_DPI_MAX, 1e-6f);
    CHECK(uim_input_scale_set_dpi_scale(&scale, 0.0f) == uim_ERR_ARGUMENT);
    CHECK_NEAR(scale.dpi_scale, uim_INPUT_SCALE_DPI_MAX, 1e-6f);
    CHECK(uim_input_scale_set_sensitivity(&scale, 0.01f) == uim_ERR_RANGE);
    CHECK_NEAR(scale.sensitivity, uim_INPUT_SCALE_SENSITIVITY_MIN, 1e-6f);
    CHECK(uim_input_scale_set_sensitivity(&scale, -1.0f) == uim_ERR_ARGUMENT);
    CHECK_NEAR(scale.sensitivity, uim_INPUT_SCALE_SENSITIVITY_MIN, 1e-6f);
    CHECK(uim_input_scale_set_sensitivity(&scale, INFINITY) == uim_ERR_ARGUMENT);
    CHECK_NEAR(scale.sensitivity, uim_INPUT_SCALE_SENSITIVITY_MIN, 1e-6f);

    /* Argument and state guards. */
    CHECK(uim_input_scale_init(NULL, 1.0f, 1.0f) == uim_ERR_ARGUMENT);
    CHECK(uim_input_scale_point(NULL, 1.0f, 1.0f, &x, &y) == uim_ERR_ARGUMENT);
    CHECK(uim_input_scale_point(&scale, 1.0f, 1.0f, NULL, &y) == uim_ERR_ARGUMENT);
    CHECK(uim_input_scale_delta(&scale, 1.0f, 1.0f, &x, NULL) == uim_ERR_ARGUMENT);
    CHECK(uim_input_scale_point(&scale, NAN, 1.0f, &x, &y) == uim_ERR_ARGUMENT);
    {
        uim_InputScale zeroed;
        zeroed.dpi_scale = 0.0f;
        zeroed.sensitivity = 0.0f;
        CHECK(uim_input_scale_point(&zeroed, 10.0f, 10.0f, &x, &y) == uim_ERR_STATE);
        CHECK(uim_input_scale_delta(&zeroed, 10.0f, 10.0f, &x, &y) == uim_ERR_STATE);
    }

    /* Init with out-of-range values clamps and reports range, not argument. */
    CHECK(uim_input_scale_init(&scale, 0.1f, 30.0f) == uim_ERR_RANGE);
    CHECK_NEAR(scale.dpi_scale, uim_INPUT_SCALE_DPI_MIN, 1e-6f);
    CHECK_NEAR(scale.sensitivity, uim_INPUT_SCALE_SENSITIVITY_MAX, 1e-6f);

    if (failures == 0) printf("UIM input scale contract: ALL PASS\n");
    else printf("%d failures\n", failures);
    return failures;
}

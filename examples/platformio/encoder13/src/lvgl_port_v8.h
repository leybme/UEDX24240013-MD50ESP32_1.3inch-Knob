/*
 * LVGL port for TFT_eSPI
 * Converted from ESP32_Display_Panel to TFT_eSPI
 */
#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

// Display resolution
#define LVGL_PORT_DISP_WIDTH                    (240)
#define LVGL_PORT_DISP_HEIGHT                   (240)
#define LVGL_PORT_TICK_PERIOD_MS                (2)

// Buffer settings
#define LVGL_PORT_BUFFER_MALLOC_CAPS            (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define LVGL_PORT_BUFFER_SIZE                   (LVGL_PORT_DISP_WIDTH * 20)
#define LVGL_PORT_BUFFER_NUM                    (2)

// Task settings
#define LVGL_PORT_TASK_MAX_DELAY_MS             (500)
#define LVGL_PORT_TASK_MIN_DELAY_MS             (2)
#define LVGL_PORT_TASK_STACK_SIZE               (6 * 1024)
#define LVGL_PORT_TASK_PRIORITY                 (2)
#define LVGL_PORT_TASK_CORE                     (ARDUINO_RUNNING_CORE)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LVGL with TFT_eSPI display driver
 *
 * @param tft Pointer to TFT_eSPI instance
 * @return true if success, otherwise false
 */
bool lvgl_port_init(TFT_eSPI *tft);

/**
 * @brief Lock the LVGL mutex
 */
bool lvgl_port_lock(int timeout_ms);

/**
 * @brief Unlock the LVGL mutex
 */
bool lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif
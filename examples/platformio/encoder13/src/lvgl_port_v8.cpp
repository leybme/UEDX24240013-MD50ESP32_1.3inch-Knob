/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_timer.h"
#include "lvgl_port_v8.h"

static const char *TAG = "lvgl_port";
static SemaphoreHandle_t lvgl_mux = nullptr;                  // LVGL mutex
static TaskHandle_t lvgl_task_handle = nullptr;
static esp_timer_handle_t lvgl_tick_timer = NULL;
static void *lvgl_buf[2] = {};

#if !LV_TICK_CUSTOM
static void tick_increment(void *arg)
{
    /* Tell LVGL how many milliseconds have elapsed */
    lv_tick_inc(LVGL_PORT_TICK_PERIOD_MS);
}

static bool tick_init(void)
{
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &tick_increment,
        .name = "LVGL tick"
    };
    ESP_PANEL_CHECK_ERR_RET(
        esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer), false, "Create LVGL tick timer failed"
    );
    ESP_PANEL_CHECK_ERR_RET(
        esp_timer_start_periodic(lvgl_tick_timer, LVGL_PORT_TICK_PERIOD_MS * 1000), false,
        "Start LVGL tick timer failed"
    );

    return true;
}

static bool tick_deinit(void)
{
    ESP_PANEL_CHECK_ERR_RET(
        esp_timer_stop(lvgl_tick_timer), false, "Stop LVGL tick timer failed"
    );
    ESP_PANEL_CHECK_ERR_RET(
        esp_timer_delete(lvgl_tick_timer), false, "Delete LVGL tick timer failed"
    );
    return true;
}
#endif

/* Flush callback for LVGL 9 */
static void flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    ESP_PanelLcd *lcd = (ESP_PanelLcd *)lv_display_get_user_data(disp);
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    lcd->drawBitmap(offsetx1, offsety1, offsetx2 - offsetx1 + 1, offsety2 - offsety1 + 1, (const uint8_t *)px_map);
    // For RGB LCD, directly notify LVGL that the buffer is ready
    if (lcd->getBus()->getType() == ESP_PANEL_BUS_TYPE_RGB) {
        lv_display_flush_ready(disp);
    }
}

static lv_display_t *display_init(ESP_PanelLcd *lcd)
{
    ESP_PANEL_CHECK_FALSE_RET(lcd != nullptr, nullptr, "Invalid LCD device");
    ESP_PANEL_CHECK_FALSE_RET(lcd->getHandle() != nullptr, nullptr, "LCD device is not initialized");

    // Alloc draw buffers used by LVGL
    int buffer_size = LVGL_PORT_BUFFER_SIZE;

    ESP_LOGD(TAG, "Malloc memory for LVGL buffer");
    for (int i = 0; i < LVGL_PORT_BUFFER_NUM; i++) {
        lvgl_buf[i] = heap_caps_malloc(buffer_size * sizeof(lv_color_t), LVGL_PORT_BUFFER_MALLOC_CAPS);
        assert(lvgl_buf[i]);
        ESP_LOGD(TAG, "Buffer[%d] address: %p, size: %d", i, lvgl_buf[i], buffer_size * sizeof(lv_color_t));
    }

    ESP_LOGD(TAG, "Create LVGL display");
    lv_display_t *disp = lv_display_create(LVGL_PORT_DISP_WIDTH, LVGL_PORT_DISP_HEIGHT);
    ESP_PANEL_CHECK_NULL_RET(disp, nullptr, "Create LVGL display failed");

    lv_display_set_flush_cb(disp, flush_callback);
    lv_display_set_buffers(disp, lvgl_buf[0], lvgl_buf[1], buffer_size * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, (void *)lcd);
    lv_display_set_default(disp);

    // Note: Coordinate alignment rounding is handled internally by LVGL 9

    // For non-RGB LCD, need to notify LVGL that the buffer is ready when the refresh is finished
    auto bus_type = lcd->getBus()->getType();
    if (bus_type != ESP_PANEL_BUS_TYPE_RGB) {
        ESP_LOGD(TAG, "Attach refresh finish callback to LCD");
        lcd->attachDrawBitmapFinishCallback(
            [](void *user_data) -> bool {
                lv_display_flush_ready((lv_display_t *)user_data);
                return false;
            },
            (void *)disp
        );
    }

    return disp;
}

/* Touchpad read callback for LVGL 9 */
static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    ESP_PanelTouch *tp = (ESP_PanelTouch *)lv_indev_get_user_data(indev);
    ESP_PanelTouchPoint point;

    /* Read data from touch controller */
    int read_touch_result = tp->readPoints(&point, 1);
    if (read_touch_result > 0) {
        data->point.x = point.x;
        data->point.y = point.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static lv_indev_t *indev_init(ESP_PanelTouch *tp)
{
    ESP_PANEL_CHECK_FALSE_RET(tp != nullptr, nullptr, "Invalid touch device");
    ESP_PANEL_CHECK_FALSE_RET(tp->getHandle() != nullptr, nullptr, "Touch device is not initialized");

    ESP_LOGD(TAG, "Create LVGL input device");
    lv_indev_t *indev = lv_indev_create();
    ESP_PANEL_CHECK_NULL_RET(indev, nullptr, "Create LVGL input device failed");

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read);
    lv_indev_set_user_data(indev, (void *)tp);

    return indev;
}

IRAM_ATTR bool onDrawBitmapFinishCallback(void *user_data)
{
    lv_display_flush_ready((lv_display_t *)user_data);

    return false;
}

static void lvgl_port_task(void *arg)
{
    ESP_LOGD(TAG, "Starting LVGL task");

    uint32_t task_delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
    while (1) {
        if (lvgl_port_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            lvgl_port_unlock();
        }
        if (task_delay_ms > LVGL_PORT_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_PORT_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_PORT_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

bool lvgl_port_init(ESP_PanelLcd *lcd, ESP_PanelTouch *tp)
{
    ESP_PANEL_CHECK_FALSE_RET(lcd != nullptr, false, "Invalid LCD device");

    lv_display_t *disp = nullptr;
    lv_indev_t *indev = nullptr;

    lv_init();
#if !LV_TICK_CUSTOM
    ESP_PANEL_CHECK_FALSE_RET(tick_init(), false, "Initialize LVGL tick failed");
#endif

    ESP_LOGD(TAG, "Initialize LVGL display driver");
    disp = display_init(lcd);
    ESP_PANEL_CHECK_NULL_RET(disp, false, "Initialize LVGL display driver failed");

    // Set the default display rotation
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

    if (tp != nullptr) {
        ESP_LOGD(TAG, "Initialize LVGL input driver");
        indev = indev_init(tp);
        ESP_PANEL_CHECK_NULL_RET(indev, false, "Initialize LVGL input driver failed");
    }

    ESP_LOGD(TAG, "Create mutex for LVGL");
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    ESP_PANEL_CHECK_NULL_RET(lvgl_mux, false, "Create LVGL mutex failed");

    ESP_LOGD(TAG, "Create LVGL task");
    BaseType_t core_id = (LVGL_PORT_TASK_CORE < 0) ? tskNO_AFFINITY : LVGL_PORT_TASK_CORE;
    BaseType_t ret = xTaskCreatePinnedToCore(lvgl_port_task, "lvgl", LVGL_PORT_TASK_STACK_SIZE, NULL,
                     LVGL_PORT_TASK_PRIORITY, &lvgl_task_handle, core_id);
    ESP_PANEL_CHECK_FALSE_RET(ret == pdPASS, false, "Create LVGL task failed");

    return true;
}

bool lvgl_port_lock(int timeout_ms)
{
    ESP_PANEL_CHECK_NULL_RET(lvgl_mux, false, "LVGL mutex is not initialized");

    const TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return (xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE);
}

bool lvgl_port_unlock(void)
{
    ESP_PANEL_CHECK_NULL_RET(lvgl_mux, false, "LVGL mutex is not initialized");

    xSemaphoreGiveRecursive(lvgl_mux);

    return true;
}

bool lvgl_port_deinit(void)
{
#if !LV_TICK_CUSTOM
    ESP_PANEL_CHECK_FALSE_RET(tick_deinit(), false, "Deinitialize LVGL tick failed");
#endif

    ESP_PANEL_CHECK_FALSE_RET(lvgl_port_lock(-1), false, "Lock LVGL failed");
    if (lvgl_task_handle != nullptr) {
        vTaskDelete(lvgl_task_handle);
        lvgl_task_handle = nullptr;
    }
    ESP_PANEL_CHECK_FALSE_RET(lvgl_port_unlock(), false, "Unlock LVGL failed");

    lv_deinit();
    for (int i = 0; i < LVGL_PORT_BUFFER_NUM; i++) {
        if (lvgl_buf[i] != nullptr) {
            free(lvgl_buf[i]);
            lvgl_buf[i] = nullptr;
        }
    }
    if (lvgl_mux != nullptr) {
        vSemaphoreDelete(lvgl_mux);
        lvgl_mux = nullptr;
    }

    return true;
}
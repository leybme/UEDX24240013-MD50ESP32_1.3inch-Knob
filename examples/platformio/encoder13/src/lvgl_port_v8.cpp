/*
 * LVGL port for TFT_eSPI
 * Converted from ESP32_Display_Panel to TFT_eSPI
 */
#include "esp_timer.h"
#include "lvgl_port_v8.h"

static const char *TAG = "lvgl_port";
static SemaphoreHandle_t lvgl_mux = nullptr;
static TaskHandle_t lvgl_task_handle = nullptr;
static esp_timer_handle_t lvgl_tick_timer = NULL;
static void *lvgl_buf[2] = {};
static TFT_eSPI *_tft = nullptr;

#if !LV_TICK_CUSTOM
static void tick_increment(void *arg)
{
    lv_tick_inc(LVGL_PORT_TICK_PERIOD_MS);
}

static bool tick_init(void)
{
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &tick_increment,
        .name = "LVGL tick"
    };
    if (esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer) != ESP_OK) {
        Serial.println("Create LVGL tick timer failed");
        return false;
    }
    if (esp_timer_start_periodic(lvgl_tick_timer, LVGL_PORT_TICK_PERIOD_MS * 1000) != ESP_OK) {
        Serial.println("Start LVGL tick timer failed");
        return false;
    }
    return true;
}

static bool tick_deinit(void)
{
    if (esp_timer_stop(lvgl_tick_timer) != ESP_OK) {
        return false;
    }
    if (esp_timer_delete(lvgl_tick_timer) != ESP_OK) {
        return false;
    }
    return true;
}
#endif

/* Flush callback for LVGL - uses TFT_eSPI pushImage */
static void flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    _tft->startWrite();
    _tft->setAddrWindow(area->x1, area->y1, w, h);
    _tft->pushColors((uint16_t *)px_map, w * h);
    _tft->endWrite();

    lv_display_flush_ready(disp);
}

static lv_display_t *display_init(TFT_eSPI *tft)
{
    // Allocate draw buffers
    int buffer_size = LVGL_PORT_BUFFER_SIZE;

    for (int i = 0; i < LVGL_PORT_BUFFER_NUM; i++) {
        lvgl_buf[i] = heap_caps_malloc(buffer_size * sizeof(lv_color_t), LVGL_PORT_BUFFER_MALLOC_CAPS);
        assert(lvgl_buf[i]);
    }

    lv_display_t *disp = lv_display_create(LVGL_PORT_DISP_WIDTH, LVGL_PORT_DISP_HEIGHT);
    if (disp == nullptr) {
        Serial.println("Create LVGL display failed");
        return nullptr;
    }

    lv_display_set_flush_cb(disp, flush_callback);
    lv_display_set_buffers(disp, lvgl_buf[0], lvgl_buf[1], buffer_size * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_default(disp);

    return disp;
}

static void lvgl_port_task(void *arg)
{
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

bool lvgl_port_init(TFT_eSPI *tft)
{
    if (tft == nullptr) {
        Serial.println("Invalid TFT device");
        return false;
    }

    _tft = tft;

    lv_display_t *disp = nullptr;

    lv_init();
#if !LV_TICK_CUSTOM
    if (!tick_init()) {
        Serial.println("Initialize LVGL tick failed");
        return false;
    }
#endif

    // Initialize TFT_eSPI
    tft->begin();
    // Vendor init commands for GC9A01 (from UEDX24240013-MD50E board config)
    tft->writecommand(0xfe);
    tft->writecommand(0xef);
    tft->writedata(0x14);
    tft->writecommand(0x84); tft->writedata(0x60);
    tft->writecommand(0x85); tft->writedata(0xFF);
    tft->writecommand(0x86); tft->writedata(0xFF);
    tft->writecommand(0x87); tft->writedata(0xFF);
    tft->writecommand(0x8e); tft->writedata(0xFF);
    tft->writecommand(0x8f); tft->writedata(0xFF);
    tft->writecommand(0x88); tft->writedata(0x0A);
    tft->writecommand(0x89); tft->writedata(0x21);
    tft->writecommand(0x8a); tft->writedata(0x00);
    tft->writecommand(0x8b); tft->writedata(0x80);
    tft->writecommand(0x8c); tft->writedata(0x01);
    tft->writecommand(0x8d); tft->writedata(0x03);
    tft->writecommand(0xb5); tft->writedata(0x08); tft->writedata(0x09); tft->writedata(0x14); tft->writedata(0x08);
    tft->writecommand(0xb6); tft->writedata(0x00); tft->writedata(0x00);
    tft->writecommand(0x36); tft->writedata(0x48);
    tft->writecommand(0x3a); tft->writedata(0x05);
    tft->writecommand(0x90); tft->writedata(0x08); tft->writedata(0x08); tft->writedata(0x08); tft->writedata(0x08);
    tft->writecommand(0xbd); tft->writedata(0x06);
    tft->writecommand(0xba); tft->writedata(0x01);
    tft->writecommand(0xbc); tft->writedata(0x00);
    tft->writecommand(0xff); tft->writedata(0x60); tft->writedata(0x01); tft->writedata(0x04);
    tft->writecommand(0xc3); tft->writedata(0x13);
    tft->writecommand(0xc4); tft->writedata(0x13);
    tft->writecommand(0xc9); tft->writedata(0x25);
    tft->writecommand(0xbe); tft->writedata(0x11);
    tft->writecommand(0xe1); tft->writedata(0x10); tft->writedata(0x0e);
    tft->writecommand(0xdf); tft->writedata(0x21); tft->writedata(0x0c); tft->writedata(0x02);
    tft->writecommand(0xf0); tft->writedata(0x45); tft->writedata(0x09); tft->writedata(0x08); tft->writedata(0x08); tft->writedata(0x26); tft->writedata(0x2a);
    tft->writecommand(0xf1); tft->writedata(0x43); tft->writedata(0x70); tft->writedata(0x72); tft->writedata(0x36); tft->writedata(0x37); tft->writedata(0x6f);
    tft->writecommand(0xf2); tft->writedata(0x45); tft->writedata(0x09); tft->writedata(0x08); tft->writedata(0x08); tft->writedata(0x26); tft->writedata(0x2a);
    tft->writecommand(0xf3); tft->writedata(0x43); tft->writedata(0x70); tft->writedata(0x72); tft->writedata(0x36); tft->writedata(0x37); tft->writedata(0x6f);
    tft->writecommand(0xed); tft->writedata(0x1b); tft->writedata(0x0b);
    tft->writecommand(0xae); tft->writedata(0x77);
    tft->writecommand(0xcd); tft->writedata(0x63);
    tft->writecommand(0x70); tft->writedata(0x07); tft->writedata(0x07); tft->writedata(0x04); tft->writedata(0x0e); tft->writedata(0x0f); tft->writedata(0x09); tft->writedata(0x07); tft->writedata(0x08); tft->writedata(0x03);
    tft->writecommand(0xe8); tft->writedata(0x34);
    tft->writecommand(0x62); tft->writedata(0x18); tft->writedata(0x0d); tft->writedata(0x71); tft->writedata(0xed); tft->writedata(0x70); tft->writedata(0x70); tft->writedata(0x18); tft->writedata(0x0f); tft->writedata(0x71); tft->writedata(0xef); tft->writedata(0x70); tft->writedata(0x70);
    tft->writecommand(0x63); tft->writedata(0x18); tft->writedata(0x11); tft->writedata(0x71); tft->writedata(0xf1); tft->writedata(0x70); tft->writedata(0x70); tft->writedata(0x18); tft->writedata(0x13); tft->writedata(0x71); tft->writedata(0xf3); tft->writedata(0x70); tft->writedata(0x70);
    tft->writecommand(0x64); tft->writedata(0x28); tft->writedata(0x29); tft->writedata(0xf1); tft->writedata(0x01); tft->writedata(0xf1); tft->writedata(0x00); tft->writedata(0x07);
    tft->writecommand(0x66); tft->writedata(0x3c); tft->writedata(0x00); tft->writedata(0xcd); tft->writedata(0x67); tft->writedata(0x45); tft->writedata(0x45); tft->writedata(0x10); tft->writedata(0x00); tft->writedata(0x00); tft->writedata(0x00);
    tft->writecommand(0x67); tft->writedata(0x00); tft->writedata(0x3c); tft->writedata(0x00); tft->writedata(0x00); tft->writedata(0x00); tft->writedata(0x01); tft->writedata(0x54); tft->writedata(0x10); tft->writedata(0x32); tft->writedata(0x98);
    tft->writecommand(0x74); tft->writedata(0x10); tft->writedata(0x85); tft->writedata(0x80); tft->writedata(0x00); tft->writedata(0x00); tft->writedata(0x4e); tft->writedata(0x00);
    tft->writecommand(0x98); tft->writedata(0x3e); tft->writedata(0x07);
    tft->writecommand(0x99); tft->writedata(0x3e); tft->writedata(0x07);
    tft->writecommand(0x35); tft->writedata(0x00);
    tft->writecommand(0x44); tft->writedata(0x00); tft->writedata(0x4a);
    tft->writecommand(0x21);
    // Set address window
    tft->writecommand(0x2a); tft->writedata(0x00); tft->writedata(0x00); tft->writedata(0x00); tft->writedata(0xef);
    tft->writecommand(0x2b); tft->writedata(0x00); tft->writedata(0x00); tft->writedata(0x00); tft->writedata(0xef);
    tft->writecommand(0x2c);
    // Sleep out + display on
    tft->writecommand(0x11);
    delay(120);
    tft->writecommand(0x29);
    delay(20);

    // Set rotation - original had MIRROR_X=1, MIRROR_Y=0, SWAP_XY=0
    // The vendor MADCTL 0x36 = 0x48 already sets MX and BGR
    tft->setRotation(0);

    Serial.println("Initialize LVGL display driver");
    disp = display_init(tft);
    if (disp == nullptr) {
        Serial.println("Initialize LVGL display driver failed");
        return false;
    }

    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    if (lvgl_mux == nullptr) {
        Serial.println("Create LVGL mutex failed");
        return false;
    }

    BaseType_t core_id = (LVGL_PORT_TASK_CORE < 0) ? tskNO_AFFINITY : LVGL_PORT_TASK_CORE;
    BaseType_t ret = xTaskCreatePinnedToCore(lvgl_port_task, "lvgl", LVGL_PORT_TASK_STACK_SIZE, NULL,
                     LVGL_PORT_TASK_PRIORITY, &lvgl_task_handle, core_id);
    if (ret != pdPASS) {
        Serial.println("Create LVGL task failed");
        return false;
    }

    return true;
}

bool lvgl_port_lock(int timeout_ms)
{
    if (lvgl_mux == nullptr) return false;
    const TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return (xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE);
}

bool lvgl_port_unlock(void)
{
    if (lvgl_mux == nullptr) return false;
    xSemaphoreGiveRecursive(lvgl_mux);
    return true;
}

bool lvgl_port_deinit(void)
{
#if !LV_TICK_CUSTOM
    if (!tick_deinit()) return false;
#endif

    if (!lvgl_port_lock(-1)) return false;
    if (lvgl_task_handle != nullptr) {
        vTaskDelete(lvgl_task_handle);
        lvgl_task_handle = nullptr;
    }
    if (!lvgl_port_unlock()) return false;

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
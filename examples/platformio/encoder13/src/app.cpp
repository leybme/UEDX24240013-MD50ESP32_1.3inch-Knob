/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Converted to use TFT_eSPI instead of ESP32_Display_Panel
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include "lvgl_port_v8.h"

#include <ESP_Knob.h>
#include <Button.h>
#include <ui.h>

// Pin definitions from BOARD_UEDX24240013_MD50E
#define GPIO_NUM_KNOB_PIN_A     7
#define GPIO_NUM_KNOB_PIN_B     6
#define GPIO_BUTTON_PIN         GPIO_NUM_9

TFT_eSPI tft = TFT_eSPI();
ESP_Knob *knob;

void onKnobLeftEventCallback(int count, void *usr_data)
{
    Serial.printf("Detect left event, count is %d\n", count);
    lvgl_port_lock(-1);
    LVGL_knob_event((void*)(intptr_t)KNOB_LEFT);
    lvgl_port_unlock();
}

void onKnobRightEventCallback(int count, void *usr_data)
{
    Serial.printf("Detect right event, count is %d\n", count);
    lvgl_port_lock(-1);
    LVGL_knob_event((void*)(intptr_t)KNOB_RIGHT);
    lvgl_port_unlock();
}

static void SingleClickCb(void *button_handle, void *usr_data) {
    Serial.println("Button Single Click");
    lvgl_port_lock(-1);
    LVGL_button_event((void*)(intptr_t)BUTTON_SINGLE_CLICK);
    lvgl_port_unlock();
}
static void DoubleClickCb(void *button_handle, void *usr_data)
{
    Serial.println("Button Double Click");
}
static void LongPressStartCb(void *button_handle, void *usr_data) {
    Serial.println("Button Long Press Start");
    lvgl_port_lock(-1);
    LVGL_button_event((void*)(intptr_t)BUTTON_LONG_PRESS_START);
    lvgl_port_unlock();
}

// Backlight control
#define BACKLIGHT_PIN 8

void setup()
{
    String title = "LVGL porting example (TFT_eSPI)";
    Serial.begin(115200);
    Serial.println(title + " start");

    // Backlight starts OFF (matches original ESP_PANEL_BACKLIGHT_IDLE_OFF = 1)
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, LOW);
    Serial.println("Backlight pin set LOW (off during init)");

    Serial.println("Initialize Knob device");
    knob = new ESP_Knob(GPIO_NUM_KNOB_PIN_A, GPIO_NUM_KNOB_PIN_B);
    knob->begin();
    knob->attachLeftEventCallback(onKnobLeftEventCallback);
    knob->attachRightEventCallback(onKnobRightEventCallback);

    Serial.println("Initialize Button device");
    Button *btn = new Button(GPIO_BUTTON_PIN, false);

    btn->attachSingleClickEventCb(&SingleClickCb, NULL);
    btn->attachDoubleClickEventCb(&DoubleClickCb, NULL);
    btn->attachLongPressStartEventCb(&LongPressStartCb, NULL);

    Serial.println("Initialize LVGL with TFT_eSPI");
    lvgl_port_init(&tft);

    // Turn on backlight after display initialization
    Serial.println("Turning backlight ON");
    digitalWrite(BACKLIGHT_PIN, HIGH);
    delay(10);

    Serial.println("Create UI");
    /* Lock the mutex due to the LVGL APIs are not thread-safe */
    lvgl_port_lock(-1);

    ui_init();

    /* Release the mutex */
    lvgl_port_unlock();

    Serial.println(title + " end");
}

void loop()
{
    // Test backlight blinking
    static unsigned long lastBlink = 0;
    static bool backlightState = false;
    if (millis() - lastBlink > 1000) {
        lastBlink = millis();
        backlightState = !backlightState;
        digitalWrite(BACKLIGHT_PIN, backlightState ? HIGH : LOW);
        Serial.printf("Backlight: %s\n", backlightState ? "ON" : "OFF");
    }

    lv_tick_inc(10);
    delay(10);
}

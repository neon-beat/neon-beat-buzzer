#pragma once

struct color_hsv {
    int32_t hue;
    uint32_t saturation;
    uint32_t value;
};

enum led_pattern {
    LED_PATTERN_OFF,
    LED_PATTERN_BLINK,
    LED_PATTERN_WAVE
};

struct led_cmd {
    enum led_pattern pattern;
    struct color_hsv color;
    uint32_t duration_ms;
    uint32_t period_ms;
    uint8_t duty_cycle;
};

void ubt_led_start();
void ubt_led_run_pattern(struct led_cmd *cmd);


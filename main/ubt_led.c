#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <led_strip.h>
#include <string.h>
#include "esp_log.h"
#include "ubt_led.h"
#define BLINK_GPIO CONFIG_UBT_LED_GPIO
#define TAG "LED"


#define LED_REFRESH_PERIOD_MS	20

led_strip_config_t strip_config = {
    .strip_gpio_num = BLINK_GPIO,  // The GPIO that connected to the LED strip's data line
    .max_leds = 1,                 // The number of LEDs in the strip,
    .led_model = LED_MODEL_WS2812, // LED strip model, it determines the bit timing
    .led_pixel_format = LED_PIXEL_FORMAT_GRB, // The color component format is G-R-B
    .flags = {
        .invert_out = false, // don't invert the output signal
    }
};

led_strip_rmt_config_t rmt_config = { .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency: 10MHz
    .flags = {
        .with_dma = false, // DMA feature is available on chips like ESP32-S3/P4
    }
};

led_strip_handle_t led_strip = NULL;

uint8_t wave[] = {
    0,   16,  31,  47,  63,  78,  93,  108, 122, 136, 149, 162, 174,
    185, 196, 206, 215, 223, 230, 237, 242, 246, 250, 252, 254, 255,
    254, 252, 250, 246, 242, 237, 230, 223, 215, 206, 196, 185, 174,
    162, 149, 136, 122, 108, 93,  78,  63,  47,  31,  16,  0,
};
static const int wave_len = sizeof(wave);

static TaskHandle_t xLedTask = NULL;
static QueueHandle_t cmd_queue = NULL;


static void led_task(void *pvParameters)
{
    uint32_t current_effect_duration_played = 0;
    int wait_delay_ticks = portMAX_DELAY;
    struct led_cmd cmd, current_cmd;
    bool blink_state = false;
    int ret, i = 0;

    while (true) {
        ret = xQueueReceive(cmd_queue, &cmd, wait_delay_ticks);
        if (ret == pdPASS) {
            switch (cmd.pattern) {
                case LED_PATTERN_OFF :
                    wait_delay_ticks = portMAX_DELAY;
                    led_strip_clear(led_strip);
                    continue;
                    break;
                case LED_PATTERN_BLINK:
                    wait_delay_ticks = pdMS_TO_TICKS(cmd.period_ms/2);
                    blink_state = false;
                    break;
                case LED_PATTERN_WAVE:
                    wait_delay_ticks = pdMS_TO_TICKS(LED_REFRESH_PERIOD_MS);
                    break;
                default:
                    ESP_LOGW(TAG, "Unknown pattern %d", cmd.pattern);
                    continue;
                    break;
            }
            ESP_LOGI(TAG,
                    "New led cmd: pattern=%d, color=%ld:%lu:%lu, d=%lu, "
                    "p=%lu, dc=%u%%",
                    cmd.pattern, cmd.color.hue, cmd.color.saturation,
                    cmd.color.value, cmd.duration_ms, cmd.period_ms,
                    cmd.duty_cycle);
            memcpy(&current_cmd, &cmd, sizeof(current_cmd));
        }

        /* The task has awoken not because of a new command but because of
         * "wait timeout", so generate the next color command depending on
         * the current pattern
         */
        switch (current_cmd.pattern) {
            case LED_PATTERN_BLINK:
                if (!blink_state)
                    led_strip_set_pixel_hsv(led_strip, 0, current_cmd.color.hue,
                            current_cmd.color.saturation,
                            current_cmd.color.value);
                else
                    led_strip_set_pixel_hsv(led_strip, 0, 0, 0, 0);
                blink_state = !blink_state;
                led_strip_refresh(led_strip);
                break;
            case LED_PATTERN_WAVE:
		wait_delay_ticks = pdMS_TO_TICKS(LED_REFRESH_PERIOD_MS);
                led_strip_set_pixel_hsv(led_strip, 0, current_cmd.color.hue,
                            current_cmd.color.saturation,
                            wave[i++]*current_cmd.color.value/255);
                led_strip_refresh(led_strip);
		if (i > wave_len) {
			i = 0;
			wait_delay_ticks = pdMS_TO_TICKS(LED_REFRESH_PERIOD_MS*50 * 100 / current_cmd.duty_cycle);
		}
                break;
            default:
                ESP_LOGE(TAG, "Reached unknown pattern %d", current_cmd.pattern);
                continue;
        }

        if (current_cmd.duration_ms) {
            current_effect_duration_played += pdTICKS_TO_MS(wait_delay_ticks);
            if (current_effect_duration_played >= current_cmd.duration_ms) {
                current_cmd.pattern = LED_PATTERN_OFF;
                wait_delay_ticks = portMAX_DELAY;
                current_effect_duration_played = 0;
		i = 0;
                led_strip_clear(led_strip);
            }
        }
    }
}

void ubt_led_start() {
    ESP_ERROR_CHECK(
            led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
    cmd_queue = xQueueCreate(2, sizeof(struct led_cmd));
    xTaskCreate(&led_task, "led_task", 4096, NULL, 5, &xLedTask);
    if (!cmd_queue)
        ESP_LOGE(TAG, "Failed to create queue !");
    ESP_LOGI(TAG, "LED ready");
}

void ubt_led_run_pattern(struct led_cmd *cmd) {
    xQueueSend(cmd_queue, cmd, 0);
}

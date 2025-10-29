#include <string.h>
#include <cJSON.h>
#include <esp_log.h>
#include "ws_commands.h"
#include "ubt_led.h"

#define TAG "CMD"

static int parse_blink_pattern(struct cJSON *details, struct led_cmd *cmd)
{
	cJSON *duration_ms, *period_ms, *dc, *color, *h, *s, *v;

	duration_ms = cJSON_GetObjectItemCaseSensitive(details, "duration_ms");
	period_ms = cJSON_GetObjectItemCaseSensitive(details, "period_ms");
	dc = cJSON_GetObjectItemCaseSensitive(details, "dc");
	color = cJSON_GetObjectItemCaseSensitive(details, "color");
	if (!duration_ms || !cJSON_IsNumber(duration_ms) ||
			!period_ms || !cJSON_IsNumber(period_ms) ||
			!dc || !cJSON_IsNumber(dc) ||
			!color || !cJSON_IsObject(color)) {
		ESP_LOGW(TAG, "Can not parse high level details for blink pattern");
		return -1;
	}
	h = cJSON_GetObjectItemCaseSensitive(color, "h");
	s = cJSON_GetObjectItemCaseSensitive(color, "s");
	v = cJSON_GetObjectItemCaseSensitive(color, "v");
	if (!h || !cJSON_IsNumber(h) ||
			!s || !cJSON_IsNumber(s) ||
			!v || !cJSON_IsNumber(v)) {
		ESP_LOGW(TAG, "Can not parse color for blink pattern");
		return -1;
	}
	cmd->duration_ms = duration_ms->valueint;
	cmd->period_ms = period_ms->valueint;
	cmd->duty_cycle = (int)(dc->valuedouble * 100);
	cmd->color.hue = (int)(h->valuedouble);
	cmd->color.saturation = (int)(s->valuedouble * 255);
	cmd->color.value = (int)(v->valuedouble * 255);
	return 0;
}

static int parse_wave_pattern(struct cJSON *details, struct led_cmd *cmd)
{
	cJSON *duration_ms, *period_ms, *dc, *color, *h, *s, *v;

	duration_ms = cJSON_GetObjectItemCaseSensitive(details, "duration_ms");
	period_ms = cJSON_GetObjectItemCaseSensitive(details, "period_ms");
	dc = cJSON_GetObjectItemCaseSensitive(details, "dc");
	color = cJSON_GetObjectItemCaseSensitive(details, "color");
	if (!duration_ms || !cJSON_IsNumber(duration_ms) ||
			!period_ms || !cJSON_IsNumber(period_ms) ||
			!dc || !cJSON_IsNumber(dc) ||
			!color || !cJSON_IsObject(color)) {
		ESP_LOGW(TAG, "Can not parse high level details for blink pattern");
		return -1;
	}
	h = cJSON_GetObjectItemCaseSensitive(color, "h");
	s = cJSON_GetObjectItemCaseSensitive(color, "s");
	v = cJSON_GetObjectItemCaseSensitive(color, "v");
	if (!h || !cJSON_IsNumber(h) ||
			!s || !cJSON_IsNumber(s) ||
			!v || !cJSON_IsNumber(v)) {
		ESP_LOGW(TAG, "Can not parse color for blink pattern");
		return -1;
	}
	cmd->duration_ms = duration_ms->valueint;
	cmd->period_ms = period_ms->valueint;
	cmd->duty_cycle = (int)(dc->valuedouble * 100);
	cmd->color.hue = (int)(h->valuedouble);
	cmd->color.saturation = (int)(s->valuedouble * 255);
	cmd->color.value = (int)(v->valuedouble * 255);
	return 0;
}

int ws_commands_process(const char *data, int data_len)
{
	cJSON *root, *pattern, *type, *details;
	struct led_cmd cmd;
	int ret = -1;

	root = cJSON_Parse(data);
	if (!root) {
		ESP_LOGW(TAG, "Can not parse json");
		goto end;
	}

	pattern = cJSON_GetObjectItemCaseSensitive(root, "pattern");
	if (!pattern) {
		ESP_LOGW(TAG, "Command is not a pattern");
		goto end;
	}

	type = cJSON_GetObjectItemCaseSensitive(pattern, "type");
	if (!type || !cJSON_IsString(type)) {
		ESP_LOGW(TAG, "Can not get pattern type");
		goto end;
	}

	if (strcmp(type->valuestring, "blink") == 0)
		cmd.pattern = LED_PATTERN_BLINK;
	else if (strcmp(type->valuestring, "wave") == 0)
		cmd.pattern = LED_PATTERN_WAVE;
	else if (strcmp(type->valuestring, "off") == 0) {
		cmd.pattern = LED_PATTERN_OFF;
		ubt_led_run_pattern(&cmd);
		goto end;
	} else {
		ESP_LOGW(TAG, "Unknown led pattern %s", type->valuestring);
		goto end;
	}


	details = cJSON_GetObjectItemCaseSensitive(pattern, "details");
	if (!details || !cJSON_IsObject(details)) {
		ESP_LOGW(TAG, "Can not get pattern details");
		goto end;
	}

	switch (cmd.pattern) {
		case LED_PATTERN_BLINK:
			ret = parse_blink_pattern(details, &cmd);
			break;
		case LED_PATTERN_WAVE:
			ret = parse_wave_pattern(details, &cmd);
			break;
		default:
			ESP_LOGW(TAG, "Details parsing not supported for pattern %s", type->valuestring);
			goto end;
			break;
	}
	
	if (!ret)
		ubt_led_run_pattern(&cmd);
end:
	cJSON_Delete(root);
	return ret;
}

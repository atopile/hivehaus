#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_sntp.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"

static const char *TAG = "time_sync";

// ADC configuration for potentiometer
// Based on motor-fader design: pot_sense ~ esp32.adc[2] ~ io[2] (GPIO2)
// ESP32-S3: GPIO2 = ADC1_CHANNEL_1
#define POT_ADC_CHANNEL     ADC1_CHANNEL_1  // GPIO2
#define POT_ADC_WIDTH       ADC_WIDTH_BIT_12
#define POT_ADC_ATTEN       ADC_ATTEN_DB_11  // 0-3.3V range
#define POT_NUM_SAMPLES     64              // Number of samples for averaging

// ADC configuration for current sense
// Based on motor-fader design: current_sense ~ esp32.adc[3] ~ io[3] (GPIO3)
// ESP32-S3: GPIO3 = ADC1_CHANNEL_2
#define CURRENT_ADC_CHANNEL ADC1_CHANNEL_2  // GPIO3

// Current sense conversion (adjust based on your sense resistor and gain)
// DRV8210 typical: V = I * R_sense * Gain
// Assuming typical values: R_sense = 0.1Ω, Gain = 10, so 1V = 1A
#define CURRENT_SENSE_GAIN  1.0f  // A/V - adjust based on actual hardware

static esp_adc_cal_characteristics_t *adc_chars;

// Motor control GPIO pins (from motor-fader design)
#define MOTOR_IN1_GPIO    4   // GPIO4 - Motor direction 1
#define MOTOR_IN2_GPIO    5   // GPIO5 - Motor direction 2
#define MOTOR_SLEEP_GPIO  6   // GPIO6 - Motor driver enable/sleep

// If your driver isn't waking up, try flipping this to 0.
#define MOTOR_SLEEP_ENABLE_LEVEL 1

// Set to -1.0f if motion is reversed vs potentiometer position.
#define MOTOR_DIRECTION_SIGN -1.0f

// Motor PWM configuration
#define MOTOR_PWM_FREQ_HZ       20000
#define MOTOR_PWM_TIMER         LEDC_TIMER_0
#define MOTOR_PWM_MODE          LEDC_LOW_SPEED_MODE
#define MOTOR_PWM_DUTY_RES      LEDC_TIMER_10_BIT
#define MOTOR_PWM_CH_IN1        LEDC_CHANNEL_0
#define MOTOR_PWM_CH_IN2        LEDC_CHANNEL_1

// Quick sanity check: forces a constant effort so you can confirm PWM + wiring.
// Set to a value in [-1..1] (e.g. 0.6f), then flash and verify motion/current.
#define MOTOR_FORCE_EFFORT NAN

// Control loop timing: keep motor/physics updates fast; print status slowly.
#define CONTROL_HZ 1000
#define STATUS_HZ 5
#define CONTROL_POT_SAMPLES 8
#define CONTROL_CURRENT_SAMPLES 8

// WiFi credentials - update these with your network
#define WIFI_SSID      "atopile"
#define WIFI_PASS      "code2pcb"

// Event group for WiFi connection
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
#define MAX_RETRY 5

// Store local IP address
static esp_netif_ip_info_t s_ip_info;
static bool s_ip_obtained = false;

typedef struct {
    float position;
    float filtered_position;
    float velocity;
    float filtered_velocity;
    float spring_center;
    float spring_error;
    float motor_effort;
    float current_amps;
    float pot_percent;
    uint32_t pot_mv;
    uint32_t current_mv;
    float control_hz_effective;
} control_status_t;

static control_status_t s_status;
static portMUX_TYPE s_status_mux = portMUX_INITIALIZER_UNLOCKED;

static inline float clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static void motor_set_effort(float effort)
{
    const uint32_t max_duty = (1U << MOTOR_PWM_DUTY_RES) - 1U;
    effort = clampf(effort, -1.0f, 1.0f);

    uint32_t duty = (uint32_t)lrintf(fabsf(effort) * (float)max_duty);
    if (duty > max_duty) duty = max_duty;

    if (effort > 0.0f) {
        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_IN1, duty);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_IN1);
        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_IN2, 0);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_IN2);
    } else if (effort < 0.0f) {
        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_IN1, 0);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_IN1);
        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_IN2, duty);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_IN2);
    } else {
        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_IN1, 0);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_IN1);
        ledc_set_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_IN2, 0);
        ledc_update_duty(MOTOR_PWM_MODE, MOTOR_PWM_CH_IN2);
    }
}

static void control_task(void *arg)
{
    (void)arg;

    float position = 0.0f;                // Current position (0-100%)
    float filtered_position = 0.0f;       // Low-pass filtered position
    float velocity = 0.0f;                // Measured velocity (%/s)
    float filtered_velocity = 0.0f;       // Low-pass filtered velocity
    float last_filtered_position = 0.0f;

    // Filter time constants (seconds); converted to alpha using dt.
    const float position_filter_tau_s = 0.04f;
    const float velocity_filter_tau_s = 0.05f;
    const float dead_zone = 0.06f;            // (%)

    // Spring controller: always pulls slider toward center with damping.
    const float spring_center = 50.0f;       // (%)
    const float spring_kp = 0.045f;          // duty / (% error)
    const float spring_kd = 0.000f;          // duty / (%/s velocity)
    const float spring_deadband = 0.20f;     // (%)
    const float spring_max_duty_base = 0.80f; // (0..1)
    const float spring_max_duty_endstop = 1.00f; // (0..1) allow full force near ends
    const float endstop_zone_pct = 2.0f;     // (% position) considered "near end stop"
    const float spring_min_duty = 0.22f;     // (0..1) overcome stiction / be useful
    const float stiction_error_threshold = 1.20f; // (%) don't apply min duty below this
    const float min_useful_current_amps = 0.05f;  // below this, small efforts do nothing

    // Safety limits
    const float current_limit_amps = 1.6f;
    const float current_hard_cut_amps = 2.2f;

    // FreeRTOS delay resolution is limited by configTICK_RATE_HZ; ensure we never pass 0 ticks.
    TickType_t control_period_ticks = (TickType_t)((configTICK_RATE_HZ + CONTROL_HZ - 1) / CONTROL_HZ);
    if (control_period_ticks < 1) control_period_ticks = 1;
    TickType_t last_wake = xTaskGetTickCount();
    const float control_hz_effective = (float)configTICK_RATE_HZ / (float)control_period_ticks;
    const float dt = 1.0f / control_hz_effective;
    const float position_filter_alpha = clampf(dt / (position_filter_tau_s + dt), 0.0f, 1.0f);
    const float velocity_filter_alpha = clampf(dt / (velocity_filter_tau_s + dt), 0.0f, 1.0f);

    for (;;) {
        vTaskDelayUntil(&last_wake, control_period_ticks);

        // Potentiometer
        uint32_t pot_adc = 0;
        for (int i = 0; i < CONTROL_POT_SAMPLES; i++) {
            pot_adc += adc1_get_raw(POT_ADC_CHANNEL);
        }
        pot_adc /= CONTROL_POT_SAMPLES;
        uint32_t pot_mv = esp_adc_cal_raw_to_voltage(pot_adc, adc_chars);
        float pot_percent = clampf(((float)pot_mv / 3300.0f) * 100.0f, 0.0f, 100.0f);

        position = pot_percent;
        filtered_position = position_filter_alpha * position + (1.0f - position_filter_alpha) * filtered_position;

        float position_change = filtered_position - last_filtered_position;
        if (fabsf(position_change) < dead_zone) position_change = 0.0f;
        velocity = position_change / dt;
        last_filtered_position = filtered_position;
        filtered_velocity = velocity_filter_alpha * velocity + (1.0f - velocity_filter_alpha) * filtered_velocity;
        // Current sense
        uint32_t current_adc = 0;
        for (int i = 0; i < CONTROL_CURRENT_SAMPLES; i++) {
            current_adc += adc1_get_raw(CURRENT_ADC_CHANNEL);
        }
        current_adc /= CONTROL_CURRENT_SAMPLES;
        uint32_t current_mv = esp_adc_cal_raw_to_voltage(current_adc, adc_chars);
        float current_amps = ((float)current_mv / 1000.0f) * CURRENT_SENSE_GAIN;

        // --- Spring control ---
        float spring_error = spring_center - filtered_position;
        if (fabsf(spring_error) < spring_deadband) spring_error = 0.0f;

        const bool near_endstop = (filtered_position <= endstop_zone_pct) || (filtered_position >= (100.0f - endstop_zone_pct));
        const float spring_max_duty = near_endstop ? spring_max_duty_endstop : spring_max_duty_base;

        float motor_effort = (spring_kp * spring_error) - (spring_kd * filtered_velocity);
        motor_effort = clampf(motor_effort, -spring_max_duty, spring_max_duty);
        const bool needs_breakaway = near_endstop || (fabsf(spring_error) > stiction_error_threshold);

        // Avoid tiny commands that won't move (and often just buzz).
        if (fabsf(motor_effort) > 0.0f && fabsf(motor_effort) < spring_min_duty) {
            motor_effort = needs_breakaway ? copysignf(spring_min_duty, motor_effort) : 0.0f;
        }

        // Current limiting
        if (current_amps >= current_hard_cut_amps) {
            motor_effort = 0.0f;
        } else if (current_amps > current_limit_amps) {
            motor_effort *= (current_limit_amps / current_amps);
        }

        // If the sensed current is still below the "useful" threshold, don't bother with small efforts.
        if (current_amps < min_useful_current_amps && fabsf(motor_effort) < spring_min_duty) {
            motor_effort = 0.0f;
        }

        // End stops
        if ((filtered_position <= 0.3f && motor_effort < 0.0f) ||
            (filtered_position >= 99.7f && motor_effort > 0.0f)) {
            motor_effort = 0.0f;
        }

        float commanded_effort = motor_effort;
        if (!isnan(MOTOR_FORCE_EFFORT)) commanded_effort = MOTOR_FORCE_EFFORT;
        motor_set_effort(MOTOR_DIRECTION_SIGN * commanded_effort);

        portENTER_CRITICAL(&s_status_mux);
        s_status.position = position;
        s_status.filtered_position = filtered_position;
        s_status.velocity = velocity;
        s_status.filtered_velocity = filtered_velocity;
        s_status.spring_center = spring_center;
        s_status.spring_error = spring_error;
        s_status.motor_effort = motor_effort;
        s_status.current_amps = current_amps;
        s_status.pot_percent = pot_percent;
        s_status.pot_mv = pot_mv;
        s_status.current_mv = current_mv;
        s_status.control_hz_effective = control_hz_effective;
        portEXIT_CRITICAL(&s_status_mux);
    }
}

static void status_task(void *arg)
{
    (void)arg;

    const TickType_t status_period_ticks = pdMS_TO_TICKS((1000 + STATUS_HZ - 1) / STATUS_HZ);
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last_wake, status_period_ticks);

        control_status_t snapshot;
        portENTER_CRITICAL(&s_status_mux);
        snapshot = s_status;
        portEXIT_CRITICAL(&s_status_mux);

        time_t now = 0;
        struct tm timeinfo = {0};
        char strftime_buf[64];
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

        printf("\033[2J\033[H");
        printf("=== Time Status ===\n");
        printf("Current time: %s\n", strftime_buf);
        printf("Control Hz: %.1f\n", snapshot.control_hz_effective);
        if (s_ip_obtained) {
            printf("Local IP: " IPSTR "\n", IP2STR(&s_ip_info.ip));
        } else {
            printf("Local IP: Not connected\n");
        }
        printf("Pot: %.1f%% (%.3fV)\n", snapshot.pot_percent, (float)snapshot.pot_mv / 1000.0f);
        printf("Motor Current: %.3fA (%.3fV)\n", snapshot.current_amps, (float)snapshot.current_mv / 1000.0f);
        printf("Measured vel: %.1f%%/s (filtered: %.1f%%/s)\n", snapshot.velocity, snapshot.filtered_velocity);
        printf("Spring: center %.1f%% err %.2f%%\n", snapshot.spring_center, snapshot.spring_error);
        printf("Motor effort: %.2f\n", snapshot.motor_effort);
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        s_ip_info = event->ip_info;
        s_ip_obtained = true;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    static int total_len = 0;
    
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            total_len = 0;
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            total_len = 0;
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Accumulate response data
                int data_len = evt->data_len;
                if (total_len + data_len < 1023) {
                    memcpy((char*)evt->user_data + total_len, (char*)evt->data, data_len);
                    total_len += data_len;
                    ((char*)evt->user_data)[total_len] = '\0';
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            total_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            total_len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

static char* get_timezone_from_ip(void)
{
    char *timezone = NULL;
    char response_buffer[1024] = {0};
    
    esp_http_client_config_t config = {
        .url = "http://ip-api.com/json/?fields=timezone",
        .event_handler = http_event_handler,
        .user_data = response_buffer,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d", status_code);
        
        if (status_code == 200) {
            cJSON *json = cJSON_Parse(response_buffer);
            if (json != NULL) {
                cJSON *tz = cJSON_GetObjectItem(json, "timezone");
                if (tz != NULL && cJSON_IsString(tz)) {
                    timezone = strdup(tz->valuestring);
                    ESP_LOGI(TAG, "Detected timezone: %s", timezone);
                }
                cJSON_Delete(json);
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return timezone;
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Connect to WiFi
    ESP_LOGI(TAG, "Connecting to WiFi...");
    wifi_init_sta();

    // Initialize SNTP
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    // Get timezone automatically from IP geolocation
    ESP_LOGI(TAG, "Detecting timezone from IP location...");
    char *timezone = get_timezone_from_ip();
    
    if (timezone != NULL) {
        // Set timezone automatically
        char tz_env[64];
        snprintf(tz_env, sizeof(tz_env), "TZ=%s", timezone);
        setenv("TZ", timezone, 1);
        tzset();
        ESP_LOGI(TAG, "Timezone set to: %s", timezone);
        free(timezone);
    } else {
        // Fallback to UTC if detection fails
        ESP_LOGW(TAG, "Failed to detect timezone, using UTC");
        setenv("TZ", "UTC", 1);
        tzset();
    }

    // Initialize ADC for potentiometer reading
    ESP_LOGI(TAG, "Initializing ADC for potentiometer...");
    
    // Configure ADC
    adc1_config_width(POT_ADC_WIDTH);
    adc1_config_channel_atten(POT_ADC_CHANNEL, POT_ADC_ATTEN);
    adc1_config_channel_atten(CURRENT_ADC_CHANNEL, POT_ADC_ATTEN);
    
    // Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, POT_ADC_ATTEN, POT_ADC_WIDTH, 1100, adc_chars);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(TAG, "ADC characterized using Two Point Value");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(TAG, "ADC characterized using eFuse Vref");
    } else {
        ESP_LOGI(TAG, "ADC characterized using Default Vref");
    }

    // Initialize motor control GPIO pins
    ESP_LOGI(TAG, "Initializing motor control GPIO...");
    gpio_reset_pin(MOTOR_IN1_GPIO);
    gpio_reset_pin(MOTOR_IN2_GPIO);
    gpio_reset_pin(MOTOR_SLEEP_GPIO);
    
    gpio_set_direction(MOTOR_SLEEP_GPIO, GPIO_MODE_OUTPUT);
    
    // Enable motor driver (sleep = HIGH)
    gpio_set_level(MOTOR_SLEEP_GPIO, MOTOR_SLEEP_ENABLE_LEVEL);

    // Configure hardware PWM for motor driver inputs (IN1/IN2)
    ledc_timer_config_t ledc_timer = {
        .speed_mode = MOTOR_PWM_MODE,
        .timer_num = MOTOR_PWM_TIMER,
        .duty_resolution = MOTOR_PWM_DUTY_RES,
        .freq_hz = MOTOR_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ch_in1 = {
        .speed_mode = MOTOR_PWM_MODE,
        .channel = MOTOR_PWM_CH_IN1,
        .timer_sel = MOTOR_PWM_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = MOTOR_IN1_GPIO,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config_t ch_in2 = {
        .speed_mode = MOTOR_PWM_MODE,
        .channel = MOTOR_PWM_CH_IN2,
        .timer_sel = MOTOR_PWM_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = MOTOR_IN2_GPIO,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_in1));
    ESP_ERROR_CHECK(ledc_channel_config(&ch_in2));

    // Initialize motor to stopped
    motor_set_effort(0.0f);

    // Run the controller in its own task; keep printing in a separate low-rate task.
    // This prevents UART printing and screen refresh from adding jitter to motor control.
    xTaskCreatePinnedToCore(control_task, "control", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(status_task, "status", 4096, NULL, 1, NULL, 1);

    vTaskDelete(NULL);
}

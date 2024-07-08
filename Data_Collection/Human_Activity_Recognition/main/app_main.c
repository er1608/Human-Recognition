#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "cJSON.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "MPU6050.h"
#include "max30102.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "esp_sntp.h"

static const char *TAG = "MQTT_EXAMPLE";
const TickType_t xDelay = 50 / portTICK_PERIOD_MS;
static esp_adc_cal_characteristics_t adc1_chars;

#define I2C_SDA 18
#define I2C_SCL 19
#define I2C_FRQ 100000
#define I2C_PORT I2C_NUM_1

max30102_config_t max30102 = {};
max30102_data_t result = {};

static void obtain_time(void) {
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

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
    if (retry == retry_count) {
        ESP_LOGE(TAG, "Failed to obtain time");
    }
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    /*int k = 0;
    char string[20];*/
    int msg_id;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        //ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        
        msg_id = esp_mqtt_client_subscribe(client, "Test", 1);
        //ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        int64_t received_time = esp_timer_get_time();
        int64_t sent_time = 0;
        sscanf(event->data, "%lld", &sent_time);
        int64_t latency = received_time - sent_time;

        time_t now;
        struct tm timeinfo;
        char strftime_buf[64];
        time(&now);
        localtime_r(&now, &timeinfo);

        printf("second: %lld seconds\r\n", now);
        printf("sent_time: %lld microseconds\r\n", sent_time);
        printf("received_time: %lld microseconds\r\n", received_time);
        printf("Latency: %lld microseconds\r\n", latency);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void MQTT_sendJSON(esp_mqtt_client_handle_t client, const char *topic, cJSON *json_data) 
{
    int msg_id;
    char *json_str = cJSON_Print(json_data);
    msg_id = esp_mqtt_client_publish(client, topic, json_str, strlen(json_str), 1, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    cJSON_free(json_str);
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://mqtt.eclipseprojects.io",    // "mqtt://username:password@broker.hivemq.com"
        //.broker.address.uri = "mqtts://admin:P683LO1W3Ucb47UTWlPL1RGOTvsoFkV5@45kg21.stackhero-network.com:1884",
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    int16_t ax, ay, az, gx, gy, gz;
    int msg_id;
    int time = 0;
    char st[10],sax[10],say[10],saz[10],sgx[10],sgy[10],sgz[10],temp[256];
    while(1)
        {
            char JSON1[200] = "{ 1651,A,";
            //MPU
            max30102_update(&max30102, &result);
            //printf("BPM: %f | SpO2: %f%%\n", result.heart_bpm, result.spO2);
            MPU_Get_Accelerometer(&ax, &ay, &az);
            MPU_Get_Gyroscope(&gx, &gy, &gz);
  
            strcat(JSON1, itoa(time,st,10));
            strcat(JSON1, ",");
            strcat(JSON1, gcvt(ax/2048.0,9,sax));
            strcat(JSON1, ",");
            strcat(JSON1, gcvt(ay/2048.0,9,say));
            strcat(JSON1, ",");
            strcat(JSON1, gcvt(az/2048.0,9,saz));
            strcat(JSON1, ",");
            strcat(JSON1, gcvt(gx/131.0,9,sgx));
            strcat(JSON1, ",");
            strcat(JSON1, gcvt(gy/131.0,9,sgy));
            strcat(JSON1, ",");
            strcat(JSON1, gcvt(gz/131.0,9,sgz));
            strcat(JSON1, ",");
            strcat(JSON1, gcvt(result.heart_bpm,9,sgz));
            strcat(JSON1, " }");
            time += 1;

            int64_t sent_time = esp_timer_get_time();
            int64_t time_in_s = sent_time / 1000000;
            int hours = (time_in_s / 3600) % 24;
            int minutes = (time_in_s / 60) % 60;
            int seconds = time_in_s % 60;
            
            snprintf(temp, sizeof(temp), "%d:%d:%d, %s", hours, minutes, seconds, JSON1);

            msg_id = esp_mqtt_client_publish(client, "Test", temp, strlen(temp), 1, 0);
            vTaskDelay(xDelay);
        }
}

esp_err_t i2c_master_init(i2c_port_t i2c_port){
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA;
    conf.scl_io_num = I2C_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_FRQ;
    i2c_param_config(i2c_port, &conf);
    return i2c_driver_install(i2c_port, I2C_MODE_MASTER, 0, 0, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
    
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);
    adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);

    i2c_master_init(I2C_PORT);
    MPU_Init();
    max30102_init( &max30102, I2C_PORT,
                   MAX30102_DEFAULT_OPERATING_MODE,
                   MAX30102_DEFAULT_SAMPLING_RATE,
                   MAX30102_DEFAULT_LED_PULSE_WIDTH,
                   MAX30102_DEFAULT_IR_LED_CURRENT,
                   MAX30102_DEFAULT_START_RED_LED_CURRENT,
                   MAX30102_DEFAULT_MEAN_FILTER_SIZE,
                   MAX30102_DEFAULT_PULSE_BPM_SAMPLE_SIZE,
                   MAX30102_DEFAULT_ADC_RANGE, 
                   MAX30102_DEFAULT_SAMPLE_AVERAGING,
                   MAX30102_DEFAULT_ROLL_OVER,
                   MAX30102_DEFAULT_ALMOST_FULL,
                   false );

    obtain_time();
    mqtt_app_start();
}
/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
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

static const char *TAG = "MQTT_EXAMPLE";
const TickType_t xDelay = 50 / portTICK_PERIOD_MS;
static esp_adc_cal_characteristics_t adc1_chars;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
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
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    MPU_Init();
    int16_t ax, ay, az;
    int msg_id;
    double time = 0;
    char string[10];
    char JSON2[16] = ", \"ay\": ";
    char JSON3[14] = ", \"az\": ";
    char JSON4[12] = ", \"Time\": ";
    char JSON5[5] = " }";
    //int adc_value;
    while(1)
        {
            char JSON1[100] = "{ \"ax\": ";
            // cJSON *json_data = cJSON_CreateObject();    // Create a sample JSON object
            // cJSON_AddNumberToObject(json_data, "temp", k);
            // k += 3;
            // cJSON_AddNumberToObject(json_data, "humidity", k);
            // k -= 1;
            // cJSON_AddNumberToObject(json_data, "Random", k);
            // cJSON_AddNumberToObject(json_data, "Time", time);
            // time += 1;
            // MQTT_sendJSON(client, "Test", json_data);   // Publish the JSON data to a topic
            // cJSON_Delete(json_data);                    // Clean up cJSON object
            // JSON = "{ \"temp\" : " + ",\"humidity\": " + ",\"Random\": " + "}}";
            // adc_value = adc1_get_raw(ADC1_CHANNEL_6);
            MPU_Get_Accelerometer(&ax, &ay, &az);
            //printf("Accel: x=%d, y=%d, z=%d\n", ax, ay, az);
            strcat(JSON1, itoa(ax,string,10));
            strcat(JSON1, JSON2);
            strcat(JSON1, itoa(ay,string,10));
            strcat(JSON1, JSON3);
            strcat(JSON1, itoa(az,string,10));
            strcat(JSON1, JSON4);
            strcat(JSON1, itoa(time,string,10));
            strcat(JSON1, JSON5);
            time += 1;
            msg_id = esp_mqtt_client_publish(client, "Test", JSON1, strlen(JSON1), 2, 0);
            //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            //printf("Accel: %s", JSON1);

            vTaskDelay(xDelay);
        }
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

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    // gpio_config_t GPIO_Config = {};
    // GPIO_Config.pin_bit_mask = (1 << 18);          /*!< GPIO pin: set with bit mask, each bit maps to a GPIO */
    // GPIO_Config.mode = GPIO_MODE_OUTPUT;               /*!< GPIO mode: set input/output mode                     */
    // GPIO_Config.pull_up_en = GPIO_PULLUP_DISABLE;       /*!< GPIO pull-up                                         */
    // GPIO_Config.pull_down_en = GPIO_PULLDOWN_DISABLE;   /*!< GPIO pull-down                                       */
    // GPIO_Config.intr_type = GPIO_INTR_DISABLE;
    // gpio_config(&GPIO_Config);
    // while(1)
    // {
    //     uint8_t data = gpio_get_level(18);
    //     printf("x = %d \n", data);
    //     vTaskDelay(xDelay);
    // }
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);

    adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);

    mqtt_app_start();
}

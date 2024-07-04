#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY CONFIG_ESP_MAXIMUM_RETRY

#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV4_ADDR
#define PORT CONFIG_EXAMPLE_PORT

#define TAG "WiFi station"
#define TAG_UDP "UDP"
#define PAYLOAD "Bonjour Romain"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
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
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS},
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

float generate_random_float(float min, float max)
{
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

void generate_sensor_data(float *temperature, float *humidity, float *lightness, float *aqi, float *gyro_x, float *gyro_y, float *gyro_z)
{
    *temperature = generate_random_float(15.0, 35.0);
    *humidity = generate_random_float(30.0, 90.0);
    *lightness = generate_random_float(100.0, 1000.0);
    *aqi = generate_random_float(0.0, 500.0);
    *gyro_x = generate_random_float(-180.0, 180.0);
    *gyro_y = generate_random_float(-180.0, 180.0);
    *gyro_z = generate_random_float(-180.0, 180.0);
}

static void udp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1)
    {

#if defined(CONFIG_EXAMPLE_IPV4_ADDR)
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
#endif

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG_UDP, "Unable to create socket: errno %d", errno);
            vTaskDelete(NULL);
            return;
        }

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        ESP_LOGI(TAG_UDP, "Socket created, sending to %s:%d", HOST_IP_ADDR, PORT);

        while (1)
        {
            float temperature, humidity, lightness, aqi, gyro_x, gyro_y, gyro_z;
            generate_sensor_data(&temperature, &humidity, &lightness, &aqi, &gyro_x, &gyro_y, &gyro_z);

            char payload[256];
            snprintf(payload, sizeof(payload), "Temperature: %.2f, Humidity: %.2f, Lightness: %.2f, AQI: %.2f, GyroX: %.2f, GyroY: %.2f, GyroZ: %.2f",
                     temperature, humidity, lightness, aqi, gyro_x, gyro_y, gyro_z);

            int err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0)
            {
                ESP_LOGE(TAG_UDP, "Error occurred during sending: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG_UDP, "Message sent: %s", payload);

            struct sockaddr_storage source_addr;
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0)
            {
                ESP_LOGE(TAG_UDP, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else
            {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG_UDP, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG_UDP, "%s", rx_buffer);
                if (strncmp(rx_buffer, "OK: ", 4) == 0)
                {
                    ESP_LOGI(TAG_UDP, "Received expected message, reconnecting");
                    break;
                }
            }

            vTaskDelay(5000 / portTICK_PERIOD_MS); // Wait 5 seconds before sending the next set of data
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG_UDP, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG_UDP, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    srand(time(NULL));

    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
}

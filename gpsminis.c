#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "driver/gpio.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"

#include "esp_event.h"

#include "esp_pm.h"
#include "esp_sleep.h"

#include "esp_timer.h"
#include "esp_log.h"

#define UART_NUM         UART_NUM_1
#define TX_PIN           21 // pin 20/21 UART to gps (neo-6m)
#define RX_PIN           20
#define BUF_SIZE         2048

// Global running variables
double currAvgPos[6] = {0, 0, 0, 0, 0, 0};
double pastAvgPos[2] = {0, 0};
double savedDist = 0.0;
int cycle = 0;
int gpsConnection = 0;
// something something overflow but still works regardless
int lastUpdateTime = 0;
int lastSaveTime = 0;

uint8_t broadcastAddress[] = {0xC8, 0x2E, 0x18, 0xF8, 0x50, 0xE0};

struct __attribute__((packed)) miniGPSdata {
    float averageSpeed;
    float totalDist;
    uint32_t totalTime;
    uint64_t mac;
};

struct miniGPSdata myData = {0.0f, 0.0f, 0, 0xDC0675A981B8};

// UBX configuration command chains
// these suck, i hate using them, yet still need to!
const uint8_t setBaud1152[] = {0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0xD0, 0x08, 0x00, 0x00, 0x00, 0xC2, 0x01, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC4, 0x96};
const uint8_t changeHzMsg[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8, 0x00, 0x01, 0x00, 0x01, 0x00, 0xDE, 0x6A};
const uint8_t disable_GGA[] = {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x00, 0x00, 0xFA, 0x0F};
const uint8_t disable_GLL[] = {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x01, 0x00, 0xFB, 0x11};
const uint8_t disable_GSA[] = {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x02, 0x00, 0xFC, 0x13};
const uint8_t disable_GSV[] = {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x03, 0x00, 0xFD, 0x15};
const uint8_t disable_VTG[] = {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x05, 0x00, 0xFF, 0x19};

// Hardware-targeted Single-Precision Equirectangular calculation pipeline
double calculate_equirectangular(double lat1, double lon1, double lat2, double lon2) {
    double delta_lat_deg = lat2 - lat1;
    double delta_lon_deg = lon2 - lon1;
    double avg_lat_deg = (lat1 + lat2) * 0.5;

    float dlat = (float)(delta_lat_deg * 0.017453292519943295); // pi/180
    float dlon = (float)(delta_lon_deg * 0.017453292519943295);
    float lat_avg = (float)(avg_lat_deg * 0.017453292519943295);
    float x = dlon * cosf(lat_avg);
    float delta_dist = sqrtf(x * x + dlat * dlat) * 6371.0f;
    return (double)delta_dist;
}

// Low-overhead NVS Flash read/write routine
void save_distance_to_nvs(double distance) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_blob(my_handle, "savedDist", &distance, sizeof(double));
        nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}

void load_distance_from_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err == ESP_OK) {
        size_t required_size = sizeof(double);
        nvs_get_blob(my_handle, "savedDist", &savedDist, &required_size);
        nvs_close(my_handle);
    } else {
        savedDist = 0.0;
    }
}

double parse_nmea_degrees(const char *nmea_str, char direction) {
    float raw_val = strtof(nmea_str, NULL);
    int degrees = (int) (raw_val / 100.0f);
    float minutes = raw_val - (degrees * 100.0f);
    double decimal_degrees = degrees + (minutes / 60.0);
    if (direction == 'S' || direction == 'W') {
        decimal_degrees = -decimal_degrees;
    }   
    return decimal_degrees;
}

bool parse_gprmc_line(char *line, double *out_lat, double *out_lon) {
    if (strncmp(line, "$GPRMC", 6) != 0) return false;
    int field_idx = 0;
    char *token = line;
    char *next_token;
    char *fields[12] = {NULL};
    while ((next_token = strchr(token, ',')) != NULL && field_idx < 12) {
        *next_token = '\0';
        fields[field_idx++] = token;
        token = next_token + 1;
    }
    if (field_idx < 7) return false;
    if (fields[2][0] != 'A') return false;
    *out_lat = parse_nmea_degrees(fields[3], fields[4][0]);
    *out_lon = parse_nmea_degrees(fields[5], fields[6][0]);
    return true;
}

void updatePos(double lat, double lon) {
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (cycle == 0) {
        currAvgPos[0] = lat;
        currAvgPos[1] = lon;
    } else if (cycle == 1) {
        currAvgPos[2] = lat;
        currAvgPos[3] = lon;
    } else if (cycle == 2) {
        currAvgPos[4] = (currAvgPos[0] + currAvgPos[2] + lat) / 3.0;
        currAvgPos[5] = (currAvgPos[1] + currAvgPos[3] + lon) / 3.0;
        if (pastAvgPos[0] != 0.0 && pastAvgPos[1] != 0.0) {
            double deltaDist = calculate_equirectangular(pastAvgPos[0], pastAvgPos[1], currAvgPos[4], currAvgPos[5]);
            if (deltaDist > 0.0016764 && deltaDist < 0.0134) { // between 3.75 and 30 mph, 16 min mile -> 2 min mile
                savedDist += deltaDist;
                if ((now_ms - lastSaveTime) > 120000) {
                    save_distance_to_nvs(savedDist);
                    lastSaveTime = now_ms;
                    printf("Lat: %.6f | Lon: %.6f | Total Dist: %.5f\n", lat, lon, savedDist);
                    fflush(stdout);
                }
            }
        }
        pastAvgPos[0] = currAvgPos[4];
        pastAvgPos[1] = currAvgPos[5];
    }
    cycle++;
    if (gpsConnection <= 5) cycle--;
    cycle = cycle % 3;
}

void send_espnow_data() {
    myData.totalDist = (float)savedDist;
    esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));
}

void init_gps_uart() {
    // initial 9600
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_XTAL,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    uart_flush_input(UART_NUM); 
    // this command will be ignored at 115200 baud, if it is a reset**
    uart_write_bytes(UART_NUM, (const char*)setBaud1152, sizeof(setBaud1152));
    vTaskDelay(pdMS_TO_TICKS(75));
    uart_set_baudrate(UART_NUM, 115200);
    vTaskDelay(pdMS_TO_TICKS(75));
    uart_write_bytes(UART_NUM, (const char*)changeHzMsg, sizeof(changeHzMsg));
    uart_write_bytes(UART_NUM, (const char*)disable_GGA, sizeof(disable_GGA));
    uart_write_bytes(UART_NUM, (const char*)disable_GLL, sizeof(disable_GLL));
    uart_write_bytes(UART_NUM, (const char*)disable_GSA, sizeof(disable_GSA));
    uart_write_bytes(UART_NUM, (const char*)disable_GSV, sizeof(disable_GSV));
    uart_write_bytes(UART_NUM, (const char*)disable_VTG, sizeof(disable_VTG));
    vTaskDelay(pdMS_TO_TICKS(50));
}

void init_wifi_espnow() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Enable aggressive modem sleep for the Wi-Fi radio
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM));

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));
}

void configure_power_management() {
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 40,
        .light_sleep_enable = true
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
    uart_set_wakeup_threshold(UART_NUM, 3);
    esp_sleep_enable_uart_wakeup(UART_NUM);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    load_distance_from_nvs();
    // init_wifi_espnow();
    configure_power_management();

    vTaskDelay(pdMS_TO_TICKS(300));

    init_gps_uart();

    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    char line_buffer[128];
    int line_buffer_idx = 0;
    // while (1) {
    //     // Increase block time to 200ms to match the 5Hz GPS incoming frequency
    //     int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(185));
    //     if (len > 0) {
    //         gpsConnection++;
    //         for (int i = 0; i < len; i++) {
    //             char c = data[i];
    //             if (c == '\n' || c == '\r') {
    //                 if (line_buffer_idx > 0) {
    //                     line_buffer[line_buffer_idx] = '\0';
    //                     double lat = 0, lon = 0;
    //                     int now_ms = esp_timer_get_time() / 1000;
    //                     if (parse_gprmc_line(line_buffer, &lat, &lon)) {
    //                         if ((now_ms - lastUpdateTime) > 100) {
    //                             updatePos(lat, lon);
                                    
    //                             // send_espnow_data();
    //                             lastUpdateTime = now_ms;
    //                         }
    //                     }
    //                     line_buffer_idx = 0;
    //                 }
    //             } else {
    //                 if (line_buffer_idx < sizeof(line_buffer) - 1) {
    //                     line_buffer[line_buffer_idx++] = c;
    //                 }
    //             }
    //         }
    //     } else {
    //         // Force a clean RTOS block if the UART buffer is empty, allowing light sleep
    //         vTaskDelay(pdMS_TO_TICKS(80));
    //     }
    // }

    int64_t total_uart_time = 0;
    int64_t total_process_time = 0;
    int64_t total_espnow_time = 0;
    int profile_count = 0;

    while (1) {
        int64_t t0 = esp_timer_get_time();

        // 1. Measure UART block time
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(200));
        
        int64_t t1 = esp_timer_get_time();
        total_uart_time += (t1 - t0);

        if (len > 0) {
            gpsConnection++;
            for (int i = 0; i < len; i++) {
                char c = data[i];
                if (c == '\n' || c == '\r') {
                    if (line_buffer_idx > 0) {
                        line_buffer[line_buffer_idx] = '\0';
                        double lat = 0, lon = 0;
                        int64_t now_ms = esp_timer_get_time() / 1000;
                        
                        if (parse_gprmc_line(line_buffer, &lat, &lon)) {
                            if ((now_ms - lastUpdateTime) > 50) {
                                updatePos(lat, lon);
                                // if (printLoc) {
                                //     printf("Lat: %.6f | Lon: %.6f | Total Dist: %.5f\n", lat, lon, savedDist);
                                //     fflush(stdout);
                                // }
                                
                                // 2. Measure ESP-NOW transmit time
                                int64_t t_now_0 = esp_timer_get_time();
                                send_espnow_data();
                                total_espnow_time += (esp_timer_get_time() - t_now_0);
                                
                                lastUpdateTime = now_ms;
                            }
                        }
                        line_buffer_idx = 0;
                    }
                } else {
                    if (line_buffer_idx < sizeof(line_buffer) - 1) {
                        line_buffer[line_buffer_idx++] = c;
                    }
                }
            }
            total_process_time += (esp_timer_get_time() - t1);
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        // Print average metrics every 100 loops
        profile_count++;
        if (profile_count >= 100) {
            printf("\n--- Profiling Metrics (Average microseconds over 100 loops) ---\n");
            printf("UART Read/Block: %lld us\n", total_uart_time / 100);
            printf("Data Processing: %lld us\n", total_process_time / 100);
            printf("ESP-NOW Transmit: %lld us\n", total_espnow_time / 100);
            printf("-------------------------------------------------------------\n\n");
            fflush(stdout);

            // Reset counters
            total_uart_time = 0;
            total_process_time = 0;
            total_espnow_time = 0;
            profile_count = 0;
        }
    }
    free(data);
}
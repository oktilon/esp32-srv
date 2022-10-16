/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include <nvs_flash.h>
#include <sys/param.h>

#include "esp_netif.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>

#include "lwip/err.h"
#include "lwip/sys.h"

#include "index.h"

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */
// CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_SSID       "den-esp"
// CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_PASS       "den12345"
// CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_ESP_WIFI_CHANNEL    7
// CONFIG_ESP_MAX_STA_CONN
#define EXAMPLE_MAX_STA_CONN        4

#define LED                         22

#define UART_BUFFER_SIZE            128

static const char *TAG = "wifi-srv";

uint8_t ledIsOn = 0;

uint8_t uart_tx[UART_BUFFER_SIZE];
uint8_t uart_rx[UART_BUFFER_SIZE];

typedef struct {
    uint8_t led_state;
    const char* msg;
} my_struct_t;

void init_uart(void);


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}


/* An HTTP GET handler */
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Hello World!"
};

static esp_err_t led_get_handler(httpd_req_t *req) {
    const my_struct_t *pmy = (my_struct_t*)req->user_ctx;
    gpio_set_level(LED, pmy->led_state);
    char resp_str[50];
    sprintf(resp_str, "%d", pmy->led_state);
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

my_struct_t my_on = {
    .led_state = 1,
    .msg = "ON"
};

my_struct_t my_off = {
    .led_state = 0,
    .msg = "OFF"
};

static const httpd_uri_t led_on = {
    .uri       = "/led_on",
    .method    = HTTP_GET,
    .handler   = led_get_handler,
    .user_ctx  = &my_on
};

static const httpd_uri_t led_off = {
    .uri       = "/led_off",
    .method    = HTTP_GET,
    .handler   = led_get_handler,
    .user_ctx  = &my_off
};

/* An HTTP POST handler */
static esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t echo = {
    .uri       = "/echo",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

/* An HTTP PUT handler. This demonstrates realtime
 * registration and deregistration of URI handlers
 */
static esp_err_t ctrl_put_handler(httpd_req_t *req)
{
    char buf;
    int ret;

    if ((ret = httpd_req_recv(req, &buf, 1)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if (buf == '0') {
        /* URI handlers can be unregistered using the uri string */
        ESP_LOGI(TAG, "Unregistering /hello and /echo URIs");
        httpd_unregister_uri(req->handle, "/hello");
        httpd_unregister_uri(req->handle, "/echo");
        /* Register the custom error handler */
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    else {
        ESP_LOGI(TAG, "Registering /hello and /echo URIs");
        httpd_register_uri_handler(req->handle, &hello);
        httpd_register_uri_handler(req->handle, &echo);
        /* Unregister custom error handler */
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, NULL);
    }

    /* Respond with empty body */
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t ctrl = {
    .uri       = "/ctrl",
    .method    = HTTP_PUT,
    .handler   = ctrl_put_handler,
    .user_ctx  = NULL
};


static esp_err_t index_get_handler(httpd_req_t *req) {
    int i;
    int sz;
    int rep = 0;
    int lvl = gpio_get_level(LED);
    char buf[1813] = {0};
    const char *led_lvl = lvl ? "ON " : "OFF";
    const char *led_cls = lvl ? "on " : "off";
    for(i = 0; i < index_htm_len; i++) {
        buf[i] = index_htm[i];
        if(i > 3 && buf[i-2] == 'C' && buf[i-1] == 'C' && buf[i] == 'C') {
            buf[i-2] = led_cls[0];
            buf[i-1] = led_cls[1];
            buf[i] = led_cls[2];
        }
        if(i > 3 && buf[i-2] == 'T' && buf[i-1] == 'T' && buf[i] == 'T') {
            buf[i-2] = led_lvl[0];
            buf[i-1] = led_lvl[1];
            buf[i] = led_lvl[2];
        }
    }
    buf[i] = 0;
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Content-Type", "text/html; charset=UTF-8");
    // httpd_resp_send(req, "<!DOCTYPE html><html><head><title>ESP32 Server</title></head><body><center>It works</center></body></html>", 106);
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t uri_index = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_get_handler,
    .user_ctx  = NULL
};

static esp_err_t send_handler(httpd_req_t *req) {
    int rxBytes = 0;
    char server_string[2] = {0x39, 0}; // 9 - error
    int timeout = 20;

    uart_tx[0] = 0xAA;
    uart_tx[1] = 0x55;
    uart_tx[2] = 0x00;
    uart_tx[3] = 0x55;

    if(ledIsOn) {
        uart_tx[2] = 0x00;
        ledIsOn = 0;
        server_string[0] = 0x37;
    } else {
        uart_tx[2] = 0x01;
        ledIsOn = 1;
        server_string[0] = 0x38;
    }

    uart_write_bytes(UART_NUM_1, uart_tx, 4);
    uart_flush_input(UART_NUM_1);

    // while (rxBytes < 1 && timeout > 0) {
    //     rxBytes = uart_read_bytes(UART_NUM_1, uart_rx, 2, 200 / portTICK_RATE_MS);
    //     timeout--;
    // }
    // if(rxBytes > 0) {
    //     if(uart_rx[0] == 0xAA) {
    //         if(uart_rx[1] == 0x31) {
    //             gpio_set_level(LED, 1);
    //             server_string[0] = 0x31;
    //         } else {
    //             gpio_set_level(LED, 0);
    //             server_string[0] = 0x30;
    //         }
    //     }
    //     // if(uart_rx[0] == 0xAA && uart_rx[1] == 0x55) {
    //     //     // if(uart_rx[0] == 0xAA) {

    //     //     // }
    //     //     server_string[0] = (char)uart_rx[2];
    //     // }
    // }
    httpd_resp_send(req, server_string, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t uri_send = {
    .uri       = "/send",
    .method    = HTTP_GET,
    .handler   = send_handler,
    .user_ctx  = NULL
};

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &uri_index);
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &echo);
        httpd_register_uri_handler(server, &ctrl);
        httpd_register_uri_handler(server, &led_on);
        httpd_register_uri_handler(server, &led_off);
        httpd_register_uri_handler(server, &uri_send);
        #if CONFIG_EXAMPLE_BASIC_AUTH
        httpd_register_basic_auth(server);
        #endif
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

// static void disconnect_handler(void* arg, esp_event_base_t event_base,
//                                int32_t event_id, void* event_data)
// {
//     httpd_handle_t* server = (httpd_handle_t*) arg;
//     if (*server) {
//         ESP_LOGI(TAG, "Stopping webserver");
//         stop_webserver(*server);
//         *server = NULL;
//     }
// }

// static void connect_handler(void* arg, esp_event_base_t event_base,
//                             int32_t event_id, void* event_data)
// {
//     httpd_handle_t* server = (httpd_handle_t*) arg;
//     if (*server == NULL) {
//         ESP_LOGI(TAG, "Starting webserver");
//         *server = start_webserver();
//     }
// }


void init_uart() {
    const uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB
    };
    int rx_buf_size = UART_BUFFER_SIZE * 2;
    uart_driver_install(UART_NUM_1, rx_buf_size, 0, 20, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    //                         TX = 4       RX = 5
    uart_set_pin(UART_NUM_1, GPIO_NUM_4, GPIO_NUM_5, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}



void app_main(void)
{

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_uart();

    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    gpio_set_level(LED, 0);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
    (void)start_webserver();

    /*uart_tx[0] = 0x33;
    uart_tx[1] = 0x33;
    uart_tx[2] = 0x33;
    uart_write_bytes(UART_NUM_1, uart_tx, 3);
    uart_flush_input(UART_NUM_1);

    int rxBytes = 0;
    while(1) {
        uart_flush_input(UART_NUM_1);

        while (rxBytes < 1) {
            rxBytes = uart_read_bytes(UART_NUM_1, uart_rx, 1, 200 / portTICK_RATE_MS);
        }
        if(rxBytes > 0) {
            if(uart_rx[0] == 0x34) {
                gpio_set_level(LED, 1);

                uart_tx[0] = 0x34;
                uart_tx[1] = 0x30;
                uart_write_bytes(UART_NUM_1, uart_tx, 2);
                uart_flush_input(UART_NUM_1);
            } else {
                gpio_set_level(LED, 0);

                uart_tx[0] = 0x34;
                uart_tx[1] = 0x34;
                uart_write_bytes(UART_NUM_1, uart_tx, 2);
                uart_flush_input(UART_NUM_1);
            }
            uart_rx[0] = 0;
        }
        rxBytes = 0;
    }*/
}

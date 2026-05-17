/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/*
 * WiFi softAP & station Example
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_server.h"

#include "cJSON.h"
#include "rgb_led.h"
#include "touchpad.h"
#include "delay.h"

/* STA Configuration */
#define EXAMPLE_ESP_WIFI_STA_SSID      CONFIG_ESP_WIFI_REMOTE_AP_SSID
#define EXAMPLE_ESP_WIFI_STA_PASSWD    CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY      CONFIG_ESP_MAXIMUM_STA_RETRY

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* AP Configuration */
#define EXAMPLE_ESP_WIFI_AP_SSID       CONFIG_ESP_WIFI_AP_SSID
#define EXAMPLE_ESP_WIFI_AP_PASSWD     CONFIG_ESP_WIFI_AP_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL       CONFIG_ESP_WIFI_AP_CHANNEL
#define EXAMPLE_MAX_STA_CONN           CONFIG_ESP_MAX_STA_CONN_AP

/* DHCP server option */
#define DHCPS_OFFER_DNS 0x02

static const char *TAG_AP = "WiFi SoftAP";
static const char *TAG_STA = "WiFi Sta";

static int s_retry_num = 0;

static volatile bool wifi_conectado = false;    //Se crean variables globales para controlar el estado de la conexión WiFi, ya que el evento de conexión es asíncrono.Para no usar RTOS 
static volatile bool wifi_fallo = false;

/* Estado actual del LED */
static int led_r = 0;
static int led_g = 0;
static int led_b = 0;
static bool led_on = false;

/* HTML de index embebido */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

/* Actualiza el LED físico y guarda el estado actual */
static void set_led_state(int r, int g, int b)
{
    led_r = r;
    led_g = g;
    led_b = b;

    led_on = !(r == 0 && g == 0 && b == 0);

    rgb_led_set_color(r, g, b);
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station " MACSTR " joined, AID=%d",
                 MAC2STR(event->mac), event->aid);

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG_AP, "Station " MACSTR " left, AID=%d, reason:%d",
                 MAC2STR(event->mac), event->aid, event->reason);

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG_STA, "Station started");

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG_STA, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        wifi_conectado = true;                  //Aasignacion de variable para controlar el estado de la conexión WiFi                                    
 
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG_STA, "Reintentando conectar al AP... (Intento %d)", s_retry_num);
        } else {
            wifi_fallo = true;                //Aasignacion de variable para controlar el estado de la conexión WiFi    
            ESP_LOGE(TAG_STA, "Fallo definitivo al conectar al AP");
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ASSIGNED_IP_TO_CLIENT) {
        const ip_event_assigned_ip_to_client_t *e =
            (const ip_event_assigned_ip_to_client_t *) event_data;

        ESP_LOGI(TAG_AP, "Assigned IP to client: " IPSTR ", MAC=" MACSTR ", hostname='%s'",
                 IP2STR(&e->ip), MAC2STR(e->mac), e->hostname);
    }
}

/* Initialize soft AP */
esp_netif_t *wifi_init_softap(void)
{
    esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_AP_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_AP_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_AP_PASSWD,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 0,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    if (strlen(EXAMPLE_ESP_WIFI_AP_PASSWD) == 0) {
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

    ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_AP_SSID,
             EXAMPLE_ESP_WIFI_AP_PASSWD,
             EXAMPLE_ESP_WIFI_CHANNEL);

    return esp_netif_ap;
}

/* Initialize WiFi station */
esp_netif_t *wifi_init_sta(void)
{
    esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_STA_SSID,
            .password = EXAMPLE_ESP_WIFI_STA_PASSWD,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = EXAMPLE_ESP_MAXIMUM_RETRY,
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    ESP_LOGI(TAG_STA, "wifi_init_sta finished.");

    return esp_netif_sta;
}

/* Respuesta para cargar la página principal */
esp_err_t http_get_handler(httpd_req_t *req)
{
    const char *html = (const char *) index_html_start;
    size_t html_len = index_html_end - index_html_start;

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, html_len);

    return ESP_OK;
}

/* Recibe JSON por POST /led y actualiza el LED */
esp_err_t http_led_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    char *buf = malloc(total_len + 1);
    int recibido = 0;

    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No hay memoria");
        return ESP_FAIL;
    }

    while (recibido < total_len) {
        int len = httpd_req_recv(req, buf + recibido, total_len - recibido);

        if (len <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Error al recibir datos");
            return ESP_FAIL;
        }

        recibido += len;
    }

    buf[total_len] = '\0';

    cJSON *json = cJSON_Parse(buf);

    if (!json) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON inválido");
        return ESP_FAIL;
    }

    cJSON *r = cJSON_GetObjectItem(json, "r");
    cJSON *g = cJSON_GetObjectItem(json, "g");
    cJSON *b = cJSON_GetObjectItem(json, "b");

    if (cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b)) {
        set_led_state(r->valueint, g->valueint, b->valueint);
    } else {
        cJSON_Delete(json);
        free(buf);
        httpd_resp_send_err(req,
                            HTTPD_400_BAD_REQUEST,
                            "Faltan campos r, g o b o no son números");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "LED actualizado");

    cJSON_Delete(json);
    free(buf);

    return ESP_OK;
}

/* Devuelve el estado actual del LED por GET /led */
esp_err_t http_led_get_handler(httpd_req_t *req)
{
    char resp[120];

    snprintf(resp,
             sizeof(resp),
             "{\"r\":%d,\"g\":%d,\"b\":%d,\"on\":%s}",
             led_r,
             led_g,
             led_b,
             led_on ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);

    return ESP_OK;
}

/* Inicialización del servidor HTTP */
httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t uri_get = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = http_get_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &uri_get);

        httpd_uri_t uri_get_led = {
            .uri = "/led",
            .method = HTTP_GET,
            .handler = http_led_get_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &uri_get_led);

        httpd_uri_t uri_post_led = {
            .uri = "/led",
            .method = HTTP_POST,
            .handler = http_led_post_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &uri_post_led);
    }

    return server;
}

void softap_set_dns_addr(esp_netif_t *esp_netif_ap, esp_netif_t *esp_netif_sta)
{
    esp_netif_dns_info_t dns;

    esp_netif_get_dns_info(esp_netif_sta, ESP_NETIF_DNS_MAIN, &dns);

    uint8_t dhcps_offer_option = DHCPS_OFFER_DNS;

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(esp_netif_ap));

    ESP_ERROR_CHECK(esp_netif_dhcps_option(esp_netif_ap,
                                           ESP_NETIF_OP_SET,
                                           ESP_NETIF_DOMAIN_NAME_SERVER,
                                           &dhcps_offer_option,
                                           sizeof(dhcps_offer_option)));

    ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_netif_ap,
                                           ESP_NETIF_DNS_MAIN,
                                           &dns));

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(esp_netif_ap));
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_ASSIGNED_IP_TO_CLIENT,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    ESP_LOGI(TAG_AP, "ESP_WIFI_MODE_AP");
    esp_netif_t *esp_netif_ap = wifi_init_softap();

    ESP_LOGI(TAG_STA, "ESP_WIFI_MODE_STA");
    esp_netif_t *esp_netif_sta = wifi_init_sta();

    ESP_ERROR_CHECK(esp_wifi_start());

    while (!wifi_conectado && !wifi_fallo) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    if (wifi_conectado) {
        ESP_LOGI(TAG_STA, "¡Conectado al AP!");
        softap_set_dns_addr(esp_netif_ap, esp_netif_sta);
    } else {
        ESP_LOGE(TAG_STA, "Fallo al conectar");
    }

    rgb_led_init();

    touchpad_init();

    set_led_state(0, 0, 0);

    start_webserver();

    ESP_LOGI("web",
             "Servidor web iniciado. Accede a http://192.168.4.1 para ver la respuesta.");

    esp_netif_set_default_netif(esp_netif_sta);

    bool last_state[TOUCHPAD_BUTTON_COUNT] = {false};

while (1) {
    for (uint8_t i = 0; i < TOUCHPAD_BUTTON_COUNT; i++) {
        bool pressed = touchpad_is_pressed(i);

        if (pressed && !last_state[i]) {
            if (i == 0) {
                set_led_state(255, 0, 0);      // rojo
            } else if (i == 1) {
                set_led_state(0, 0, 0);        // apagado
            } else if (i == 2) {
                set_led_state(0, 0, 255);      // azul
            } else if (i == 3) {
                set_led_state(255, 255, 0);    // amarillo
            } else if (i == 4) {
                set_led_state(255, 0, 255);    // violeta
            } else if (i == 5) {
                set_led_state(0, 255, 0);      // verde
            }
        }

        last_state[i] = pressed;
    }

    delay_ms(50);
}

    /*
     * Esta funcionalidad era para que actúe como repetidor WiFi.
     *
     * if (esp_netif_napt_enable(esp_netif_ap) != ESP_OK) {
     *     ESP_LOGE(TAG_STA, "NAPT not enabled on the netif: %p", esp_netif_ap);
     * }
     */
}
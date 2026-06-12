#include "ssdp.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "templates.h"
#include "Global.h"

static const char *TAG = "SSDP";
static char local_ip[16];

static TaskHandle_t ssdp_task_handle = NULL;
volatile bool ssdp_running = false;
static int ssdp_sock = -1;


static void ssdp_update_ip(void)
{
    esp_netif_ip_info_t ip;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
        esp_ip4addr_ntoa(&ip.ip, local_ip, sizeof(local_ip));
    }
}

void ssdp_task(void *arg)
{
    struct sockaddr_in addr, source_addr;
    socklen_t socklen = sizeof(source_addr);
    char rx_buf[512];

    ssdp_running = true;

    ssdp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ssdp_sock < 0) {
        ESP_LOGE(TAG, "Erro criando socket");
        goto exit;
    }

    int reuse = 1;
    setsockopt(ssdp_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(1900);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ssdp_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Erro no bind");
        goto exit;
    }

    struct ip_mreq mreq = {
        .imr_multiaddr.s_addr = inet_addr("239.255.255.250"),
        .imr_interface.s_addr = htonl(INADDR_ANY),
    };

    setsockopt(ssdp_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    ESP_LOGI(TAG, "SSDP escutando...");

    while (ssdp_running) {

        int len = recvfrom(
            ssdp_sock,
            rx_buf,
            sizeof(rx_buf) - 1,
            0,
            (struct sockaddr *)&source_addr,
            &socklen
        );

        if (len < 0) {
            if (!ssdp_running) break; // socket fechado pelo stop
            continue;
        }

        rx_buf[len] = 0;

        if (strstr(rx_buf, "M-SEARCH") &&
            strstr(rx_buf, "ssdp:discover")) {

            char response[512];
            int len = template_build_ssdp_response(
                response,
                sizeof(response),
                local_ip
            );

            sendto(
                ssdp_sock,
                response,
                len,
                0,
                (struct sockaddr *)&source_addr,
                sizeof(source_addr)
            );

            #if DEBUG_SSDP
                ESP_LOGI(TAG, "SSDP respondido para %s:%d",
                        inet_ntoa(source_addr.sin_addr),
                        ntohs(source_addr.sin_port));
            #endif
        }
    }

exit:
    if (ssdp_sock >= 0) {
        close(ssdp_sock);
        ssdp_sock = -1;
    }

    ssdp_task_handle = NULL;
    ssdp_running = false;

    ESP_LOGI(TAG, "SSDP task finalizada");
    vTaskDelete(NULL);
}


void ssdp_start(void)
{
    if (ssdp_task_handle) {
        return; // já rodando
    }

    ssdp_update_ip();

    xTaskCreate(
        ssdp_task,
        "ssdp_task",
        4096,
        NULL,
        5,
        &ssdp_task_handle
    );
}


void ssdp_stop(void)
{
    if (!ssdp_task_handle) return;

    ssdp_running = false;

    // força o recvfrom a destravar
    if (ssdp_sock >= 0) {
        shutdown(ssdp_sock, SHUT_RDWR);
        close(ssdp_sock);
        ssdp_sock = -1;
    }
}


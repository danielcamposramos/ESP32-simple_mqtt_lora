#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "lora.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_EXAMPLE";
static char meslora[180] = "";
static int msg_id;
uint8_t but[152];

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Ultimo erro %s: 0x%x", message, error_code);
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
    ESP_LOGD(TAG, "Evento disparado do loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "PPMEC_UNB_2023", "Gateway esp32 DCR - Conectado ao servidor MQTT Broker.", 0, 1, 0);
        ESP_LOGI(TAG, "Dados publicados com sucesso, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "PPMEC_UNB_2023", 0);
        ESP_LOGI(TAG, "Inscricao enviada com sucesso, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        ESP_LOGI(TAG, "MQTT desconectado, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_SUBSCRIBED:
/*      ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        */
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("Topico = %.*s\r\n", event->topic_len, event->topic);
        printf("Mensagem = %.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reportado por esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reportado por tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("capturado como um erro de transporte de socket n",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Ultima string de erro n (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Outro evento de id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://zionbtnet.ddns.net",
        .broker.address.port = 1883,
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Digite o endereco do servidor Broker e aperte enter\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Endereco do Servidor Broker: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuracao desencontrada: endereco do servidor broker errado");
        abort();
    }
#endif
   esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
   esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
   esp_mqtt_client_start(client);
   int x;
   for(;;) {
      lora_receive();    // put into receive mode
      while(lora_received()) {
         x = lora_receive_packet(but, sizeof(but));
         but[x] = 0;
         sprintf(meslora, "Gateway DCR: %s", but);
         printf("LORA - Recebido: %s\n", but);
         esp_mqtt_client_publish(client, "PPMEC_UNB_2023", meslora, 180, 1, 0);
         printf("Dados publicados com sucesso, msg_id=%d", msg_id);
         lora_receive();
      }
      vTaskDelay(1000);
   }
}

/*
void task_rx(void *)
{
   
   }
}
*/
void app_main()
{
   lora_init();
   lora_set_frequency(915e6);
   lora_enable_crc();
   ESP_ERROR_CHECK(nvs_flash_init());
   ESP_ERROR_CHECK(esp_netif_init());
   ESP_ERROR_CHECK(esp_event_loop_create_default());
   ESP_ERROR_CHECK(example_connect());
   
   mqtt_app_start();
  //xTaskCreate(&task_rx, "task_rx", 3078, NULL, 5, NULL);
}

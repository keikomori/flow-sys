/*
  Autor: Tatiany Keiko Mori
  Hardware: NodeMCU ESP32
  Espressif ESP-IDF: v4.2.2
*/
/*
   Pulse counter module - Flowmeter
   Based on example found on esp-idf repo: https://github.com/espressif/esp-idf/tree/6c49f1924/examples/peripherals/pcnt
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
   */

/* main.c -- MQTT client example
 *
 * Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of Redis nor the names of its contributors may be used
 * to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Biblioteca C
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/**
 * FreeRTOS
 */
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "freertos/semphr.h"
 #include "freertos/queue.h"
 #include "freertos/event_groups.h"
 #include "freertos/portmacro.h"

#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sntp.h"
/**
 * NVS
 */
#include "nvs_flash.h"
/**
 * Lwip
 */
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/api.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"
/**
 * Lib MQTT
 */
#include "mqtt_client.h"
#include "esp_tls.h"
/**
 * GPIOs;
 */
#include "driver/gpio.h"
#include "driver/pcnt.h"
#include "driver/periph_ctrl.h"
#include "soc/gpio_sig_map.h"
 /**
  * Lib Wifimanager
  */
#include "wifi_manager.h"
/**
 * Definições Gerais
 */
#define DEBUG 1
#define TOPICO_FLOW "flow"
/**
 * TEST CODE BRIEF
 *
 * Use PCNT module to count pulses generated by a flowmeter hall sensor..
 *
 * The signal is connected to GPIO4,
 *
 * Load example, open a serial port to view the message printed on your screen.
 *
 * An interrupt will be triggered when the counter value:
 *   - reaches 'thresh1' or 'thresh0' value,
 *   - reaches 'l_lim' value or 'h_lim' value,
 *   - will be reset to zero.
 */
 #define PCNT_TEST_UNIT      PCNT_UNIT_0
 #define PCNT_H_LIM_VAL      1000
 #define PCNT_THRESH1_VAL    500
 #define PCNT_THRESH0_VAL   -500
 #define PCNT_INPUT_SIG_IO   18  // Pulse Input GPIO

xQueueHandle pcnt_evt_queue;   // A queue to handle pulse counter events
pcnt_isr_handle_t user_isr_handle = NULL; //user's ISR service handle

/* A sample structure to pass events from the PCNT
 * interrupt handler to the main program.
 */
typedef struct {
    int unit;  // the PCNT unit that originated an interrupt
    uint32_t status; // information on the event type that caused the interrupt
} pcnt_evt_t;
/**
 * Variáveis Globais
 */
static const char *TAG = "main: ";
static EventGroupHandle_t wifi_event_group;
static EventGroupHandle_t mqtt_event_group;
const static int WIFI_CONNECTED_BIT = BIT0;
QueueHandle_t Queue_Flow;
esp_mqtt_client_handle_t client;

static void mqtt_app_start( void );
static void task_flow( void *pvParameter );
void connection_off(void *pvParameter);
void connection_on(void *pvParameter);
static void IRAM_ATTR pcnt_intr_handler(void *arg);
static void pcnt_init(void);

/**
 * Função de callback chamada quando o roteador aceitar a conexão do ESP32 na rede WiFi;
 */
void connection_on(void *pvParameter)
{
	if( DEBUG )
		ESP_LOGI(TAG, "Online...");

	xEventGroupSetBits( wifi_event_group, WIFI_CONNECTED_BIT );
}

/**
 * Função de callback chamada quando o ESP32 estiver offline na rede WiFi;
 */
void connection_off(void *pvParameter)
{
	if( DEBUG )
		ESP_LOGI(TAG, "Offline...");

	xEventGroupClearBits( wifi_event_group, WIFI_CONNECTED_BIT );
}

/**
 * Função de callback do stack MQTT;
 * Por meio deste callback é possível receber as notificações com os status
 * da conexão e dos tópicos assinados e publicados;
 */
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    client = event->client;

    switch (event->event_id)
    {

        case MQTT_EVENT_BEFORE_CONNECT:
			if ( DEBUG )
				ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;

        case MQTT_EVENT_CONNECTED:

            if ( DEBUG )
                ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

            xEventGroupSetBits( mqtt_event_group, WIFI_CONNECTED_BIT );
            break;

        case MQTT_EVENT_DISCONNECTED:

            if( DEBUG )
                ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");

            xEventGroupClearBits(mqtt_event_group, WIFI_CONNECTED_BIT);
            break;


        case MQTT_EVENT_PUBLISHED:

            if ( DEBUG )
                ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_ERROR:
            if ( DEBUG )
                ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;

		case MQTT_EVENT_ANY:
			if ( DEBUG )
				ESP_LOGI(TAG, "MQTT_EVENT_ANY");
			break;
    }
    return ESP_OK;
}
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
	if ( DEBUG )
		ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start( void )
{
    const esp_mqtt_client_config_t mqtt_cfg = {
		.uri = "mqtt://localhost:1883",
		.username = "sensorflow",
		.password = "123456",
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

/* Decode what PCNT's unit originated an interrupt
 * and pass this information together with the event type
 * the main program using a queue.
 */
static void IRAM_ATTR pcnt_intr_handler(void *arg)
{
    uint32_t intr_status = PCNT.int_st.val;
    int i;
    pcnt_evt_t evt;
    portBASE_TYPE HPTaskAwoken = pdFALSE;

    for (i = 0; i < PCNT_UNIT_MAX; i++) {
        if (intr_status & (BIT(i))) {
            evt.unit = i;
            /* Save the PCNT event type that caused an interrupt
               to pass it to the main program */
            evt.status = PCNT.status_unit[i].val;
            PCNT.int_clr.val = BIT(i);
            xQueueSendFromISR(pcnt_evt_queue, &evt, &HPTaskAwoken);
            if (HPTaskAwoken == pdTRUE) {
                portYIELD_FROM_ISR();
            }
        }
    }
}

/* Initialize PCNT functions:
 *  - configure and initialize PCNT
 *  - set up the input filter
 *  - set up the counter events to watch
 */
static void pcnt_init(void)
{
    /* Prepare configuration for the PCNT unit */
    pcnt_config_t pcnt_config = {
        // Set PCNT input signal and control GPIOs
        .pulse_gpio_num = PCNT_INPUT_SIG_IO, // Configura GPIO para entrada dos pulsos
        .channel = PCNT_CHANNEL_0, // Canal de contagem PCNT - 0
        .unit = PCNT_TEST_UNIT, // Unidade de contagem PCNT - 0
        .pos_mode = PCNT_COUNT_INC,   // Incrementa contagem na subida do pulso
        // Set the maximum limit value
        .counter_h_lim = PCNT_H_LIM_VAL,
    };
    /* Initialize PCNT unit */
    pcnt_unit_config(&pcnt_config);

    /* Configure and enable the input filter */
    pcnt_set_filter_value(PCNT_TEST_UNIT, 100);
    pcnt_filter_enable(PCNT_TEST_UNIT);

    /* Set threshold 0 and 1 values and enable events to watch */
    pcnt_set_event_value(PCNT_TEST_UNIT, PCNT_EVT_THRES_1, PCNT_THRESH1_VAL);
    pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_THRES_1);
    /* Enable events on zero, maximum and minimum limit values */
    pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_ZERO);
    pcnt_event_enable(PCNT_TEST_UNIT, PCNT_EVT_H_LIM);

    /* Initialize PCNT's counter */
    pcnt_counter_pause(PCNT_TEST_UNIT);
    pcnt_counter_clear(PCNT_TEST_UNIT);

    /* Register ISR handler and enable interrupts for PCNT unit */
    pcnt_isr_register(pcnt_intr_handler, NULL, 0, &user_isr_handle);
    pcnt_intr_enable(PCNT_TEST_UNIT);

    /* Everything is set up, now go to counting */
    pcnt_counter_resume(PCNT_TEST_UNIT);
}

/**
 * Task responsável pela manipulação
 */
static void task_flow( void *pvParameter )
{
    /* Initialize PCNT event queue and PCNT functions */
    pcnt_evt_queue = xQueueCreate(10, sizeof(pcnt_evt_t));
    pcnt_init();

    char str[127];

    int16_t count = 0;
    unsigned long aux = 0;

    unsigned long counterPulses;
    unsigned long multPulses = 0;

    pcnt_evt_t evt;
    portBASE_TYPE res;

    for(;;)
	  {
        /* Aguarde que as informações do evento sejam transmitidas pelo manipulador de interrupção do PCNT.
         * Depois de recebido, decodifique o tipo de evento e imprima-o no monitor serial.
         */
        res = xQueueReceive(pcnt_evt_queue, &evt, 1000 / portTICK_PERIOD_MS);
        if (res == pdTRUE)
        {
            pcnt_get_counter_value(PCNT_TEST_UNIT, &count);
            printf("Event PCNT unit[%d]; cnt: %d\n", evt.unit, count);
            printf("\n%d\n", count);
            if ( DEBUG )
                ESP_LOGI( TAG, "Event PCNT unit[%d]; cnt: %d\n", evt.unit, count );
            
            if (evt.status & PCNT_STATUS_THRES1_M)
            {
                printf("THRES1 EVT\n");
                if( DEBUG )
                    ESP_LOGI( TAG, "THRES1 EVT\n" );
            }
            if (evt.status & PCNT_STATUS_H_LIM_M)
            {
                printf("H_LIM EVT\n");
                if ( DEBUG )
                    ESP_LOGI( TAG, "H_LIM EVT\n" );
            }
            if (evt.status & PCNT_STATUS_ZERO_M)
            {
                multPulses++;
                printf("ZERO EVT\n");
                if( DEBUG )
                    ESP_LOGI( TAG, "ZERO EVT\n" );
            }
        }
        else
        {
           pcnt_get_counter_value(PCNT_TEST_UNIT, &count);

            counterPulses = (multPulses * PCNT_H_LIM_VAL + count) / 458;
            //sprintf( str, "{\"value\": :%lu}", counterPulses );
            //esp_mqtt_client_publish( client, TOPICO_FLOW, str, strlen( str ), 0, 0 );

            if (counterPulses != aux)
            {
                sprintf( str, "mqtt_consumer,unity=223D consumer=%lu\n", counterPulses);
                esp_mqtt_client_publish( client, TOPICO_FLOW, str, strlen( str ), 0, 0 );
                aux = counterPulses;
                //pcnt_counter_clear(PCNT_TEST_UNIT);
                printf("\n%lu litros\n", counterPulses);
            }
        }
    }
    if (user_isr_handle)
    {
        //Free the ISR service handle.
        esp_intr_free(user_isr_handle);
        user_isr_handle = NULL;
    }
}

void app_main( void )
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

	wifi_event_group = xEventGroupCreate();
	mqtt_event_group = xEventGroupCreate();

	wifi_manager_start();

	/* Registra as funções de callback do WiFi */
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &connection_on);
    wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, &connection_off);

    /**
     * Aguarda a conexão WiFi do ESP32 com o roteador;
     */
    xEventGroupWaitBits( wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY );

    mqtt_app_start();

	if( xTaskCreate( task_flow, "task_flow", 4098, NULL, 2, NULL )  != pdTRUE )
    {
        if( DEBUG )
            ESP_LOGI( TAG, "error - Nao foi possivel alocar task_flow.\r\n" );

        return;
    }
}

#include "buttons_handler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hue.h"
#include "ssdp.h"
#include "webserver.h"

// Suppress deprecation warning for legacy touch pad API on ESP32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"

#if TOUCH_ENABLED
    #if CONFIG_IDF_TARGET_ESP32
        #include "driver/touch_pad.h"

    #elif CONFIG_IDF_TARGET_ESP32S3
        #include "driver/touch_sens.h"
        #include "esp_err.h"

    #endif
#endif

#pragma GCC diagnostic pop

static const char *TAG = "INPUT_HANDLER";
static QueueHandle_t gpio_evt_queue = NULL;

// Definição da quantidade de entradas baseada no modo escolhido
#if SINGLE_INPUT
    #define NUM_INPUTS 1
    static const uint32_t input_channels[NUM_INPUTS] = { INPUT_PIN };
#else
    #define NUM_INPUTS 4
    static const uint32_t input_channels[NUM_INPUTS] = { INPUT_PIN, INPUT_PIN2, INPUT_PIN3, INPUT_PIN4 };
#endif


#if TOUCH_ENABLED
    static uint16_t touch_thresholds[NUM_INPUTS] = { 0 };

    #if CONFIG_IDF_TARGET_ESP32S3
        static touch_channel_handle_t touch_handles[NUM_INPUTS] = { NULL };
    #endif
#endif


// --- SEÇÃO DE INTERRUPÇÕES (ISR) ---

#if TOUCH_ENABLED
    static void IRAM_ATTR touch_isr_handler(void *arg)
    {
        uint32_t pad_num = (uint32_t)arg;
        xQueueSendFromISR(gpio_evt_queue, &pad_num, NULL);
    }
#else
    static void IRAM_ATTR gpio_isr_handler(void *arg)
    {
        uint32_t gpio_num = (uint32_t)arg;
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    }
#endif

// Executa a ação baseada no índice do botão que disparou o evento
// --- FUNÇÃO AUXILIAR DE EXECUÇÃO ---

static void execute_button_action(int index, uint64_t duration)
{
    #if DEVICE_TYPE == VENTILADOR

        // Lógica Híbrida para 4 botões: 
        // Se for o primeiro botão (index 0), checa os tempos longos primeiro.
        if (index == 0 && duration >= 10000)
        {
            ESP_LOGI(TAG, "Botão 1 segurado por 10s -> Resetando...");
            reset();
        }
        else if (index == 0 && duration >= 3000)
        {
            ESP_LOGI(TAG, "Botão 1 segurado por 3s -> Alternando SSDP...");
            if (ssdp_running) ssdp_stop();
            else ssdp_start();
        }
        // Se não for um pressionamento longo do Botão 1, trata como toque normal (debounce)
        else if (duration > 80) 
        {
            switch (index)
            {
                case 0:
                    #if SINGLE_INPUT
                        ESP_LOGI(TAG, "Botão único: toggle_state()");
                        toggle_state();
                    #else
                        ESP_LOGI(TAG, "Botão 1: set_state(false, 0)");
                        set_state(false, 0);
                    #endif
                    break;
                case 1:
                    ESP_LOGI(TAG, "Botão 2 (Toque rápido): set_state(true, 80)");
                    set_state(true, 80);
                    break;
                case 2:
                    ESP_LOGI(TAG, "Botão 3: set_state(true, 160)");
                    set_state(true, 160);
                    break;
                case 3:
                    ESP_LOGI(TAG, "Botão 4: set_state(true, 240)");
                    set_state(true, 240);
                    break;
                
                default:
                    break;
            }
        }
    #else
            // Mantém exatamente a sua lógica original para o botão único
        if (duration >= 10000)
        {
            reset();
        }
        else if (duration >= 3000)
        {
            if (ssdp_running) ssdp_stop();
            else ssdp_start();
        }
        else if (duration > 100)
        {
            toggle_state();
        }
    #endif
}


// --- TASK DE TRATAMENTO ---

static void button_task(void *arg)
{
    uint32_t io_num;
    
#if TOUCH_ENABLED
    // Calibração dinâmica para todos os canais ativos
    for (int i = 0; i < NUM_INPUTS; i++)
    {
        uint16_t touch_value;
        touch_pad_read(input_channels[i], &touch_value);
        touch_thresholds[i] = (touch_value * 2) / 3;
        touch_pad_config(input_channels[i], touch_thresholds[i]);
        ESP_LOGI(TAG, "Touch CH %lu Inicializado. Base: %d, Threshold: %d", input_channels[i], touch_value, touch_thresholds[i]);
    }
#endif

    while (1)
    {
        // Fica bloqueado aqui até receber o evento de TOQUE inicial (vindo da ISR)
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            int idx = -1;
            for (int i = 0; i < NUM_INPUTS; i++)
            {
                if (input_channels[i] == io_num)
                {
                    idx = i;
                    break;
                }
            }

            if (idx == -1) continue;

            // Se a task acordou mas o botão já consta como processado, limpa ruído
            bool is_pressed = false;
#if TOUCH_ENABLED
            uint16_t val;
            touch_pad_read(input_channels[idx], &val);
            is_pressed = (val < touch_thresholds[idx]);
#else
            is_pressed = (gpio_get_level(input_channels[idx]) == 0);
#endif

            if (is_pressed)
            {
                uint64_t start_time = esp_timer_get_time() / 1000ULL;
                ESP_LOGD(TAG, "Input %d pressionado...", idx + 1);

                // --- LAÇO DE MONITORAMENTO DO RELEASE ---
                // Fica aqui preso monitorando ativamente enquanto o botão estiver pressionado
                while (is_pressed)
                {
                    vTaskDelay(pdMS_TO_TICKS(40)); // Polling controlado de 40ms

#if TOUCH_ENABLED
                    touch_pad_read(input_channels[idx], &val);
                    is_pressed = (val < touch_thresholds[idx]);
#else
                    is_pressed = (gpio_get_level(input_channels[idx]) == 0);
#endif
                }

                // Saiu do laço? Significa que o usuário SOLTOU o botão
                uint64_t end_time = esp_timer_get_time() / 1000ULL;
                uint64_t duration = end_time - start_time;

                ESP_LOGI(TAG, "Input %d solto. Duração: %llu ms", idx + 1, duration);

                // Executa a ação imediatamente no momento em que solta
                execute_button_action(idx, duration);

                // Esvazia qualquer evento residual idêntico acumulado na fila para evitar repetição por ruído
                xQueueReset(gpio_evt_queue);
            }
        }
    }
}


// --- INICIALIZAÇÃO ---

void button_init(void)
{
    gpio_evt_queue = xQueueCreate(15, sizeof(uint32_t));

#if TOUCH_ENABLED
    ESP_LOGI(TAG, "Initializing Touch Pads (Mode: %s)", SINGLE_INPUT ? "SINGLE" : "MULTIPLE");
    touch_pad_init();
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);

    // Inicializa todos os canais definidos no array
    for (int i = 0; i < NUM_INPUTS; i++)
    {
        touch_pad_config(input_channels[i], 0);
        touch_pad_isr_register(touch_isr_handler, (void *)input_channels[i]);
    }
    touch_pad_intr_enable();
#else
    ESP_LOGI(TAG, "Initializing Physical Button GPIO %d", INPUT_PIN);
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << INPUT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(INPUT_PIN, gpio_isr_handler, (void *)INPUT_PIN);
#endif

    xTaskCreate(button_task, "button_task", 4096, NULL, 10, NULL);
}
#include "buttons_handler.h"

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "hue.h"
#include "ssdp.h"
#include "webserver.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"

#if TOUCH_ENABLED

    #if CONFIG_IDF_TARGET_ESP32
        //API legacy usada no ESP32 clássico.
        #include "driver/touch_pad.h"
    #elif CONFIG_IDF_TARGET_ESP32S3
        // API nova obrigatória para o ESP32-S3 no ESP-IDF 6.
        #include "driver/touch_sens.h"
        #include "soc/soc_caps.h"
    #else
        #error "Touch não implementado para este target"
    #endif

#endif

#pragma GCC diagnostic pop

static const char *TAG = "INPUT_HANDLER";
static QueueHandle_t input_event_queue = NULL;

/* ================================================================
 * ENTRADAS
 * ================================================================ */

#if SINGLE_INPUT

    #define NUM_INPUTS 1

    static const uint32_t input_channels[NUM_INPUTS] = {
        INPUT_PIN
    };

#else

    #define NUM_INPUTS 4

    static const uint32_t input_channels[NUM_INPUTS] = {
        INPUT_PIN,
        INPUT_PIN2,
        INPUT_PIN3,
        INPUT_PIN4
    };

#endif


/* ================================================================
 * CONFIGURAÇÕES GERAIS
 * ================================================================ */

#define MINIMUM_SHORT_PRESS_MS        60ULL
#define LONG_PRESS_SSDP_MS          3000ULL
#define LONG_PRESS_RESET_MS        10000ULL

#define GPIO_DEBOUNCE_INTERVAL_MS      10
#define GPIO_CONFIRM_SAMPLES            2
#define GPIO_RELEASE_SAMPLES            3


/* ================================================================
 * ESTRUTURA DE EVENTO
 * ================================================================ */

typedef enum
{
    INPUT_EVENT_PRESSED = 0,
    INPUT_EVENT_RELEASED
} input_event_type_t;


typedef struct
{
    uint32_t input;
    input_event_type_t type;
    uint64_t timestamp_ms;
} input_event_t;


/* ================================================================
 * ESTADO DE CADA ENTRADA
 * ================================================================ */

typedef struct
{
    bool pressed;
    uint64_t pressed_at_ms;
} input_state_t;

static input_state_t input_states[NUM_INPUTS] = { 0 };

/* ================================================================
 * CONFIGURAÇÕES DO ESP32 CLÁSSICO
 * ================================================================ */

#if TOUCH_ENABLED && CONFIG_IDF_TARGET_ESP32

    #define ESP32_TOUCH_CALIBRATION_SAMPLES      20
    #define ESP32_TOUCH_CALIBRATION_INTERVAL_MS  20

    #define ESP32_TOUCH_PRESS_PERCENT            70
    #define ESP32_TOUCH_RELEASE_PERCENT          85

    #define ESP32_TOUCH_POLL_INTERVAL_MS         20
    #define ESP32_TOUCH_PRESS_CONFIRM_SAMPLES     2
    #define ESP32_TOUCH_RELEASE_CONFIRM_SAMPLES   3

    static uint16_t esp32_touch_baselines[NUM_INPUTS] = { 0 };
    static uint16_t esp32_touch_press_thresholds[NUM_INPUTS] = { 0 };
    static uint16_t esp32_touch_release_thresholds[NUM_INPUTS] = { 0 };

#endif


/* ================================================================
 * CONFIGURAÇÕES DO ESP32-S3
 * ================================================================ */

#if TOUCH_ENABLED && CONFIG_IDF_TARGET_ESP32S3

    //O ESP32-S3 possui apenas uma configuração de amostragem.
    #define S3_TOUCH_SAMPLE_CONFIG_COUNT 1

    //Número de varreduras realizadas para estabilizar o benchmark.
    #define S3_TOUCH_INITIAL_SCAN_COUNT 30

    /*
     * Threshold relativo ao benchmark.
     *
     * 0.08 significa 8%.
     *
     * Se estiver sensível demais, aumente.
     * Se estiver pouco sensível, diminua.
     */
    #define S3_TOUCH_THRESHOLD_RATIO 0.08f

    /*
     * Threshold inicial temporário.
     * Depois da calibração ele é recalculado para cada canal.
     */
    #define S3_TOUCH_INITIAL_THRESHOLD 2000U

    static touch_sensor_handle_t s3_touch_sensor = NULL;

    static touch_channel_handle_t
        s3_touch_channel_handles[NUM_INPUTS] = { NULL };

#endif


/* ================================================================
 * FUNÇÕES AUXILIARES
 * ================================================================ */

static uint64_t get_time_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}


static int get_input_index(uint32_t input)
{
    for (int i = 0; i < NUM_INPUTS; i++)
    {
        if (input_channels[i] == input)
            return i;
    }
    return -1;
}


/* ================================================================
 * AÇÕES DOS BOTÕES
 * ================================================================ */

static void execute_button_action(int index, uint64_t duration)
{
#if DEVICE_TYPE == VENTILADOR

    if (index == 0 && duration >= LONG_PRESS_RESET_MS)
    {
        ESP_LOGI(
            TAG,
            "Botão 1 pressionado por %" PRIu64
            " ms: resetando dispositivo",
            duration
        );

        reset();
    }
    else if (index == 0 && duration >= LONG_PRESS_SSDP_MS)
    {
        ESP_LOGI(
            TAG,
            "Botão 1 pressionado por %" PRIu64
            " ms: alternando SSDP",
            duration
        );

        if (ssdp_running)
            ssdp_stop();
        else
            ssdp_start();
    }

    else if (duration >= MINIMUM_SHORT_PRESS_MS)
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

                ESP_LOGI(TAG, "Botão 2: set_state(true, 80)");
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

                ESP_LOGW(TAG, "Índice de botão inválido: %d",index);
                break;
        }
    }
    else
    {
        ESP_LOGD(
            TAG,
            "Toque curto descartado no botão %d: %" PRIu64 " ms",
            index + 1,
            duration
        );
    }

#else

    if (duration >= LONG_PRESS_RESET_MS)
    {
        ESP_LOGI(
            TAG,
            "Botão pressionado por %" PRIu64
            " ms: resetando",
            duration
        );

        reset();
    }
    else if (duration >= LONG_PRESS_SSDP_MS)
    {
        ESP_LOGI(
            TAG,
            "Botão pressionado por %" PRIu64
            " ms: alternando SSDP",
            duration
        );

        if (ssdp_running)
            ssdp_stop();
        
        else
            ssdp_start();
    }
    else if (duration >= MINIMUM_SHORT_PRESS_MS)
    {
        ESP_LOGI(TAG,"Toque rápido: toggle_state()");
        toggle_state();
    }
    else
    {
        ESP_LOGD(
            TAG,
            "Toque curto descartado: %" PRIu64 " ms",
            duration
        );
    }

#endif
}


/* ================================================================
 * PROCESSAMENTO DE EVENTOS
 * ================================================================ */

static void process_input_event(
    const input_event_t *event
)
{
    int index = get_input_index(event->input);

    if (index < 0)
    {
        ESP_LOGW(
            TAG,
            "Evento recebido para entrada desconhecida: %" PRIu32,
            event->input
        );

        return;
    }

    if (event->type == INPUT_EVENT_PRESSED)
    {
        /*
         * Ignora eventos de pressionamento duplicados.
         */
        if (input_states[index].pressed)
        {
            ESP_LOGD(
                TAG,
                "Pressionamento duplicado ignorado no input %d",
                index + 1
            );

            return;
        }

        input_states[index].pressed = true;
        input_states[index].pressed_at_ms = event->timestamp_ms;

        ESP_LOGI(
            TAG,
            "Input %d pressionado",
            index + 1
        );

        return;
    }

    // Ignora evento de soltura se não houver pressionamento ativo.
    if (!input_states[index].pressed)
    {
        ESP_LOGD(
            TAG,
            "Soltura sem pressionamento ignorada no input %d",
            index + 1
        );

        return;
    }

    uint64_t duration =
        event->timestamp_ms -
        input_states[index].pressed_at_ms;

    input_states[index].pressed = false;
    input_states[index].pressed_at_ms = 0;

    ESP_LOGI(
        TAG,
        "Input %d solto. Duração: %" PRIu64 " ms",
        index + 1,
        duration
    );

    execute_button_action(index, duration);
}


/* ================================================================
 * ESP32 CLÁSSICO
 * ================================================================ */

#if TOUCH_ENABLED && CONFIG_IDF_TARGET_ESP32

static void IRAM_ATTR esp32_touch_isr_handler(void *arg)
{
    uint32_t touch_status;
    BaseType_t task_woken = pdFALSE;

    (void)arg;

    touch_status = touch_pad_get_status();

    // Limpa imediatamente a interrupção pendente.
    touch_pad_clear_status();

    for (int i = 0; i < NUM_INPUTS; i++)
    {
        uint32_t channel = input_channels[i];

        if ((touch_status & (1UL << channel)) == 0)
            continue;
        

        input_event_t event = {
            .input = channel,
            .type = INPUT_EVENT_PRESSED,
            .timestamp_ms = get_time_ms()
        };

        xQueueSendFromISR(
            input_event_queue,
            &event,
            &task_woken
        );
    }

    if (task_woken == pdTRUE)
        portYIELD_FROM_ISR();
    
}


static esp_err_t esp32_calibrate_touch(void)
{
    ESP_LOGI(TAG, "Calibrando touch do ESP32 clássico");

    ESP_LOGI(TAG, "Não toque nos terminais durante a calibração");

    vTaskDelay(pdMS_TO_TICKS(500));

    for (int i = 0; i < NUM_INPUTS; i++)
    {
        uint32_t accumulator = 0;

        for (int sample = 0; sample < ESP32_TOUCH_CALIBRATION_SAMPLES; sample++)
        {
            uint16_t value = 0;

            esp_err_t result = touch_pad_read(
                (touch_pad_t)input_channels[i],
                &value
            );

            if (result != ESP_OK)
                return result;
            
            accumulator += value;

            vTaskDelay(
                pdMS_TO_TICKS(
                    ESP32_TOUCH_CALIBRATION_INTERVAL_MS
                )
            );
        }

        uint16_t baseline =
            accumulator /
            ESP32_TOUCH_CALIBRATION_SAMPLES;

        if (baseline == 0)
            return ESP_FAIL;

        esp32_touch_baselines[i] = baseline;

        esp32_touch_press_thresholds[i] =
            (uint16_t)(
                ((uint32_t)baseline *
                 ESP32_TOUCH_PRESS_PERCENT) /
                100U
            );

        esp32_touch_release_thresholds[i] =
            (uint16_t)(
                ((uint32_t)baseline *
                 ESP32_TOUCH_RELEASE_PERCENT) /
                100U
            );

        ESP_ERROR_CHECK(
            touch_pad_config(
                (touch_pad_t)input_channels[i],
                esp32_touch_press_thresholds[i]
            )
        );

        ESP_LOGI(
            TAG,
            "ESP32 CH %" PRIu32
            ": base=%u, press=%u, release=%u",
            input_channels[i],
            esp32_touch_baselines[i],
            esp32_touch_press_thresholds[i],
            esp32_touch_release_thresholds[i]
        );
    }

    touch_pad_clear_status();

    return ESP_OK;
}


static bool esp32_confirm_touch_pressed(int index)
{
    int consecutive_samples = 0;

    for (int sample = 0; sample < ESP32_TOUCH_PRESS_CONFIRM_SAMPLES + 2; sample++)
    {
        uint16_t value = 0;

        if (
            touch_pad_read(
                (touch_pad_t)input_channels[index],
                &value
            ) != ESP_OK
        )
        {
            return false;
        }

        if (value < esp32_touch_press_thresholds[index])
        {
            consecutive_samples++;

            if (consecutive_samples >= ESP32_TOUCH_PRESS_CONFIRM_SAMPLES)
                return true;
            
        }
        else
        {
            consecutive_samples = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(ESP32_TOUCH_POLL_INTERVAL_MS));
    }

    return false;
}


static void esp32_wait_touch_release(int index)
{
    int release_samples = 0;

    while (
        release_samples <
        ESP32_TOUCH_RELEASE_CONFIRM_SAMPLES
    )
    {
        uint16_t value = 0;

        vTaskDelay(pdMS_TO_TICKS(ESP32_TOUCH_POLL_INTERVAL_MS));

        if (
            touch_pad_read(
                (touch_pad_t)input_channels[index],
                &value
            ) != ESP_OK
        )
        {
            ESP_LOGE(
                TAG,
                "Falha ao ler touch ESP32 no input %d",
                index + 1
            );

            return;
        }

        ESP_LOGD(
            TAG,
            "ESP32 CH %" PRIu32
            ": value=%u, press=%u, release=%u",
            input_channels[index],
            value,
            esp32_touch_press_thresholds[index],
            esp32_touch_release_thresholds[index]
        );

        if (value >esp32_touch_release_thresholds[index])
            release_samples++;

        else
            release_samples = 0;
    }
}


static void esp32_touch_task(void *arg)
{
    input_event_t event;

    (void)arg;

    while (1)
    {
        if (xQueueReceive(input_event_queue, &event, portMAX_DELAY) != pdTRUE)
            continue;

        int index = get_input_index(event.input);

        if (index < 0)
            continue;
        

        if (!esp32_confirm_touch_pressed(index))
        {
            ESP_LOGD(
                TAG,
                "Ruído descartado no input %d",
                index + 1
            );

            continue;
        }

        event.type = INPUT_EVENT_PRESSED;
        event.timestamp_ms = get_time_ms();

        process_input_event(&event);

        esp32_wait_touch_release(index);

        event.type = INPUT_EVENT_RELEASED;
        event.timestamp_ms = get_time_ms();

        process_input_event(&event);

        touch_pad_clear_status();
    }
}

#endif


/* ================================================================
 * ESP32-S3
 * ================================================================ */

#if TOUCH_ENABLED && CONFIG_IDF_TARGET_ESP32S3

/*
 * Callback executado quando um canal é pressionado.
 *
 * Esse callback é executado no contexto da interrupção.
 * Portanto, ele apenas coloca o evento na fila.
 */
static bool IRAM_ATTR s3_touch_active_callback(
    touch_sensor_handle_t sensor_handle,
    const touch_active_event_data_t *event_data,
    void *user_context
)
{
    BaseType_t task_woken = pdFALSE;

    (void)sensor_handle;
    (void)user_context;

    input_event_t event = {
        .input = (uint32_t)event_data->chan_id,
        .type = INPUT_EVENT_PRESSED,
        .timestamp_ms = get_time_ms()
    };

    if (input_event_queue != NULL)
    {
        xQueueSendFromISR(
            input_event_queue,
            &event,
            &task_woken
        );
    }

    return task_woken == pdTRUE;
}


/*
 * Callback executado quando o canal é solto.
 */
static bool IRAM_ATTR s3_touch_inactive_callback(
    touch_sensor_handle_t sensor_handle,
    const touch_inactive_event_data_t *event_data,
    void *user_context
)
{
    BaseType_t task_woken = pdFALSE;

    (void)sensor_handle;
    (void)user_context;

    input_event_t event = {
        .input = (uint32_t)event_data->chan_id,
        .type = INPUT_EVENT_RELEASED,
        .timestamp_ms = get_time_ms()
    };

    if (input_event_queue != NULL)
    {
        xQueueSendFromISR(
            input_event_queue,
            &event,
            &task_woken
        );
    }

    return task_woken == pdTRUE;
}


static void s3_input_task(void *arg)
{
    input_event_t event;

    (void)arg;

    ESP_LOGI(
        TAG,
        "Task de eventos touch do ESP32-S3 iniciada"
    );

    while (1)
    {
        if (xQueueReceive(input_event_queue, &event, portMAX_DELAY) == pdTRUE)
            process_input_event(&event);
        
    }
}


static esp_err_t s3_calibrate_touch_channels(void)
{
    ESP_LOGI(TAG, "Iniciando calibração do ESP32-S3");

    ESP_LOGI(TAG, "Não toque nos terminais durante a calibração");

    /*
     * Habilita temporariamente o sensor para inicializar
     * smooth data e benchmark.
     */
    ESP_RETURN_ON_ERROR(
        touch_sensor_enable(s3_touch_sensor),
        TAG,
        "Falha ao habilitar touch para calibração"
    );

    /*
     * Realiza várias varreduras iniciais.
     */
    for (int scan = 0; scan < S3_TOUCH_INITIAL_SCAN_COUNT; scan++)
    {
        ESP_RETURN_ON_ERROR(
            touch_sensor_trigger_oneshot_scanning(
                s3_touch_sensor,
                2000
            ),
            TAG,
            "Falha na varredura inicial do touch"
        );

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    ESP_RETURN_ON_ERROR(
        touch_sensor_disable(s3_touch_sensor),
        TAG,
        "Falha ao desabilitar touch após calibração"
    );

    /*
     * Agora que existe um benchmark válido, calcula o threshold
     * relativo de cada canal.
     */
    for (int i = 0; i < NUM_INPUTS; i++)
    {
        uint32_t benchmark[S3_TOUCH_SAMPLE_CONFIG_COUNT] = { 0 };

        ESP_RETURN_ON_ERROR(
            touch_channel_read_data(
                s3_touch_channel_handles[i],
                TOUCH_CHAN_DATA_TYPE_BENCHMARK,
                benchmark
            ),
            TAG,
            "Falha ao ler benchmark do canal %d",
            i
        );

        uint32_t calculated_threshold =
            (uint32_t)(
                (float)benchmark[0] *
                S3_TOUCH_THRESHOLD_RATIO
            );

        if (calculated_threshold == 0)
        {
            ESP_LOGE(
                TAG,
                "Threshold inválido para o S3 CH %" PRIu32
                ". Benchmark=%" PRIu32,
                input_channels[i],
                benchmark[0]
            );

            return ESP_FAIL;
        }

        /*
         * No ESP32-S3, o threshold é relativo ao benchmark.
         *
         * O canal fica ativo quando:
         *
         * smooth_data >= benchmark + active_thresh
         */
        touch_channel_config_t channel_config = {
            .active_thresh = {
                calculated_threshold
            },
            .charge_speed = TOUCH_CHARGE_SPEED_7,
            .init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT
        };

        ESP_RETURN_ON_ERROR(
            touch_sensor_reconfig_channel(
                s3_touch_channel_handles[i],
                &channel_config
            ),
            TAG,
            "Falha ao reconfigurar canal touch do S3"
        );

        touch_chan_info_t channel_info = { 0 };

        ESP_RETURN_ON_ERROR(
            touch_sensor_get_channel_info(
                s3_touch_channel_handles[i],
                &channel_info
            ),
            TAG,
            "Falha ao obter informações do canal touch"
        );

        ESP_LOGI(
            TAG,
            "S3 CH %" PRIu32
            "/GPIO%d: benchmark=%" PRIu32
            ", threshold=%" PRIu32,
            input_channels[i],
            channel_info.chan_gpio,
            benchmark[0],
            calculated_threshold
        );
    }

    return ESP_OK;
}


static esp_err_t s3_touch_init(void)
{
    ESP_LOGI(
        TAG,
        "Inicializando touch do ESP32-S3"
    );

    /*
     * Configuração padrão para touch hardware V2.
     *
     * ESP32-S3 usa touch V2 e possui apenas uma configuração
     * de amostragem.
     */
    touch_sensor_sample_config_t sample_config[
        S3_TOUCH_SAMPLE_CONFIG_COUNT
    ] = {
        TOUCH_SENSOR_V2_DEFAULT_SAMPLE_CONFIG(
            1000,
            TOUCH_VOLT_LIM_L_0V5,
            TOUCH_VOLT_LIM_H_2V2
        )
    };

    touch_sensor_config_t sensor_config =
        TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(
            S3_TOUCH_SAMPLE_CONFIG_COUNT,
            sample_config
        );

    ESP_RETURN_ON_ERROR(
        touch_sensor_new_controller(
            &sensor_config,
            &s3_touch_sensor
        ),
        TAG,
        "Falha ao criar controlador touch do S3"
    );

    /*
     * Threshold temporário.
     *
     * O valor definitivo será calculado após as primeiras
     * varreduras.
     */
    touch_channel_config_t channel_config = {
        .active_thresh = {
            S3_TOUCH_INITIAL_THRESHOLD
        },
        .charge_speed = TOUCH_CHARGE_SPEED_7,
        .init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT
    };

    for (int i = 0; i < NUM_INPUTS; i++)
    {
        /*
         * No ESP32-S3, INPUT_PIN 4 significa canal CH4,
         * correspondente ao GPIO4.
         */
        ESP_RETURN_ON_ERROR(
            touch_sensor_new_channel(
                s3_touch_sensor,
                (int)input_channels[i],
                &channel_config,
                &s3_touch_channel_handles[i]
            ),
            TAG,
            "Falha ao criar canal touch %" PRIu32,
            input_channels[i]
        );

        touch_chan_info_t channel_info = { 0 };

        ESP_RETURN_ON_ERROR(
            touch_sensor_get_channel_info(
                s3_touch_channel_handles[i],
                &channel_info
            ),
            TAG,
            "Falha ao obter GPIO do canal touch"
        );

        ESP_LOGI(
            TAG,
            "Touch S3 CH %" PRIu32 " configurado no GPIO%d",
            input_channels[i],
            channel_info.chan_gpio
        );
    }

    /*
     * Ativa o filtro padrão do driver.
     *
     * No ESP32-S3 o filtro é implementado em hardware.
     */
    touch_sensor_filter_config_t filter_config =
        TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();

    ESP_RETURN_ON_ERROR(
        touch_sensor_config_filter(
            s3_touch_sensor,
            &filter_config
        ),
        TAG,
        "Falha ao configurar filtro touch do S3"
    );

    /*
     * Executa calibração antes de registrar os callbacks.
     */
    ESP_RETURN_ON_ERROR(
        s3_calibrate_touch_channels(),
        TAG,
        "Falha na calibração touch do S3"
    );

    touch_event_callbacks_t callbacks = {
        .on_active = s3_touch_active_callback,
        .on_inactive = s3_touch_inactive_callback
    };

    ESP_RETURN_ON_ERROR(
        touch_sensor_register_callbacks(
            s3_touch_sensor,
            &callbacks,
            NULL
        ),
        TAG,
        "Falha ao registrar callbacks touch do S3"
    );

    ESP_RETURN_ON_ERROR(
        touch_sensor_enable(s3_touch_sensor),
        TAG,
        "Falha ao habilitar touch do S3"
    );

    ESP_RETURN_ON_ERROR(
        touch_sensor_start_continuous_scanning(
            s3_touch_sensor
        ),
        TAG,
        "Falha ao iniciar varredura contínua do S3"
    );

    ESP_LOGI(
        TAG,
        "Touch do ESP32-S3 inicializado com sucesso"
    );

    return ESP_OK;
}

#endif


/* ================================================================
 * BOTÕES GPIO
 * ================================================================ */

#if !TOUCH_ENABLED

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)(uintptr_t)arg;

    BaseType_t task_woken = pdFALSE;

    input_event_t event = {
        .input = gpio_num,
        .type = INPUT_EVENT_PRESSED,
        .timestamp_ms = get_time_ms()
    };

    if (input_event_queue != NULL)
    {
        xQueueSendFromISR(
            input_event_queue,
            &event,
            &task_woken
        );
    }

    if (task_woken == pdTRUE)
        portYIELD_FROM_ISR();
    
}


static bool confirm_gpio_pressed(int index)
{
    int consecutive_samples = 0;

    for (int sample = 0; sample < 4; sample++)
    {
        if (gpio_get_level(input_channels[index]) == 0)
        {
            consecutive_samples++;

            if (consecutive_samples >= GPIO_CONFIRM_SAMPLES)
                return true;
        }
        else
        {
            consecutive_samples = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(GPIO_DEBOUNCE_INTERVAL_MS));
    }

    return false;
}


static void wait_gpio_release(int index)
{
    int release_samples = 0;

    while (release_samples < GPIO_RELEASE_SAMPLES)
    {
        vTaskDelay(pdMS_TO_TICKS(GPIO_DEBOUNCE_INTERVAL_MS));

        if (gpio_get_level(input_channels[index]) != 0)
            release_samples++;
        else
            release_samples = 0;
    }
}


static void gpio_input_task(void *arg)
{
    input_event_t event;

    (void)arg;

    while (1)
    {
        if (xQueueReceive(input_event_queue, &event, portMAX_DELAY) != pdTRUE)
            continue;

        int index = get_input_index(event.input);

        if (index < 0)
            continue;

        if (!confirm_gpio_pressed(index))
            continue;

        event.type = INPUT_EVENT_PRESSED;
        event.timestamp_ms = get_time_ms();

        process_input_event(&event);

        wait_gpio_release(index);

        event.type = INPUT_EVENT_RELEASED;
        event.timestamp_ms = get_time_ms();

        process_input_event(&event);
    }
}

#endif


/* ================================================================
 * INICIALIZAÇÃO PRINCIPAL
 * ================================================================ */

void button_init(void)
{
    input_event_queue = xQueueCreate(
        20,
        sizeof(input_event_t)
    );

    if (input_event_queue == NULL)
    {
        ESP_LOGE(TAG, "Falha ao criar fila de eventos");

        return;
    }


#if TOUCH_ENABLED && CONFIG_IDF_TARGET_ESP32

    ESP_LOGI(
        TAG,
        "Inicializando ESP32 clássico em modo %s",
        SINGLE_INPUT ? "SINGLE" : "MULTIPLE"
    );

    ESP_ERROR_CHECK(
        touch_pad_init()
    );

    ESP_ERROR_CHECK(
        touch_pad_set_fsm_mode(
            TOUCH_FSM_MODE_TIMER
        )
    );

    for (int i = 0; i < NUM_INPUTS; i++)
    {
        ESP_ERROR_CHECK(
            touch_pad_config(
                (touch_pad_t)input_channels[i],
                0
            )
        );
    }

    ESP_ERROR_CHECK(
        esp32_calibrate_touch()
    );

    ESP_ERROR_CHECK(
        touch_pad_isr_register(
            esp32_touch_isr_handler,
            NULL
        )
    );

    touch_pad_clear_status();

    ESP_ERROR_CHECK(
        touch_pad_intr_enable()
    );

    if (
        xTaskCreate(
            esp32_touch_task,
            "esp32_touch_task",
            4096,
            NULL,
            10,
            NULL
        ) != pdPASS
    )
    {
        ESP_LOGE(
            TAG,
            "Falha ao criar task touch do ESP32"
        );

        return;
    }


#elif TOUCH_ENABLED && CONFIG_IDF_TARGET_ESP32S3

    ESP_LOGI(
        TAG,
        "Inicializando ESP32-S3 em modo %s",
        SINGLE_INPUT ? "SINGLE" : "MULTIPLE"
    );

    ESP_ERROR_CHECK(
        s3_touch_init()
    );

    if (
        xTaskCreate(
            s3_input_task,
            "s3_touch_task",
            4096,
            NULL,
            10,
            NULL
        ) != pdPASS
    )
    {
        ESP_LOGE(
            TAG,
            "Falha ao criar task touch do ESP32-S3"
        );

        return;
    }


#else

    ESP_LOGI(
        TAG,
        "Inicializando botões GPIO em modo %s",
        SINGLE_INPUT ? "SINGLE" : "MULTIPLE"
    );

    uint64_t pin_mask = 0;

    for (int i = 0; i < NUM_INPUTS; i++)
    {
        pin_mask |= (
            1ULL << input_channels[i]
        );
    }

    gpio_config_t io_config = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };

    ESP_ERROR_CHECK(
        gpio_config(&io_config)
    );

    esp_err_t isr_result =
        gpio_install_isr_service(0);

    if (
        (isr_result != ESP_OK) &&
        (isr_result != ESP_ERR_INVALID_STATE)
    )
    {
        ESP_LOGE(
            TAG,
            "Falha ao instalar ISR GPIO: %s",
            esp_err_to_name(isr_result)
        );

        return;
    }

    for (int i = 0; i < NUM_INPUTS; i++)
    {
        ESP_ERROR_CHECK(
            gpio_isr_handler_add(
                input_channels[i],
                gpio_isr_handler,
                (void *)(uintptr_t)input_channels[i]
            )
        );
    }

    if (
        xTaskCreate(
            gpio_input_task,
            "gpio_input_task",
            4096,
            NULL,
            10,
            NULL
        ) != pdPASS
    )
    {
        ESP_LOGE(TAG, "Falha ao criar task GPIO");
        return;
    }

#endif

    ESP_LOGI(TAG, "Inicialização dos botões concluída");
}
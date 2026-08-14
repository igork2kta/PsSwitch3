#include "hue.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "esp_system.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "persistent_data.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"


char    device_name[device_name_max_len] = "PSR Switch";
bool    state = false;
uint8_t bri = 1;
bool    timer_state = false;
uint8_t timer_bri;
uint8_t timer_bri = 0;
int     minutes = -1;
bool    start_state = false;
bool    ota_state = false;
uint8_t thermal_shutdown_count = 0;
uint32_t gpio_state_low = 0;
uint32_t gpio_state_high = 0;



static TimerHandle_t light_timer = NULL;

static const char *TAG = "hue";
#define NVS_NAMESPACE    "hue"
#define NVS_KEY_DEVICE   "device_name"


static void output_init(void);

void init(void)
{
    #if DEBUG_HUE 
        ESP_LOGI(TAG, "[HUE] Initializing variables...");
    #endif

    persistent_data_init();

    output_init();

    // Carrega device_name persistente
    //load_device_info_from_nvs();
    persistent_data_read_name(device_name);
    start_state = persistent_data_get_start_state();
    set_state(start_state, 10);

    #if TEMPERATURE_SENSOR
        temperature_init();

        xTaskCreate(
            temperature_task,
            "temperature_task",
            2048,
            NULL,
            5,
            NULL
        );

        

        gpio_set_direction(TEMPERATURE_LED_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(TEMPERATURE_LED_PIN, 0); // inicia desligado

    #endif

    #if DEBUG_HUE
        ESP_LOGI(TAG, "[HUE] Device name: %s | Start state: %d",
                device_name, start_state);
    #endif
    
}


static void output_init(void)
{
    uint64_t mask = 0;

    // Sempre inclui o pino 1
    mask |= (1ULL << OUTPUT_PIN_1);

    // Se for ventilador, adiciona mais pinos
    #if DEVICE_TYPE == VENTILADOR
        mask |= (1ULL << OUTPUT_PIN_2);
        mask |= (1ULL << OUTPUT_PIN_3);
    #endif

    gpio_config_t io_conf = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);

    // Inicializa os estados
    gpio_set_level(OUTPUT_PIN_1, !start_state);

    #if DEVICE_TYPE == VENTILADOR
        gpio_set_level(OUTPUT_PIN_2, !start_state);
        gpio_set_level(OUTPUT_PIN_3, !start_state);
    #endif
}


//============================= SETTERS =======================================//

void set_device_name(const char *name)
{
    if (!name || name[0] == '\0') {
        ESP_LOGW(TAG, "device_name inválido");
        return;
    }

    if (strncmp(name, device_name, device_name_max_len - 1) == 0) { 
        ESP_LOGI(TAG, "[HUE] Mesmo nome") ; 
        return;
    }

    strncpy(device_name, name, device_name_max_len - 1);
    device_name[device_name_max_len - 1] = '\0';

    persistent_data_write_name(device_name);

    ESP_LOGI(TAG, "Renomeado: %s", device_name);
}

void apply_outputs()
{

    //Desliga todos, tanto do banco alto quanto baixo
    GPIO.out_w1tc = GPIO_MASK_LOW;
    GPIO.out1_w1tc.val = GPIO_MASK_HIGH;

    //Liga os necessários, tanto no banco alto quanto baixo
    GPIO.out_w1ts = gpio_state_low;
    GPIO.out1_w1ts.val = gpio_state_high;

    /*
    W1TS: Write 1 to Set (Escreva 1 para Ligar / Nível Alto)
    W1TC: Write 1 to Clear (Escreva 1 para Desligar / Nível Baixo)

    Detalhe importante:
        GPIO.out_* → GPIO 0–31
        GPIO.out1_* → GPIO 32–39
    */
}

void set_state(bool on, uint8_t value)
{
    if (state == on && bri == value)
        return;

    state = on;
    bri = value;

#if DEVICE_TYPE == VENTILADOR

    if (!state) {
        gpio_state_low = 0;
        gpio_state_high = 0;
    }
    else if (value <= 85) {
        gpio_state_low = SPEED1_LOW;
        gpio_state_high = SPEED1_HIGH;
    }
    else if (value <= 170) {
        gpio_state_low = SPEED2_LOW;
        gpio_state_high = SPEED2_HIGH;
    }
    else {
        gpio_state_low = SPEED3_LOW;
        gpio_state_high = SPEED3_HIGH;
    }

#elif DEVICE_TYPE == PLUG

    gpio_state_low = state ? ON : 0;
    gpio_state_high = state ? ON : 0;

#endif

#if TEMPERATURE_SENSOR
    gpio_set_level(TEMPERATURE_LED_PIN,0);
#endif

    apply_outputs();

#if DEBUG_HUE
    ESP_LOGI(TAG, "STATE: %d | BRI: %d\n", state, bri);
#endif

}

void toggle_state(void)
{
    if(state == false)  //está desligado, seta para velocidade 1
            set_state(true, 80);

        else if(bri <= 85)  //Está no 1, seta para 2
            set_state(true, 160);
        
        
        else if(bri > 85 && bri <= 170)  //Está no 2, seta para 3
            set_state(true, 240);
        
        else if(bri > 170)  //Está no 3, seta para desligado
            set_state(false, 0);

        #if DEBUG_HUE
            ESP_LOGI(TAG, "STATE set to: %d | BRI set to : %d\n", state, bri);
        #endif
}


void set_start_state(bool new_start_state)
{
    if (start_state == new_start_state)
        return;

    start_state = new_start_state;

    persistent_data_set_start_state(start_state);

    #if DEBUG_HUE
        ESP_LOGI(TAG, "Start state atualizado: %d", start_state);
    #endif
}


void set_timer(bool new_state, uint8_t new_bri, uint16_t new_minutes){
    timer_state = new_state;
    timer_bri = new_bri;
    minutes = new_minutes;

    start_timer(minutes,  timer_state);

    #if DEBUG_HUE 
      if (minutes < 0)
        ESP_LOGI(TAG, "[HUE] Timer OFF");
      else
        ESP_LOGI(TAG, "[HUE] Timer: %s |  Bri: %d in %d min\n",
                timer_state ? "ON" : "OFF", timer_bri,  minutes);
    #endif

}

void set_ota_state(bool new_ota_state)
{
    if (ota_state == new_ota_state)
        return;

    ota_state = new_ota_state;


    #if DEBUG_HUE
        ESP_LOGI(TAG, "OTA STATE atualizado: %d", start_state);
    #endif
}



//============================= GETTERS =======================================//

const char *get_device_name(void){return device_name;}
bool get_state(void){return state;}
uint8_t get_bri(void){return bri;}
bool get_timer_state(void){return timer_state;}
int get_timer_minutes(void){return minutes;}
bool get_start_state(void){return start_state;}
bool get_ota_state(void){return ota_state;}
uint8_t get_thermal_shutdown_count(void){return thermal_shutdown_count;}

//============================= TIMER =======================================//


static void timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Timer expired. Setting state to: %d", timer_state);
    minutes = -1;
    set_state(timer_state, timer_bri);
}

void start_timer(int minutes, bool target_state)
{
    uint32_t period_ms = (int)minutes * 60000UL;

    if (light_timer == NULL) {
        light_timer = xTimerCreate(
            "LightTimer",
            pdMS_TO_TICKS(period_ms),
            pdFALSE,   // one-shot
            NULL,
            timer_callback
        );
    }

    if (light_timer != NULL) {
        xTimerStop(light_timer, 0);
        xTimerChangePeriod(light_timer,
                           pdMS_TO_TICKS(period_ms),
                           0);
        xTimerStart(light_timer, 0);

        ESP_LOGI(TAG, "Timer started: %d minutes -> state %d",
                 minutes, target_state);
    }
}

void stop_light_timer(void)
{
    if (light_timer != NULL) {
        xTimerStop(light_timer, 0);
        ESP_LOGI(TAG, "Timer stopped");
    }
}



//============================= END TIMER ===================================//

#if TEMPERATURE_SENSOR

static adc_oneshot_unit_handle_t adc_handle;
static adc_unit_t adc_unit;
static adc_channel_t adc_channel;

static adc_cali_handle_t adc_cali_handle = NULL;
static bool adc_calibrated = false;

void temperature_init(void)
{
    ESP_ERROR_CHECK(
        adc_oneshot_io_to_channel(
            SENSOR_PIN,
            &adc_unit,
            &adc_channel
        )
    );

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = adc_unit,
    };

    ESP_ERROR_CHECK(
        adc_oneshot_new_unit(
            &init_config,
            &adc_handle
        )
    );

    adc_oneshot_chan_cfg_t channel_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc_handle,
            adc_channel,
            &channel_config
        )
    );

#if CONFIG_IDF_TARGET_ESP32

    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = adc_unit,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

    if (adc_cali_create_scheme_line_fitting(
            &cali_config,
            &adc_cali_handle) == ESP_OK)
    {
        adc_calibrated = true;
    }

#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = adc_unit,
        .chan = adc_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

    if (adc_cali_create_scheme_curve_fitting(
            &cali_config,
            &adc_cali_handle) == ESP_OK)
    {
        adc_calibrated = true;
    }

#endif
}

void temperature_task(void *pvParameters)
{
    while (1)
    {
        float temperature = get_temperature();

        #if DEBUG_TEMPERATURE
            ESP_LOGI(TAG, "Temperature: %.2f°C", temperature);
        #endif

        if (temperature > MAX_TEMPERATURE)
        {
            ESP_LOGW(TAG, "Temperature exceeded limit: %.2f°C. Turning off the device.", temperature);
            set_state(false, 0);
            gpio_set_level(TEMPERATURE_LED_PIN, 1);
            thermal_shutdown_count++;
        }
        

        vTaskDelay(pdMS_TO_TICKS(5000)); // verifica a cada 5 segundos
    }
}



    #if SENSOR_TYPE == SENSOR_NTC_10K
        float get_temperature(void)
        {
            int64_t sum = 0;

            for (int i = 0; i < 16; i++)
            {
                int adc_raw;

                ESP_ERROR_CHECK(
                    adc_oneshot_read(
                        adc_handle,
                        adc_channel,
                        &adc_raw
                    )
                );

                sum += adc_raw;

                vTaskDelay(pdMS_TO_TICKS(5));
            }

            int adc_average = (int)(sum / 16);

            if (adc_average <= 0)
                return -999.0f;

            int voltage_mv;

            if (adc_calibrated)
            {
                ESP_ERROR_CHECK(
                    adc_cali_raw_to_voltage(
                        adc_cali_handle,
                        adc_average,
                        &voltage_mv
                    )
                );
            }
            else
            {
                voltage_mv = (adc_average * 3300) / 4095;
            }

            float voltage = voltage_mv / 1000.0f;         

            if (voltage <= 0.0f || voltage >= 3.3f)
                return -999.0f;

            float resistance =
                SERIES_RESISTOR *
                (voltage / (3.3f - voltage));

            float steinhart;

            steinhart = resistance / NOMINAL_RESISTANCE;
            steinhart = logf(steinhart);
            steinhart /= BETA_COEFFICIENT;
            steinhart +=
                1.0f /
                (NOMINAL_TEMPERATURE + 273.15f);

            steinhart = 1.0f / steinhart;
            steinhart -= 273.15f;

            return steinhart;
        }
    #endif

#else
    float get_temperature(void) return -1; // Retorna um valor inválido se o sensor não estiver habilitado
#endif




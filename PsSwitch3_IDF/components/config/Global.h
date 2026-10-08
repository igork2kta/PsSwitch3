#pragma once
#include "sdkconfig.h"
//SE A ALEXA TIVER DE GRACINHA PRA ENCONTRAR O DISPOSITIVO, UTILIZANDO CACHE E NÃO ACHANDO, ALTERAR O UNIQUEID DO /LIGHTS

#define SW_VERSION "3.1.4"

#define device_name_max_len 25

#define DEBUG_HUE true
#define DEBUG_WEBSERVER true
#define DEBUG_SSDP false
#define DEBUG_WIFI_ false
#define DEBUG_TEMPERATURE false

#define TOUCH_ENABLED true
#define SINGLE_INPUT  false 
#define TEMPERATURE_SENSOR true

#define PIN_LED 2

#if TOUCH_ENABLED 
    
    #if CONFIG_IDF_TARGET_ESP32 // ESP32 clássico
            
        #define INPUT_PIN       TOUCH_PAD_NUM0   // GPIO4
            
        #if SINGLE_INPUT == false

            #define INPUT_PIN2      TOUCH_PAD_NUM4   // GPIO13
            #define INPUT_PIN3      TOUCH_PAD_NUM3   // GPIO15
            #define INPUT_PIN4      TOUCH_PAD_NUM9   // GPIO32
        #endif

    #elif CONFIG_IDF_TARGET_ESP32S3

        #define INPUT_PIN       4

        #if SINGLE_INPUT == false
            #define INPUT_PIN2      5
            #define INPUT_PIN3      6
            #define INPUT_PIN4      7
        #endif
      #endif

#else
    //Ativo em LOW
    #define INPUT_PIN 13  
#endif


#define PLUG 1
#define VENTILADOR 2

#define DEVICE_TYPE VENTILADOR

#if DEVICE_TYPE == VENTILADOR
  #define PSSWITCH_TYPE "VENTILADOR"

  //ventilador
  #define OUTPUT_PIN_1 26
  #define OUTPUT_PIN_2 25
  #define OUTPUT_PIN_3 33

  //s3
  //#define OUTPUT_PIN_1 35
  //#define OUTPUT_PIN_2 36
  //#define OUTPUT_PIN_3 37
//Banco Baixo (Low): Controla os pinos de 0 a 31.
//Banco Alto (High): Controla os pinos de 32 para cima (32 a 39).
//Esses são usados para desligar
  #define GPIO_MASK_LOW  ( \
      ((OUTPUT_PIN_1 < 32) ? (1UL << OUTPUT_PIN_1) : 0) | \
      ((OUTPUT_PIN_2 < 32) ? (1UL << OUTPUT_PIN_2) : 0) | \
      ((OUTPUT_PIN_3 < 32) ? (1UL << OUTPUT_PIN_3) : 0) \
  )

  #define GPIO_MASK_HIGH ( \
      ((OUTPUT_PIN_1 >= 32) ? (1UL << (OUTPUT_PIN_1 - 32)) : 0) | \
      ((OUTPUT_PIN_2 >= 32) ? (1UL << (OUTPUT_PIN_2 - 32)) : 0) | \
      ((OUTPUT_PIN_3 >= 32) ? (1UL << (OUTPUT_PIN_3 - 32)) : 0) \
  )

  //Se o pino for menor que 32, ele pertence ao banco baixo (low), (1UL << pin): Pega o número 1 (Unsigned Long, garante 32 bits) e desloca esse bit para a esquerda (<<) a quantidade de vezes igual ao número do pino.
  //Se o pino for o GPIO 4, 1UL << 4 resulta no binário 00000000000000000000000000010000. Isso ativa o 4º bit do registrador.
  //Se o pino for >= 32, pertence ao banco alto (high), e faz (1UL << (pin - 32)): O segundo registrador também começa do bit 0, precisa "subtrair 32" do número do pino para achar a posição correta no novo registrador.
  #define GPIO_LOW(pin)  ((pin < 32) ? (1UL << pin) : 0)
  #define GPIO_HIGH(pin) ((pin >= 32) ? (1UL << (pin - 32)) : 0)

  //Esses são usados para ligar
  #define SPEED1_LOW   GPIO_LOW(OUTPUT_PIN_1)
  #define SPEED1_HIGH  GPIO_HIGH(OUTPUT_PIN_1)

  #define SPEED2_LOW   GPIO_LOW(OUTPUT_PIN_2)
  #define SPEED2_HIGH  GPIO_HIGH(OUTPUT_PIN_2)

  #define SPEED3_LOW   GPIO_LOW(OUTPUT_PIN_3)
  #define SPEED3_HIGH  GPIO_HIGH(OUTPUT_PIN_3)

#elif DEVICE_TYPE == PLUG
  #define PSSWITCH_TYPE "PLUG"

  #define OUTPUT_PIN_1 35

  #define GPIO_MASK_LOW  ( \
      ((OUTPUT_PIN_1 < 32) ? (1UL << OUTPUT_PIN_1) : 0)  \
  )

  #define GPIO_MASK_HIGH ( \
      ((OUTPUT_PIN_1 >= 32) ? (1UL << (OUTPUT_PIN_1 - 32)) : 0)  \
  )

  #define ON (GPIO_MASK_LOW | GPIO_MASK_HIGH)
#endif


#if TEMPERATURE_SENSOR

  #define SENSOR_NTC_10K 1
  #define SENSOR_TYPE SENSOR_NTC_10K
  #define MAX_TEMPERATURE 80.0

  //Ventilador
  #define SENSOR_PIN 34
  //#define SENSOR_PIN 4
  #define TEMPERATURE_LED_PIN 14

    #if SENSOR_TYPE == SENSOR_NTC_10K
        #define SERIES_RESISTOR 10000.0
        #define NOMINAL_RESISTANCE 10000.0
        #define NOMINAL_TEMPERATURE 25.0
        #define BETA_COEFFICIENT 3950.0
    #endif
#endif







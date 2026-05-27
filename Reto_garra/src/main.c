#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "UI_ESP";

#define UART_TX_PIN       GPIO_NUM_17
#define UART_RX_PIN       GPIO_NUM_16
#define I2C_SDA_PIN       GPIO_NUM_21
#define I2C_SCL_PIN       GPIO_NUM_22
#define BUZZER_PIN        GPIO_NUM_25
#define LED_STRIP_PIN     GPIO_NUM_13  //tira LED

#define I2C_MASTER_NUM    0
#define LCD_ADDR          0x27 

//COMandos LCD I2C (PCF8574)
#define LCD_CMD           0x00
#define LCD_DATA          0x01
#define LCD_BACKLIGHT     0x08
#define ENABLE_BIT        0x04

void lcd_send_byte(uint8_t val, uint8_t mode) {
    uint8_t high_nibble = (val & 0xF0) | mode | LCD_BACKLIGHT;
    uint8_t low_nibble = ((val << 4) & 0xF0) | mode | LCD_BACKLIGHT;
    
    uint8_t data_t[4] = {
        high_nibble | ENABLE_BIT,
        high_nibble & ~ENABLE_BIT,
        low_nibble | ENABLE_BIT,
        low_nibble & ~ENABLE_BIT
    };
    i2c_master_write_to_device(I2C_MASTER_NUM, LCD_ADDR, data_t, 4, pdMS_TO_TICKS(10));
}

void lcd_init_real() {
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_send_byte(0x03, LCD_CMD);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_send_byte(0x03, LCD_CMD);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_send_byte(0x02, LCD_CMD); //4 bitssss
    
    lcd_send_byte(0x28, LCD_CMD); // 2 líneas matriz 5x8
    lcd_send_byte(0x0C, LCD_CMD); // Display ON, Cursor OFF
    lcd_send_byte(0x06, LCD_CMD); // Auto-incrementa el cursor
    lcd_send_byte(0x01, LCD_CMD); // Limpiar pantalla
    vTaskDelay(pdMS_TO_TICKS(5));
}

void lcd_clear() {
    lcd_send_byte(0x01, LCD_CMD);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd_print_real(const char* text) {
    lcd_clear();
    while (*text) {
        lcd_send_byte((uint8_t)*text, LCD_DATA);
        text++;
    }
}

//CONTROL DE BUZZER
void play_tone(uint32_t freq, uint32_t duration_ms) {
    if (freq == 0) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        return;
    }
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 100); // Volumen
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

//LUCES
void set_lights_state(const char* state) {
    if (strcmp(state, "IDLE") == 0) {
        ESP_LOGI(TAG, "LEDs: Efecto Arcoíris / Espera");
    } else if (strcmp(state, "PLAYING") == 0) {
        ESP_LOGI(TAG, "LEDs: Color Verde Fijo (Jugando)");
    } else if (strcmp(state, "GAME_OVER") == 0) {
        ESP_LOGI(TAG, "LEDs: Destello Rojo");
    }
}

void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

void setup_buzzer() {
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .gpio_num = BUZZER_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&channel_conf);
}

void task_ui_manager(void *pvParameters) {
    uint8_t data[128];
    while (1) {
        int length = uart_read_bytes(UART_NUM_1, data, sizeof(data) - 1, pdMS_TO_TICKS(100));
        if (length > 0) {
            data[length] = '\0';
            
            if (strstr((char*)data, "STATE:INSERT_COIN")) {
                lcd_print_real("INSERTAR MONEDAS");
                set_lights_state("IDLE");
                play_tone(1000, 100); 
            } 
            else if (strstr((char*)data, "STATE:READY")) {
                lcd_print_real("¡LISTO! PULSA ST");
                set_lights_state("IDLE");
                play_tone(1500, 400); 
            }
            else if (strstr((char*)data, "STATE:PLAYING")) {
                lcd_print_real("¡A JUGAR!");
                set_lights_state("PLAYING");
                play_tone(800, 150); 
            }
            else if (strstr((char*)data, "STATE:CLAW_ACTIVE")) {
                lcd_print_real("¡ATRAPANDO!");
            }
            else if (strstr((char*)data, "STATE:GAME_OVER")) {
                lcd_print_real("JUEGO TERMINADO");
                set_lights_state("GAME_OVER");
                play_tone(400, 800); 
            }
        }
    }
}

void app_main() {
    i2c_master_init();
    lcd_init_real();
    setup_buzzer();

    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install(UART_NUM_1, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    lcd_print_real("BIENVENIDO");
    
    xTaskCreate(task_ui_manager, "ui_manager", 4096, NULL, 5, NULL);
}
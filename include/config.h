#ifndef CONFIG_H
#define CONFIG_H

// Matriz de LEDs NeoPixel 5x5 (BitDogLab)
#define PINO_MATRIZ 7
#define NUM_LEDS 25
#define AMOSTRAS_MATRIZ 256

// Microfone onboard (ADC)
#define PINO_MICROFONE 28

// Áudio / Mozzi / I2S
#define AUDIO_SAMPLE_RATE 16384
#define AUDIO_CONTROL_RATE 128 // Hz, powers of 2 are most reliable
#define AUDIO_I2S_PIN_BCK 8
#define AUDIO_I2S_PIN_DATA 20
#define TAMANHO_BUFFER 32768
#define NUM_VOZES 10

// Gravação longa: reutiliza os slots 1..3 (3 * 32 KB = 96 KB = 6 segundos @ 16384 Hz)
#define RECORD_BUFFER_SLOTS 3
#define RECORD_MAX_SAMPLES (TAMANHO_BUFFER * RECORD_BUFFER_SLOTS)

// FFT para análise de pitch (alocada sob demanda para economizar RAM)
#define FFT_PITCH_SAMPLES 2048 // Deve ser sempre uma potência de 2

// Armazenamento (SD)
#define MAX_SAMPLES_MENU 100 // Limite máximo de ficheiros WAV para mostrar no menu
#define SEQ_MAX_SONGS_MENU 32
#define SEQ_FILENAME_LEN 32

// Sequenciador estilo Mario Paint
#define SEQ_NUM_SYMBOLS 4
#define SEQ_MAX_STEPS 64
#define SEQ_MAX_NOTES_PER_STEP 6
#define SEQ_VISIBLE_STEPS 8
#define SEQ_DEFAULT_BPM 120
#define SEQ_MIN_BPM 60
#define SEQ_MAX_BPM 200

// Teclado (Expansor I2C MCP23017)
#define KEYBOARD_SDA_PIN 2
#define KEYBOARD_SCL_PIN 3
#define KEYBOARD_I2C_ADDR 0x20
#define KEYBOARD_TOTAL_KEYS 56
#define KEYBOARD_NOTE_KEYS 32
#define KEYBOARD_DEFAULT_OFFSET 53
#define KEY_COMPOSER_REST 37

// LEDs de status RGB na BitDogLab
#define LED_STATUS_G_PIN 11
#define LED_STATUS_B_PIN 12
#define LED_STATUS_R_PIN 13

// Interface de usuário (OLED, Joystick e Botões)
#define OLED_SCL_PIN 15
#define OLED_SDA_PIN 14
#define OLED_I2C_ADDR 0x3C

#define JOYSTICK_Y 26
#define JOYSTICK_X 27
#define BOTAO_JOYSTICK 22
#define BOTAO_A 5
#define BOTAO_B 6

#endif

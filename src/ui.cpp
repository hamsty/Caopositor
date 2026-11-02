
#include "ui.h"
#include <U8g2lib.h>

// I2C por software:
// SCL = GPIO 15
// SDA = GPIO 14

U8G2_SSD1306_128X64_NONAME_F_SW_I2C display(
    U8G2_R0,
    15,
    14,
    U8X8_PIN_NONE
);

static int opcaoSelecionada = 0;

static const char* opcoes[] = {
    "Modo MIDI",
    "Modo Teclado",
    "Gravar Sons"
};

bool ui_init() {
    display.setI2CAddress(0x3C * 2);
    display.begin();

    ui_desenharMenu();

    return true;
}

void ui_desenharMenu() {
    display.clearBuffer();

    display.setFont(u8g2_font_6x10_tf);

    display.drawStr(30, 10, "AOPOSITOR V2");

    for (int i = 0; i < 3; i++) {
        int y = 20 + i * 14;

        if (i == opcaoSelecionada) {
            display.setDrawColor(1);
            display.drawBox(0, y - 9, 128, 13);

            display.setDrawColor(0);
        } else {
            display.setDrawColor(1);
        }

        display.drawStr(12, y, opcoes[i]);
    }

    display.setDrawColor(1);

    // Mensagem temporaria para confirmar o firmware
    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(25, 63, "TESTE GITHUB");

    display.sendBuffer();
    }

void ui_update() {
    // Joystick sera implementado depois.
}

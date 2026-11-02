
#include "ui.h"
#include <U8g2lib.h>
#define JOYSTICK_Y 26
static bool joystickLiberado=true;
static unsigned long ultimoMovimento=0;
const unsigned long intervaloJoystick=250;

// I2C por software
// SCL=GPIO 15
// SDA=GPIO 14

// biblioteca implementada para o I2C por software, para evitar conflito
U8G2_SSD1306_128X64_NONAME_F_SW_I2C display(
    U8G2_R0,
    15,
    14,
    U8X8_PIN_NONE
);

static int opcaoSelecionada = 0;

//instrumento destacado na tela
static int instrumentoSelecionado = 0;
//instrumento confirmado
static int instrumentoAtivo = 0;

static bool instrumentoConfirmado = false;

static const char* instrumentos[] = {
    "Gram Piano",
    "Yogurt Sound",
    "Som Personalizado 1"
};

enum class Tela {
    MENU,
    MIDI,
    TECLADO,
    GRAVAR,
    GRAVANDO,
    SOM_GRAVADO,
    EDITAR_TECLADO
};

static Tela telaAtual=Tela::MENU;

#define BOTAO_JOYSTICK 22
#define BOTAO_B 6
static bool joystickPressionadoAntes=false;
static bool botaoBPressionadoAntes=false;
#define BOTAO_A 5
static bool botaoAPressionadoAntes=false;

static const char* opcoes[] = {
    "Modo MIDI",
    "Modo Teclado",
    "Gravar Sons"
};

// Som selecionado para atribuir a uma tecla
static int somParaEditar = 0;

// Exemplos temporarios de sons
static const char* sonsGravados[] = {
    "Som 1 - Do 4",
    "Som 2 - Fa 5"
};

static const int quantidadeSons = 2;

bool ui_init() {
    pinMode(JOYSTICK_Y, INPUT);
    pinMode(BOTAO_JOYSTICK, INPUT_PULLUP);
    pinMode(BOTAO_B, INPUT_PULLUP);
    pinMode(BOTAO_A, INPUT_PULLUP);
    analogReadResolution(12);
    display.setI2CAddress(0x3C * 2);
    display.begin();
    ui_desenharMenu();

    return true;
}

void ui_desenharMenu() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(30, 10, "CAOPOSITOR");

    for (int i=0; i<3; i++) {
        int y = 20+i*14;

        if (i==opcaoSelecionada) {
            display.setDrawColor(1);
            display.drawBox(0, y-9, 128, 13);

            display.setDrawColor(0);
        } else {
            display.setDrawColor(1);
        }
        display.drawStr(12, y, opcoes[i]);
    }
    display.setDrawColor(1);

    display.sendBuffer();
}


void desenharModoMIDI() {
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);

    display.drawStr(35, 12, "MODO MIDI");
    display.drawHLine(0, 17, 128);
    display.drawStr(12, 34, "Controlador MIDI");
    display.drawStr(12, 46, "via USB");
    display.drawStr(0, 63, "B: Voltar");

    display.sendBuffer();
}


void desenharModoTeclado() {
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);

    display.drawStr(23, 10, "MODO TECLADO");

    for (int i = 0; i < 3; i++) {
        int y = 24 + i * 12;

        if (i == instrumentoSelecionado) {
            display.setDrawColor(1);
            display.drawBox(0, y - 10, 128, 12);
            display.setDrawColor(0);
        } else {
            display.setDrawColor(1);
        }

        display.drawStr(4, y, instrumentos[i]);
    }

    display.setDrawColor(1);
    display.setFont(u8g2_font_5x7_tf);

    if (instrumentoConfirmado) {
        display.drawStr(0, 63, "Som selecionado!");
    } else {
        display.drawStr(0, 63, "OK: Escolher  B: Voltar");
    }

    display.sendBuffer();
}

void desenharGravarSons() {
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(28, 12, "GRAVAR SONS");
    display.drawHLine(0, 17, 128);
    display.drawStr(12, 35, "Pressione A para");
    display.drawStr(12, 47, "iniciar gravacao");
    display.drawStr(0, 63, "B: Voltar");

    display.sendBuffer();
}

void desenharGravando() {
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);

    display.drawStr(28, 12, "GRAVAR SONS");
    display.drawHLine(0, 17, 128);

    display.drawCircle(17, 34, 5);
    display.drawDisc(17, 34, 3);

    display.drawStr(30, 38, "GRAVANDO...");

    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(10, 62, "A: Parar  B: Cancelar");

    display.sendBuffer();
}

void desenharSomGravado() {
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);

    display.drawStr(28, 10, "GRAVAR SONS");
    display.drawHLine(0, 15, 128);

    display.drawStr(20, 28, "SOM GRAVADO");

    display.drawHLine(0, 44, 128);

    display.drawStr(5, 53, "A: Gravar outro");
    display.drawStr(5, 62, "B: Editar teclado");

    display.sendBuffer();
}


void desenharEditarTeclado() {
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);

    display.drawStr(20, 10, "EDITAR TECLADO");
    display.drawHLine(0, 14, 128);

    // Lista de sons
    for (int i = 0; i < quantidadeSons; i++) {
        int y = 27 + i * 13;

        if (i == somParaEditar) {
            display.setDrawColor(1);
            display.drawBox(0, y - 10, 128, 12);
            display.setDrawColor(0);
        } else {
            display.setDrawColor(1);
        }

        display.drawStr(5, y, sonsGravados[i]);
    }

    display.setDrawColor(1);
    display.drawHLine(0, 45, 128);

    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(3, 54, "Pressione uma tecla");
    display.drawStr(3, 62, "A: Voltar");

    display.sendBuffer();
}

void ui_update() {
    // Leitura dos botoes
    bool joystickPressionado =
        digitalRead(BOTAO_JOYSTICK) == LOW;

    bool botaoBPressionado =
        digitalRead(BOTAO_B) == LOW;

    bool botaoAPressionado =
    digitalRead(BOTAO_A) == LOW;

    // Debounce simples, sem bloquear o programa
    static unsigned long ultimaAcaoBotao = 0;
    unsigned long agora = millis();

    if (agora - ultimaAcaoBotao >= 50) {

        // Confirmar opcao no menu principal
        if (joystickPressionado &&
            !joystickPressionadoAntes &&
            telaAtual == Tela::MENU) {

            switch (opcaoSelecionada) {
                case 0:
                    telaAtual = Tela::MIDI;
                    desenharModoMIDI();
                    break;

                case 1:
                    telaAtual = Tela::TECLADO;
                    desenharModoTeclado();
                    break;

                case 2:
                    telaAtual = Tela::GRAVAR;
                    desenharGravarSons();
                    break;
            }

            ultimaAcaoBotao = agora;
        }

        // Confirmar instrumento no Modo Teclado
        if (joystickPressionado &&
            !joystickPressionadoAntes &&
            telaAtual == Tela::TECLADO) {

            instrumentoAtivo = instrumentoSelecionado;
            instrumentoConfirmado = true;

            desenharModoTeclado();

            ultimaAcaoBotao = agora;
        }

        // Botao A: controlar a simulacao de gravacao
        if (botaoAPressionado &&
            !botaoAPressionadoAntes) {

            if (telaAtual == Tela::GRAVAR) {
                telaAtual = Tela::GRAVANDO;
                desenharGravando();
                ultimaAcaoBotao = agora;
            }
            else if (telaAtual == Tela::GRAVANDO) {
                telaAtual = Tela::SOM_GRAVADO;
                desenharSomGravado();
                ultimaAcaoBotao = agora;
            }
            else if (telaAtual == Tela::SOM_GRAVADO) {
                telaAtual = Tela::GRAVAR;
                desenharGravarSons();
                ultimaAcaoBotao = agora;
            }
            else if (telaAtual == Tela::EDITAR_TECLADO) {
                telaAtual = Tela::SOM_GRAVADO;
                desenharSomGravado();
                ultimaAcaoBotao = agora;
            }
        }

        if (botaoBPressionado &&
            !botaoBPressionadoAntes &&
            telaAtual != Tela::MENU) {

            if (telaAtual == Tela::SOM_GRAVADO) {
                telaAtual = Tela::EDITAR_TECLADO;
                desenharEditarTeclado();
            } else {
                telaAtual = Tela::MENU;
                ui_desenharMenu();
            }

            ultimaAcaoBotao = agora;
        }

        joystickPressionadoAntes = joystickPressionado;
        botaoBPressionadoAntes = botaoBPressionado;
        botaoAPressionadoAntes = botaoAPressionado;

        // Permitir navegacao no menu principal
        // e na selecao de instrumentos
        if (telaAtual != Tela::MENU &&
            telaAtual != Tela::TECLADO &&
            telaAtual != Tela::EDITAR_TECLADO) {
            return;
        }

        // Navegacao vertical ja testada
        int valorY = analogRead(JOYSTICK_Y);

        if (valorY > 1500 && valorY < 2600) {
            joystickLiberado = true;
            return;
        }

        if (!joystickLiberado) {
            return;
        }

        if (millis() - ultimoMovimento <
            intervaloJoystick) {
            return;
        }

        // Joystick para baixo
        if (valorY < 1000) {

            if (telaAtual == Tela::MENU) {
                opcaoSelecionada =
                    (opcaoSelecionada + 1) % 3;

                ui_desenharMenu();
            }
            else if (telaAtual == Tela::TECLADO) {
                instrumentoSelecionado =
                    (instrumentoSelecionado + 1) % 3;

                instrumentoConfirmado = false;
                desenharModoTeclado();
            }
            else if (telaAtual == Tela::EDITAR_TECLADO) {
                somParaEditar =
                    (somParaEditar + 1) % quantidadeSons;

                desenharEditarTeclado();
            }

            joystickLiberado = false;
            ultimoMovimento = millis();
        }

        // Joystick para cima
        else if (valorY > 3000) {

            if (telaAtual == Tela::MENU) {
                opcaoSelecionada =
                    (opcaoSelecionada + 2) % 3;

                ui_desenharMenu();
            }
            else if (telaAtual == Tela::TECLADO) {
                instrumentoSelecionado =
                    (instrumentoSelecionado + 2) % 3;

                instrumentoConfirmado = false;
                desenharModoTeclado();
            }
            else if (telaAtual == Tela::EDITAR_TECLADO) {
                somParaEditar =
                    (somParaEditar + quantidadeSons - 1)
                    % quantidadeSons;

                desenharEditarTeclado();
            }

            joystickLiberado = false;
            ultimoMovimento = millis();
        }
    }
}
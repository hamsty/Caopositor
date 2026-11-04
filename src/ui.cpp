
#include "ui.h"
#include <U8g2lib.h>
#define JOYSTICK_Y 26
static bool joystickLiberado=true;
static unsigned long ultimoMovimento=0;
const unsigned long intervaloJoystick=250;

// biblioteca implementada para o I2C por software, para evitar conflito
U8G2_SSD1306_128X64_NONAME_F_SW_I2C display(
    U8G2_R0,
    15,
    14,
    U8X8_PIN_NONE
);

static int opcaoSelecionada = 0;

static int instrumentoSelecionado = 0;
static int instrumentoAtivo = 0;
static bool instrumentoConfirmado = false;

static const char **instrumentos;

static char instrumentFile[32];

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
    "Gravar Sons",
    "Compor Música"
};

int list_offset = 0;

int keyboard_menu_up = 0;
int keyboard_menu_down = 2;

int quantidadeInstrumentos = 0;

//som selecionado para atribuir a uma tecla
static int somParaEditar = 0;

//cada posicao representa uma tecla fisica
//-1 significa que nao existe som associado
static int mapaSons[56] = {};

//ultima tecla associada
static int ultimaTeclaAtribuida = -1;

//evento pendente para processar na interface
static int teclaPendente = -1;


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

    for (int i = 0; i < 56; i++) {
        mapaSons[i] = -1;
    }

    return true;
}

void setInstrumentList(const char** instrumentList, int quantidade) {
    instrumentos = instrumentList;
    quantidadeInstrumentos = quantidade;
}

Tela getCurrentScreen () {
    return telaAtual;
}

int getCurrentInstrumentIndex() {
    return instrumentoAtivo;
}

const char*  getCurrentInstrumentFile() {
    sprintf(instrumentFile, "%s.wav", instrumentos[instrumentoAtivo]);

    return instrumentFile;
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

    for (int i=0; i<3; i++) {
        int y = 24+i*12;

        if (i+list_offset == instrumentoSelecionado) {
            display.setDrawColor(1);
            display.drawBox(0, y - 10, 128, 12);
            display.setDrawColor(0);
        } else {
            display.setDrawColor(1);
        }
        display.drawStr(4, y, instrumentos[i+list_offset]);
    }

    display.setDrawColor(1);
    display.setFont(u8g2_font_5x7_tf);

    if (instrumentoConfirmado) {
        display.drawStr(0, 63, "Som selecionado");
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
    display.drawStr(5, 62, "B: Voltar ao menu");

    display.sendBuffer();
}

void setCurrentScreen(Tela novaTela) {
    telaAtual = novaTela;
}


// void desenharEditarTeclado() {
//     display.clearBuffer();
//     display.setDrawColor(1);
//     display.setFont(u8g2_font_6x10_tf);

//     display.drawStr(20, 10, "EDITAR TECLADO");
//     display.drawHLine(0, 14, 128);

//     //lista de sons
//     for (int i=0; i<quantidadeSons; i++) {
//         int y = 27+i*13;

//         if (i == somParaEditar) {
//             display.setDrawColor(1);
//             display.drawBox(0, y-10, 128, 12);
//             display.setDrawColor(0);
//         } else {
//             display.setDrawColor(1);
//         }

//         display.drawStr(5, y, sonsGravados[i]);
//     }

//     display.setDrawColor(1);
//     display.drawHLine(0, 45, 128);

//     display.setFont(u8g2_font_5x7_tf);

//     if (ultimaTeclaAtribuida >= 0) {
//         char mensagem[32];
//         snprintf(
//             mensagem,
//             sizeof(mensagem),
//             "Som atribuido: tecla %d",
//             ultimaTeclaAtribuida
//         );

//         display.drawStr(3, 54, mensagem);
//     } else {
//         display.drawStr(3, 54, "Pressione uma tecla");
//     }

//     display.drawStr(3, 62, "A: Voltar");
//     display.sendBuffer();
// }

void ui_update() {

    if (teclaPendente >= 0) {
        int tecla = teclaPendente;
        teclaPendente = -1;

        // if (telaAtual == Tela::EDITAR_TECLADO) {
        //     mapaSons[tecla] = somParaEditar;
        //     ultimaTeclaAtribuida = tecla;
        //     desenharEditarTeclado();
        // }
    }
    //leitura dos botoes
    bool joystickPressionado = digitalRead(BOTAO_JOYSTICK) == LOW;
    bool botaoBPressionado = digitalRead(BOTAO_B) == LOW;
    bool botaoAPressionado = digitalRead(BOTAO_A) == LOW;

    static unsigned long ultimaAcaoBotao = 0;
    unsigned long agora = millis();

    if (agora-ultimaAcaoBotao >= 50) {

        //confirmar opcao no menu principal
        if (joystickPressionado && !joystickPressionadoAntes && telaAtual == Tela::MENU) {

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

        //confirmar instrumento no Modo Teclado
        if (joystickPressionado && !joystickPressionadoAntes && telaAtual == Tela::TECLADO) {

            instrumentoAtivo = instrumentoSelecionado;
            instrumentoConfirmado = true;
            desenharModoTeclado();
            ultimaAcaoBotao = agora;
        }

        // Botao A
        if (botaoAPressionado && !botaoAPressionadoAntes) {

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
            // else if (telaAtual == Tela::EDITAR_TECLADO) {
            //     telaAtual = Tela::SOM_GRAVADO;
            //     desenharSomGravado();
            //     ultimaAcaoBotao = agora;
            // }
        }

        if (botaoBPressionado && !botaoBPressionadoAntes && telaAtual != Tela::MENU) {

            if (telaAtual != Tela::SOM_GRAVADO) {
            //     telaAtual = Tela::EDITAR_TECLADO;
            //     desenharEditarTeclado();
            // } else {
                telaAtual = Tela::MENU;
                ui_desenharMenu();
            }
            ultimaAcaoBotao = agora;
        }

        joystickPressionadoAntes = joystickPressionado;
        botaoBPressionadoAntes = botaoBPressionado;
        botaoAPressionadoAntes = botaoAPressionado;

        //permitir navegacao no menu principal e na selecao de instrumentos
        if (telaAtual != Tela::MENU && telaAtual != Tela::TECLADO){ //&& telaAtual != Tela::EDITAR_TECLADO) {
            return;
        }

        int valorY = analogRead(JOYSTICK_Y);

        if (valorY>1500 && valorY<2600) {
            joystickLiberado = true;
            return;
        }

        if (!joystickLiberado) {
            return;
        }

        if (millis()-ultimoMovimento <
            intervaloJoystick) {
            return;
        }

        //joystick para baixo
        if (valorY<1000) {

            if (telaAtual == Tela::MENU) {
                opcaoSelecionada = (opcaoSelecionada + 1) % 4;
                ui_desenharMenu();
            }
            else if (telaAtual == Tela::TECLADO) {
                instrumentoSelecionado = instrumentoSelecionado < quantidadeInstrumentos - 1 ? instrumentoSelecionado + 1 : instrumentoSelecionado;
                if( instrumentoSelecionado > keyboard_menu_up) {
                    list_offset++;
                    keyboard_menu_up++;
                    keyboard_menu_down++;
                }
                if(instrumentoSelecionado < keyboard_menu_down) {
                    list_offset--;
                    keyboard_menu_up--;
                    keyboard_menu_down--;
                }

                instrumentoConfirmado = false;
                desenharModoTeclado();
            }
            // else if (telaAtual == Tela::EDITAR_TECLADO) {
            //     somParaEditar = (somParaEditar + 1) % quantidadeSons;

            //     desenharEditarTeclado();
            // }

            joystickLiberado = false;
            ultimoMovimento = millis();
        }

        //joystick para cima
        else if (valorY > 3000) {

            if (telaAtual == Tela::MENU) {
                opcaoSelecionada = (opcaoSelecionada + 2) % 3;

                ui_desenharMenu();
            }
            else if (telaAtual == Tela::TECLADO) {
                instrumentoSelecionado = instrumentoSelecionado > 0 ? instrumentoSelecionado - 1 : instrumentoSelecionado;

                instrumentoConfirmado = false;
                desenharModoTeclado();
            }
            // // else if (telaAtual == Tela::EDITAR_TECLADO) {
            //     somParaEditar = (somParaEditar + quantidadeSons - 1)
            //         % quantidadeSons;

            //     desenharEditarTeclado();
            // }
            joystickLiberado = false;
            ultimoMovimento = millis();
        }
    }
}

// void ui_onKeyPressed(int tecla) {
//     // if (telaAtual != Tela::EDITAR_TECLADO) {
//     //     return;
//     // }
//     if (tecla < 0 || tecla >= 56) {
//         return;
//     }
//     teclaPendente = tecla;
// }
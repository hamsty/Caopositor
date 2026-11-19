#include "ui.h"
#include "config.h"
#include "audio_engine.h"
#include "pitch_detector.h"
#include "recorder.h"
#include "sequencer.h"
#include "storage.h"
#include "seq_icons.h"
#include <U8g2lib.h>
#include <string.h>

static unsigned long ultimoMovimento = 0;
static const unsigned long intervaloJoystickMenu = 220;
static const unsigned long intervaloJoystickSeq = 160;

// biblioteca implementada para o I2C por software, para evitar conflito
static U8G2_SSD1306_128X64_NONAME_F_SW_I2C display(
    U8G2_R0,
    OLED_SCL_PIN,
    OLED_SDA_PIN,
    U8X8_PIN_NONE);

static int opcaoSelecionada = 0;

static int instrumentoSelecionado = 0;
static int instrumentoAtivo = 0;
static bool instrumentoConfirmado = false;

static const char **instrumentos = nullptr;
static char instrumentFile[40];

static Tela telaAtual = Tela::MENU;

static bool joystickPressionadoAntes = false;
static bool botaoBPressionadoAntes = false;
static bool botaoAPressionadoAntes = false;

static const char *opcoes[] = {
    "Modo MIDI",
    "Modo Teclado",
    "Gravar Sons",
    "Compor Musica"};

static int list_offset = 0;
static int keyboard_menu_up = 0;
static int keyboard_menu_down = 2;
static int quantidadeInstrumentos = 0;
static uint8_t ultimaNotaTeclado = 0;

// ---------- Estado da UI do Sequenciador (Estilo Mario Paint) ----------
static bool seqFocusTopBar = false;
static int seqTopBarItem = 0; // 0:Play/Stop, 1:Simbolo, 2:BPM, 3:Compasso, 4:Config
static bool seqEditingBpm = false;
static uint8_t seqActiveSymbol = 0;
static int seqCursorStep = 0;
static int seqScrollStep = 0;
static uint8_t seqLastNoteMidi = 0;

// Redesenho diferido para nunca travar a varredura de acordes no teclado com I2C do OLED
static bool uiDirty = false;
static unsigned long uiLastKeyEventMs = 0;
static const unsigned long UI_KEY_REDRAW_DEBOUNCE_MS = 25;

// Controle de teclas seguradas para registro de acordes e sustentação por múltiplos slots
static bool seqHeldKeys[128] = {false};
static uint8_t seqHeldCount = 0;
static bool seqChordRecorded = false;
static uint8_t seqRecordStartStep = 0;
static uint8_t seqCurrentHoldDuration = 1;
static unsigned long seqHoldStepStartMs = 0;
static uint8_t seqChordNotes[SEQ_MAX_NOTES_PER_STEP] = {0};
static uint8_t seqChordNoteCount = 0;

static void resetComposerHeldKeys()
{
    memset(seqHeldKeys, 0, sizeof(seqHeldKeys));
    seqHeldCount = 0;
    seqChordRecorded = false;
    seqCurrentHoldDuration = 1;
    seqChordNoteCount = 0;
}

// Submenu de configurações do sequenciador
static const int SEQ_CONFIG_ITEMS = 6;
static int seqConfigSel = 0;
static int seqConfigOffset = 0;
static char seqStatusMsg[24] = "";

// Submenu de símbolos e seleção de sample
static int seqSymbolSel = 0;
static int seqSampleSel = 0;
static int seqSampleOffset = 0;

// Submenu de arquivos (.cseq)
static int seqSongSel = 0;
static int seqSongOffset = 0;

// ---------- Estado do Editor de Gravação (Seletor de Forma de Onda estilo Instagram) ----------
static const int TRIM_WAVE_WIDTH = 120;
static uint32_t trimTotalSamples = 0;
static uint32_t trimStart = 0;
static uint32_t trimLen = TAMANHO_BUFFER;
static uint8_t trimWaveformCols[TRIM_WAVE_WIDTH];
static bool trimPreviewPlaying = false;
static unsigned long trimPreviewStartMs = 0;
static unsigned long trimPreviewDurationMs = 0;
static unsigned long ultimoUpdateGravandoMs = 0;
static unsigned long ultimoUpdatePreviewMs = 0;

// Forward declarations das telas
static void desenharModoMIDI();
static void desenharModoTeclado();
static void desenharGravarSons();
static void desenharGravando();
static void desenharEditarGravacao();
static void desenharCompositor();
static void desenharSeqConfig();
static void desenharSeqSimbolos();
static void desenharSeqEscolherSample();
static void desenharSeqArquivos();

bool ui_init()
{
    pinMode(JOYSTICK_Y, INPUT);
    pinMode(JOYSTICK_X, INPUT);
    pinMode(BOTAO_JOYSTICK, INPUT_PULLUP);
    pinMode(BOTAO_B, INPUT_PULLUP);
    pinMode(BOTAO_A, INPUT_PULLUP);

    analogReadResolution(12);
    display.setI2CAddress(OLED_I2C_ADDR * 2);
    display.begin();
    ui_desenharMenu();

    return true;
}

void setInstrumentList(const char **instrumentList, int quantidade)
{
    instrumentos = instrumentList;
    quantidadeInstrumentos = quantidade;
    if (instrumentoSelecionado >= quantidadeInstrumentos && quantidadeInstrumentos > 0)
    {
        instrumentoSelecionado = quantidadeInstrumentos - 1;
    }
    if (instrumentoAtivo >= quantidadeInstrumentos && quantidadeInstrumentos > 0)
    {
        instrumentoAtivo = 0;
    }
}

Tela getCurrentScreen()
{
    return telaAtual;
}

int getCurrentInstrumentIndex()
{
    return instrumentoAtivo;
}

const char *getCurrentInstrumentFile()
{
    if (instrumentos == nullptr || quantidadeInstrumentos <= 0 || instrumentoAtivo >= quantidadeInstrumentos)
    {
        return "";
    }
    snprintf(instrumentFile, sizeof(instrumentFile), "%s.wav", instrumentos[instrumentoAtivo]);
    return instrumentFile;
}

void ui_show_loading(const char *msg)
{
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(16, 36, msg != nullptr ? msg : "Carregando...");
    display.sendBuffer();
}

void ui_desenharMenu()
{
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(30, 10, "CAOPOSITOR");

    for (int i = 0; i < 4; i++)
    {
        int y = 20 + i * 14;

        if (i == opcaoSelecionada)
        {
            display.setDrawColor(1);
            display.drawBox(0, y - 9, 128, 13);
            display.setDrawColor(0);
        }
        else
        {
            display.setDrawColor(1);
        }
        display.drawStr(12, y, opcoes[i]);
    }
    display.setDrawColor(1);
    display.sendBuffer();
}

static void desenharModoMIDI()
{
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

static void desenharModoTeclado()
{
    uiDirty = false;
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);

    display.drawStr(23, 10, "MODO TECLADO");

    if (quantidadeInstrumentos <= 0 || instrumentos == nullptr)
    {
        display.drawStr(10, 34, "Sem samples no SD");
    }
    else
    {
        for (int i = 0; i < 3 && (i + list_offset) < quantidadeInstrumentos; i++)
        {
            int y = 24 + i * 12;

            if (i + list_offset == instrumentoSelecionado)
            {
                display.setDrawColor(1);
                display.drawBox(0, y - 10, 128, 12);
                display.setDrawColor(0);
            }
            else
            {
                display.setDrawColor(1);
            }
            display.drawStr(4, y, instrumentos[i + list_offset]);
        }
    }

    display.setDrawColor(1);
    display.setFont(u8g2_font_5x7_tf);

    if (ultimaNotaTeclado != 0)
    {
        char notaBuf[24];
        char fullBuf[16];
        sequencer_midi_to_full_dual_name(ultimaNotaTeclado, fullBuf, sizeof(fullBuf));
        snprintf(notaBuf, sizeof(notaBuf), "Nota: %s", fullBuf);
        display.drawUTF8(0, 63, notaBuf);
    }
    else if (instrumentoConfirmado)
    {
        display.drawStr(0, 63, "Som selecionado");
    }
    else
    {
        display.drawStr(0, 63, "OK: Escolher  B: Voltar");
    }
    display.sendBuffer();
}

static void desenharGravarSons()
{
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

static void desenharGravando()
{
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);

    display.drawStr(28, 11, "GRAVAR SONS");
    display.drawHLine(0, 14, 128);

    display.drawCircle(15, 27, 4);
    display.drawDisc(15, 27, 2);

    uint32_t gravado = recorder_get_sample_index();
    uint32_t maximo = recorder_get_max_samples();
    float segAtual = (float)gravado / (float)AUDIO_SAMPLE_RATE;
    float segMax = (float)maximo / (float)AUDIO_SAMPLE_RATE;

    char tempoStr[24];
    snprintf(tempoStr, sizeof(tempoStr), "REC %.1fs / %.1fs", segAtual, segMax);
    display.drawStr(25, 31, tempoStr);

    // Barra de progresso da gravação
    display.drawFrame(10, 39, 108, 9);
    int fillW = (maximo > 0) ? (int)((gravado * 104UL) / maximo) : 0;
    if (fillW > 104)
        fillW = 104;
    if (fillW > 0)
    {
        display.drawBox(12, 41, fillW, 5);
    }

    display.setFont(u8g2_font_5x7_tf);
    display.drawStr(4, 62, "A: Parar/Editar  B: Sair");

    display.sendBuffer();
}

static void trim_stop_preview()
{
    if (trimPreviewPlaying)
    {
        audio_handle_key(0x80, 60, 0);
        audio_stop_all();
        trimPreviewPlaying = false;
    }
}

static void trim_compute_waveform()
{
    const int8_t *raw = recorder_get_buffer();
    memset(trimWaveformCols, 0, sizeof(trimWaveformCols));

    if (raw == nullptr || trimTotalSamples == 0)
        return;

    // 1. Calcula DC offset aproximado
    long somaDC = 0;
    uint32_t stepDC = (trimTotalSamples > 2048) ? (trimTotalSamples / 2048) : 1;
    uint32_t countDC = 0;
    for (uint32_t i = 0; i < trimTotalSamples; i += stepDC)
    {
        somaDC += raw[i];
        countDC++;
    }
    int dcOffset = (countDC > 0) ? (int)(somaDC / (long)countDC) : 0;

    // 2. Encontra pico por coluna e pico global para normalização visual
    uint8_t colPeaks[TRIM_WAVE_WIDTH];
    int globalPeak = 12; // Piso mínimo para silêncio não virar ruído em tela cheia

    for (int col = 0; col < TRIM_WAVE_WIDTH; col++)
    {
        uint32_t s0 = (uint32_t)(((uint64_t)col * trimTotalSamples) / TRIM_WAVE_WIDTH);
        uint32_t s1 = (uint32_t)(((uint64_t)(col + 1) * trimTotalSamples) / TRIM_WAVE_WIDTH);
        if (s1 <= s0)
            s1 = s0 + 1;
        if (s1 > trimTotalSamples)
            s1 = trimTotalSamples;

        uint32_t stride = (s1 - s0 > 64) ? ((s1 - s0) / 64) : 1;
        int colMax = 0;
        for (uint32_t s = s0; s < s1; s += stride)
        {
            int val = abs((int)raw[s] - dcOffset);
            if (val > colMax)
                colMax = val;
        }
        colPeaks[col] = (uint8_t)(colMax > 127 ? 127 : colMax);
        if (colMax > globalPeak)
            globalPeak = colMax;
    }

    // 3. Normaliza alturas para 0..17 pixels (metade da área de onda de 38 px)
    for (int col = 0; col < TRIM_WAVE_WIDTH; col++)
    {
        int h = ((int)colPeaks[col] * 17) / globalPeak;
        if (h > 17)
            h = 17;
        trimWaveformCols[col] = (uint8_t)h;
    }
}

static void trim_start_preview()
{
    const int8_t *raw = recorder_get_buffer();
    if (raw == nullptr || trimTotalSamples == 0 || trimLen == 0)
        return;

    trim_stop_preview();

    uint32_t copyLen = trimLen;
    if (copyLen > TAMANHO_BUFFER)
        copyLen = TAMANHO_BUFFER;
    if (trimStart + copyLen > trimTotalSamples)
        copyLen = trimTotalSamples - trimStart;

    // Copia o trecho selecionado para o slot 0 sem sobrescrever a gravação bruta (slots 1..3)
    audio_set_i2s_enabled(false);
    int8_t *slot0 = audio_get_sample_buffer(0);
    memcpy(slot0, raw + trimStart, copyLen);
    if (copyLen < TAMANHO_BUFFER)
    {
        memset(slot0 + copyLen, 0, TAMANHO_BUFFER - copyLen);
    }
    audio_set_slot_length(copyLen, 0);
    audio_set_base_midi_note(60.0f, 0); // Toca na velocidade/tom original
    audio_set_i2s_enabled(true);

    audio_handle_key(0x90, 60, 0);
    trimPreviewPlaying = true;
    trimPreviewStartMs = millis();
    trimPreviewDurationMs = (copyLen * 1000UL) / AUDIO_SAMPLE_RATE;
    if (trimPreviewDurationMs < 100)
        trimPreviewDurationMs = 100;
    ultimoUpdatePreviewMs = trimPreviewStartMs;
}

static void trim_save_clip()
{
    const int8_t *raw = recorder_get_buffer();
    if (raw == nullptr || trimTotalSamples == 0 || trimLen == 0)
        return;

    trim_stop_preview();
    audio_set_i2s_enabled(false);
    ui_show_loading("Salvando WAV...");

    uint32_t copyLen = trimLen;
    if (copyLen > TAMANHO_BUFFER)
        copyLen = TAMANHO_BUFFER;
    if (trimStart + copyLen > trimTotalSamples)
        copyLen = trimTotalSamples - trimStart;

    int8_t *slot0 = audio_get_sample_buffer(0);
    memcpy(slot0, raw + trimStart, copyLen);
    if (copyLen < TAMANHO_BUFFER)
    {
        memset(slot0 + copyLen, 0, TAMANHO_BUFFER - copyLen);
    }

    audio_set_slot_length(copyLen, 0);
    float notaDetectada = pitch_detect_midi_note(slot0, copyLen);
    audio_set_base_midi_note(notaDetectada, 0);
    sequencer_mark_all_slots_dirty();

    char nomeArquivo[24];
    snprintf(nomeArquivo, sizeof(nomeArquivo), "recsample%d.wav", storage_next_recorded_index());
    if (storage_save_wav(nomeArquivo, slot0, copyLen))
    {
        setInstrumentList(storage_get_sample_list(), storage_get_sample_count());
    }

    telaAtual = Tela::SOM_GRAVADO;
    desenharSomGravado();
}

static void desenharEditarGravacao()
{
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_5x7_tf);

    float inicioS = (float)trimStart / (float)AUDIO_SAMPLE_RATE;
    float fimS = (float)(trimStart + trimLen) / (float)AUDIO_SAMPLE_RATE;
    float durS = (float)trimLen / (float)AUDIO_SAMPLE_RATE;

    // Barra superior com intervalo e duração do recorte
    char topLeft[20];
    snprintf(topLeft, sizeof(topLeft), "%.1fs-%.1fs", inicioS, fimS);
    display.drawStr(2, 8, topLeft);

    char topRight[14];
    if (trimPreviewPlaying)
        snprintf(topRight, sizeof(topRight), "Ouvindo");
    else
        snprintf(topRight, sizeof(topRight), "[%.2fs]", durS);
    display.drawStr(88, 8, topRight);

    display.drawHLine(0, 10, 128);

    // Mapeamento da janela de seleção (x = 4..123)
    int xSelStart = 4;
    int xSelEnd = 4 + TRIM_WAVE_WIDTH - 1;
    if (trimTotalSamples > 0)
    {
        xSelStart = 4 + (int)(((uint64_t)trimStart * (TRIM_WAVE_WIDTH - 1)) / trimTotalSamples);
        xSelEnd = 4 + (int)(((uint64_t)(trimStart + trimLen) * (TRIM_WAVE_WIDTH - 1)) / trimTotalSamples);
        if (xSelEnd < xSelStart + 4)
            xSelEnd = xSelStart + 4;
        if (xSelEnd > 4 + TRIM_WAVE_WIDTH - 1)
            xSelEnd = 4 + TRIM_WAVE_WIDTH - 1;
    }

    const int yMid = 31;

    // Desenha forma de onda (sólida dentro do recorte, pontilhada fora)
    for (int col = 0; col < TRIM_WAVE_WIDTH; col++)
    {
        int x = 4 + col;
        int h = trimWaveformCols[col];
        bool inside = (x >= xSelStart && x <= xSelEnd);

        if (inside)
        {
            if (h == 0)
                display.drawPixel(x, yMid);
            else
                display.drawVLine(x, yMid - h, 2 * h + 1);
        }
        else
        {
            if (h == 0)
            {
                if (col % 2 == 0)
                    display.drawPixel(x, yMid);
            }
            else
            {
                for (int py = yMid - h; py <= yMid + h; py += 2)
                {
                    display.drawPixel(x, py);
                }
            }
        }
    }

    // Moldura estilo seletor do Instagram (alças laterais grossas + trilhos superior/inferior)
    int selW = xSelEnd - xSelStart + 1;
    display.drawBox(xSelStart, 12, 2, 40);
    display.drawBox(xSelEnd - 1, 12, 2, 40);
    display.drawHLine(xSelStart, 12, selW);
    display.drawHLine(xSelStart, 51, selW);

    // Playhead correndo dentro da janela durante o preview
    if (trimPreviewPlaying && trimPreviewDurationMs > 0)
    {
        unsigned long elapsed = millis() - trimPreviewStartMs;
        if (elapsed > trimPreviewDurationMs)
            elapsed = trimPreviewDurationMs;
        int xPlay = xSelStart + (int)((elapsed * (unsigned long)(xSelEnd - xSelStart)) / trimPreviewDurationMs);
        display.drawTriangle(xPlay, 52, xPlay - 2, 55, xPlay + 2, 55);
    }

    // Rodapé de comandos
    display.drawStr(2, 63, "A:Ouvir OK:Salvar B:Sair");
    display.sendBuffer();
}

void ui_open_recording_editor()
{
    trim_stop_preview();
    trimTotalSamples = recorder_get_sample_index();
    if (trimTotalSamples == 0)
    {
        telaAtual = Tela::GRAVAR;
        desenharGravarSons();
        return;
    }

    trimStart = 0;
    trimLen = (trimTotalSamples < TAMANHO_BUFFER) ? trimTotalSamples : TAMANHO_BUFFER;
    trim_compute_waveform();

    telaAtual = Tela::EDITAR_GRAVACAO;
    desenharEditarGravacao();
}

void desenharSomGravado()
{
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

// ---------- Telas do Sequenciador Estilo Mario Paint ----------

static void desenharCompositor()
{
    uiDirty = false;
    const SeqSong &song = sequencer_get_song();
    bool playing = sequencer_is_playing();
    uint8_t playStep = sequencer_get_play_step();
    uint8_t totalSteps = sequencer_get_total_steps();
    uint8_t stepsPerBar = sequencer_get_steps_per_bar();

    if (seqCursorStep >= totalSteps)
    {
        seqCursorStep = totalSteps - 1;
    }

    // Auto-scroll por página durante reprodução para leitura estável da pauta
    if (playing)
    {
        int pageStart = (playStep / SEQ_VISIBLE_STEPS) * SEQ_VISIBLE_STEPS;
        if (pageStart + SEQ_VISIBLE_STEPS > totalSteps && totalSteps >= SEQ_VISIBLE_STEPS)
        {
            pageStart = totalSteps - SEQ_VISIBLE_STEPS;
        }
        seqScrollStep = pageStart;
    }
    else
    {
        if (seqCursorStep < seqScrollStep)
        {
            seqScrollStep = seqCursorStep;
        }
        else if (seqCursorStep >= seqScrollStep + SEQ_VISIBLE_STEPS)
        {
            seqScrollStep = seqCursorStep - SEQ_VISIBLE_STEPS + 1;
        }
    }

    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_5x7_tf);

    // ===== 1. BARRA SUPERIOR (y = 0..9) =====
    // Item 0: Play / Stop (x=1..10)
    if (seqFocusTopBar && seqTopBarItem == 0)
    {
        display.drawBox(0, 0, 11, 9);
        display.setDrawColor(0);
    }
    if (playing)
    {
        display.drawXBMP(3, 2, 5, 5, ICON_STOP);
    }
    else
    {
        display.drawXBMP(3, 1, 5, 7, ICON_PLAY);
    }
    display.setDrawColor(1);

    // Item 1: Símbolo ativo (x=14..24)
    if (seqFocusTopBar && seqTopBarItem == 1)
    {
        display.drawBox(13, 0, 11, 9);
        display.setDrawColor(0);
    }
    display.drawXBMP(15, 1, 7, 7, SEQ_SYMBOL_ICONS[seqActiveSymbol]);
    display.setDrawColor(1);

    // Item 2: BPM (x=25..53)
    char bpmStr[10];
    if (seqEditingBpm)
        snprintf(bpmStr, sizeof(bpmStr), "<%d>", song.bpm);
    else
        snprintf(bpmStr, sizeof(bpmStr), "%d", song.bpm);

    if (seqFocusTopBar && seqTopBarItem == 2)
    {
        display.drawBox(25, 0, 28, 9);
        display.setDrawColor(0);
    }
    display.drawStr(26, 8, bpmStr);
    display.setDrawColor(1);

    // Item 3: Compasso (x=54..72)
    char sigStr[6];
    snprintf(sigStr, sizeof(sigStr), "%d/4", song.beatsPerBar);
    if (seqFocusTopBar && seqTopBarItem == 3)
    {
        display.drawBox(54, 0, 18, 9);
        display.setDrawColor(0);
    }
    display.drawStr(56, 8, sigStr);
    display.setDrawColor(1);

    // Indicador de nota em Dó/Ré/Mi + Cifra (ou passo/compasso quando vazio) (x=75..115)
    char infoStr[16];
    int refStep = playing ? playStep : seqCursorStep;
    uint8_t stepNote = 0;
    int stepNoteCount = 0;
    if (refStep >= 0 && refStep < SEQ_MAX_STEPS)
    {
        for (int s = 0; s < SEQ_MAX_NOTES_PER_STEP; s++)
        {
            if (song.steps[refStep][s].midiNote != 0)
            {
                if (stepNote == 0)
                    stepNote = song.steps[refStep][s].midiNote;
                stepNoteCount++;
            }
        }
    }

    if (seqHeldCount > 0 && seqLastNoteMidi != 0)
    {
        sequencer_midi_to_compact_dual_name(seqLastNoteMidi, infoStr, sizeof(infoStr));
    }
    else if (stepNote != 0)
    {
        if (stepNoteCount > 1)
        {
            char nBuf[10];
            sequencer_midi_to_note_name(stepNote, nBuf, sizeof(nBuf));
            snprintf(infoStr, sizeof(infoStr), "%s+%d", nBuf, stepNoteCount - 1);
        }
        else
        {
            sequencer_midi_to_compact_dual_name(stepNote, infoStr, sizeof(infoStr));
        }
    }
    else if (playing)
    {
        int barNum = (playStep / stepsPerBar) + 1;
        snprintf(infoStr, sizeof(infoStr), "C%d/%d", barNum, song.numBars);
    }
    else
    {
        snprintf(infoStr, sizeof(infoStr), "P%d/%d", seqCursorStep + 1, totalSteps);
    }
    display.drawUTF8(75, 8, infoStr);

    // Item 4: Engrenagem de Configurações (x=117..127)
    if (seqFocusTopBar && seqTopBarItem == 4)
    {
        display.drawBox(117, 0, 11, 9);
        display.setDrawColor(0);
    }
    display.drawXBMP(119, 1, 7, 7, ICON_GEAR);
    display.setDrawColor(1);

    // ===== 2. CLAVES DE SOL E FÁ (x = 0..13) =====
    display.drawVLine(12, 14, 41); // Linha unindo as duas claves
    display.drawXBMP(2, 12, 8, 21, ICON_CLEF_TREBLE);
    display.drawXBMP(2, 38, 8, 11, ICON_CLEF_BASS);

    // ===== 3. LINHAS DAS DUAS PAUTAS (x = 13..127) =====
    // Clave de Sol: y = 14, 18, 22, 26, 30
    for (int l = 0; l < 5; l++)
    {
        display.drawHLine(13, 14 + l * 4, 115);
    }
    // Clave de Fá: y = 38, 42, 46, 50, 54
    for (int l = 0; l < 5; l++)
    {
        display.drawHLine(13, 38 + l * 4, 115);
    }

    // ===== 4A. BARRAS DE SUSTENTAÇÃO (NOTAS COM DURAÇÃO > 1 SLOT) =====
    int viewEndStep = seqScrollStep + SEQ_VISIBLE_STEPS - 1;
    for (int s = 0; s < totalSteps; s++)
    {
        for (int slot = 0; slot < SEQ_MAX_NOTES_PER_STEP; slot++)
        {
            const SeqNote &n = song.steps[s][slot];
            if (n.midiNote == 0 || n.duration <= 1)
                continue;

            int noteEndStep = s + n.duration - 1;
            if (noteEndStep >= totalSteps)
                noteEndStep = totalSteps - 1;

            // Ignora se não intersecta a janela visível
            if (noteEndStep < seqScrollStep || s > viewEndStep)
                continue;

            int noteY = 34;
            bool isSharp = false;
            sequencer_midi_to_staff_y(n.midiNote, noteY, isSharp);

            int xStart = (s >= seqScrollStep)
                             ? (15 + (s - seqScrollStep) * 14 + 7 + 4)
                             : 14;
            bool drawEndCap = (noteEndStep <= viewEndStep);
            int xEnd = drawEndCap
                           ? (15 + (noteEndStep - seqScrollStep) * 14 + 7 + 2)
                           : 127;

            if (xEnd >= xStart)
            {
                int w = xEnd - xStart + 1;
                // Faixa oca de 3 px (branco-preto-branco) para destacar sobre linha OU espaço da pauta
                display.setDrawColor(1);
                display.drawHLine(xStart, noteY - 1, w);
                display.drawHLine(xStart, noteY + 1, w);
                display.setDrawColor(0);
                display.drawHLine(xStart, noteY, w);
                display.setDrawColor(1);
                if (drawEndCap)
                {
                    display.drawVLine(xEnd, noteY - 1, 3);
                }
            }
        }
    }

    // ===== 4B. COLUNAS DE PASSOS (8 passos visíveis, 14 px cada) =====
    for (int col = 0; col < SEQ_VISIBLE_STEPS; col++)
    {
        int step = seqScrollStep + col;
        if (step >= totalSteps)
            break;

        int xCol = 15 + col * 14;
        int xCenter = xCol + 7;

        // Barra vertical de compasso
        if (step > 0 && (step % stepsPerBar == 0))
        {
            display.drawVLine(xCol, 14, 17);
            display.drawVLine(xCol, 38, 17);
        }

        // Guia sutil da coluna ativa (playhead durante reprodução ou slot de gravação)
        bool isHighlightedCol = playing ? (step == playStep) : (!seqFocusTopBar && step == seqCursorStep);
        if (isHighlightedCol)
        {
            for (int py = 12; py <= 56; py += 4)
            {
                display.drawPixel(xCenter, py);
            }
            if (!playing)
            {
                display.drawHLine(xCenter - 2, 11, 5);
            }
        }

        // Desenha notas existentes nesse passo
        for (int slot = 0; slot < SEQ_MAX_NOTES_PER_STEP; slot++)
        {
            const SeqNote &n = song.steps[step][slot];
            if (n.midiNote == 0 || n.symbolId >= SEQ_NUM_SYMBOLS)
                continue;

            int noteY = 34;
            bool isSharp = false;
            sequencer_midi_to_staff_y(n.midiNote, noteY, isSharp);

            // Linha suplementar do Dó Central (C4)
            if (noteY == 34)
            {
                display.drawHLine(xCenter - 5, 34, 11);
            }

            // Máscara preta de 1px atrás do símbolo para destacar sobre as linhas da pauta
            display.setDrawColor(0);
            display.drawBox(xCenter - 4, noteY - 4, 9, 9);
            display.setDrawColor(1);
            display.drawXBMP(xCenter - 3, noteY - 3, 7, 7, SEQ_SYMBOL_ICONS[n.symbolId]);

            // Sustenido '#' à esquerda do ícone
            if (isSharp)
            {
                display.setDrawColor(0);
                display.drawBox(xCenter - 8, noteY - 3, 5, 7);
                display.setDrawColor(1);
                display.drawXBMP(xCenter - 7, noteY - 2, 3, 5, ICON_SHARP);
            }
        }

        // ===== 5. MARCADORES DE PASSO E PLAYHEAD INFERIOR (y = 59..63) =====
        bool isActiveMarker = playing ? (step == playStep) : (step == seqCursorStep);
        if (isActiveMarker)
        {
            display.drawTriangle(xCenter, 59, xCenter - 3, 63, xCenter + 3, 63);
        }
        else if (step % stepsPerBar == 0)
        {
            display.drawBox(xCenter - 1, 61, 3, 2);
        }
        else
        {
            display.drawPixel(xCenter, 62);
        }
    }

    display.sendBuffer();
}

static void desenharSeqConfig()
{
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(14, 10, "CONFIG SEQUENCER");
    display.drawHLine(0, 12, 128);

    char itemBuf[SEQ_CONFIG_ITEMS][24];
    snprintf(itemBuf[0], sizeof(itemBuf[0]), "Config. Simbolos");
    snprintf(itemBuf[1], sizeof(itemBuf[1]), "Compasso: %d/4", sequencer_get_beats_per_bar());
    snprintf(itemBuf[2], sizeof(itemBuf[2]), "Compassos: %d", sequencer_get_num_bars());
    snprintf(itemBuf[3], sizeof(itemBuf[3]), "Salvar Musica");
    snprintf(itemBuf[4], sizeof(itemBuf[4]), "Carregar Musica");
    snprintf(itemBuf[5], sizeof(itemBuf[5]), "Limpar Partitura");

    display.setFont(u8g2_font_5x7_tf);
    for (int i = 0; i < 4; i++)
    {
        int idx = seqConfigOffset + i;
        if (idx >= SEQ_CONFIG_ITEMS)
            break;

        int y = 23 + i * 10;
        if (idx == seqConfigSel)
        {
            display.setDrawColor(1);
            display.drawBox(0, y - 8, 128, 10);
            display.setDrawColor(0);
        }
        else
        {
            display.setDrawColor(1);
        }
        display.drawStr(4, y, itemBuf[idx]);
    }

    display.setDrawColor(1);
    if (seqStatusMsg[0] != '\0')
    {
        display.drawStr(0, 63, seqStatusMsg);
    }
    else
    {
        display.drawStr(0, 63, "OK: Sel   B: Partitura");
    }

    display.sendBuffer();
}

static void desenharSeqSimbolos()
{
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(16, 10, "SIMBOLOS -> WAV");
    display.drawHLine(0, 12, 128);

    display.setFont(u8g2_font_5x7_tf);
    for (int i = 0; i < SEQ_NUM_SYMBOLS; i++)
    {
        int y = 22 + i * 10;
        if (i == seqSymbolSel)
        {
            display.setDrawColor(1);
            display.drawBox(0, y - 8, 128, 10);
            display.setDrawColor(0);
        }
        else
        {
            display.setDrawColor(1);
        }

        display.drawXBMP(2, y - 7, 7, 7, SEQ_SYMBOL_ICONS[i]);

        char linha[28];
        snprintf(linha, sizeof(linha), "%s: %.12s", SEQ_SYMBOL_NAMES[i], sequencer_get_symbol_sample_name(i));
        display.drawStr(13, y, linha);
    }

    display.setDrawColor(1);
    display.drawStr(0, 63, "OK: Trocar   B: Voltar");
    display.sendBuffer();
}

static void desenharSeqEscolherSample()
{
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);

    char titulo[24];
    snprintf(titulo, sizeof(titulo), "WAV p/ %s", SEQ_SYMBOL_NAMES[seqSymbolSel]);
    display.drawStr(10, 10, titulo);
    display.drawHLine(0, 12, 128);

    display.setFont(u8g2_font_5x7_tf);
    if (quantidadeInstrumentos <= 0 || instrumentos == nullptr)
    {
        display.drawStr(10, 36, "Sem samples no SD");
    }
    else
    {
        for (int i = 0; i < 4 && (seqSampleOffset + i) < quantidadeInstrumentos; i++)
        {
            int idx = seqSampleOffset + i;
            int y = 23 + i * 10;
            if (idx == seqSampleSel)
            {
                display.setDrawColor(1);
                display.drawBox(0, y - 8, 128, 10);
                display.setDrawColor(0);
            }
            else
            {
                display.setDrawColor(1);
            }
            display.drawStr(4, y, instrumentos[idx]);
        }
    }

    display.setDrawColor(1);
    display.drawStr(0, 63, "OK: Usar     B: Voltar");
    display.sendBuffer();
}

static void desenharSeqArquivos()
{
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(16, 10, "CARREGAR MUSICA");
    display.drawHLine(0, 12, 128);

    display.setFont(u8g2_font_5x7_tf);
    int totalSongs = storage_get_song_count();
    if (totalSongs <= 0)
    {
        display.drawStr(14, 36, "Nenhuma musica salva");
    }
    else
    {
        for (int i = 0; i < 4 && (seqSongOffset + i) < totalSongs; i++)
        {
            int idx = seqSongOffset + i;
            int y = 23 + i * 10;
            if (idx == seqSongSel)
            {
                display.setDrawColor(1);
                display.drawBox(0, y - 8, 128, 10);
                display.setDrawColor(0);
            }
            else
            {
                display.setDrawColor(1);
            }
            display.drawStr(4, y, storage_get_song_name(idx));
        }
    }

    display.setDrawColor(1);
    display.drawStr(0, 63, "OK: Abrir    B: Voltar");
    display.sendBuffer();
}

void setCurrentScreen(Tela novaTela)
{
    telaAtual = novaTela;
}

void ui_redraw_current()
{
    uiDirty = false;
    switch (telaAtual)
    {
    case Tela::MENU:
        ui_desenharMenu();
        break;
    case Tela::MIDI:
        desenharModoMIDI();
        break;
    case Tela::TECLADO:
        desenharModoTeclado();
        break;
    case Tela::GRAVAR:
        desenharGravarSons();
        break;
    case Tela::GRAVANDO:
        desenharGravando();
        break;
    case Tela::EDITAR_GRAVACAO:
        desenharEditarGravacao();
        break;
    case Tela::SOM_GRAVADO:
        desenharSomGravado();
        break;
    case Tela::COMPOSITOR:
        desenharCompositor();
        break;
    case Tela::SEQ_CONFIG:
        desenharSeqConfig();
        break;
    case Tela::SEQ_SIMBOLOS:
        desenharSeqSimbolos();
        break;
    case Tela::SEQ_ESCOLHER_SAMPLE:
        desenharSeqEscolherSample();
        break;
    case Tela::SEQ_ARQUIVOS:
        desenharSeqArquivos();
        break;
    }
}

uint8_t ui_get_active_symbol()
{
    return seqActiveSymbol;
}

void ui_on_keyboard_note_played(uint8_t note)
{
    ultimaNotaTeclado = note;
    if (telaAtual == Tela::TECLADO)
    {
        uiDirty = true;
        uiLastKeyEventMs = millis();
    }
}

void ui_on_composer_rest_key()
{
    if (telaAtual != Tela::COMPOSITOR || sequencer_is_playing())
    {
        return;
    }

    seqFocusTopBar = false;
    seqEditingBpm = false;
    resetComposerHeldKeys();
    seqLastNoteMidi = 0;

    // Garante pausa (silêncio) no slot atual e avança para o próximo slot
    sequencer_clear_step(seqCursorStep);
    uint8_t totalSteps = sequencer_get_total_steps();
    seqCursorStep = (seqCursorStep + 1) % totalSteps;

    uiDirty = true;
    uiLastKeyEventMs = 0; // Redesenha imediatamente após sair do scan do teclado
}

void ui_on_composer_piano_key(uint8_t code, uint8_t note)
{
    if (telaAtual != Tela::COMPOSITOR || note >= 128)
    {
        return;
    }

    if (code == 0x90)
    {
        // Dispara áudio imediatamente sem bloquear a varredura do teclado com o OLED
        audio_handle_key(0x90, note, seqActiveSymbol);
        seqLastNoteMidi = note;

        // Em modo edição (parado), registra nota ou acorde no slot atual
        if (!sequencer_is_playing())
        {
            seqFocusTopBar = false;
            seqEditingBpm = false;

            if (seqHeldCount == 0)
            {
                seqRecordStartStep = seqCursorStep;
                seqCurrentHoldDuration = 1;
                seqHoldStepStartMs = millis();
                seqChordNoteCount = 0;
            }

            if (!seqHeldKeys[note])
            {
                seqHeldKeys[note] = true;
                seqHeldCount++;
            }

            // Guarda a nota na lista do acorde atual para estender sua sustentação enquanto segurada
            bool jaNaLista = false;
            for (uint8_t i = 0; i < seqChordNoteCount; i++)
            {
                if (seqChordNotes[i] == note)
                {
                    jaNaLista = true;
                    break;
                }
            }
            if (!jaNaLista && seqChordNoteCount < SEQ_MAX_NOTES_PER_STEP)
            {
                seqChordNotes[seqChordNoteCount++] = note;
            }

            sequencer_add_note(seqRecordStartStep, note, seqActiveSymbol, seqCurrentHoldDuration);
            seqChordRecorded = true;
            uiDirty = true;
            uiLastKeyEventMs = millis();
        }
    }
    else if (code == 0x80)
    {
        audio_handle_key(0x80, note, seqActiveSymbol);

        if (seqHeldKeys[note])
        {
            seqHeldKeys[note] = false;
            if (seqHeldCount > 0)
            {
                seqHeldCount--;
            }

            // Quando todas as teclas da nota/acorde forem soltas, avança para o slot seguinte à sustentação
            if (seqHeldCount == 0 && seqChordRecorded && !sequencer_is_playing())
            {
                seqChordRecorded = false;
                uint8_t totalSteps = sequencer_get_total_steps();
                seqCursorStep = (seqRecordStartStep + seqCurrentHoldDuration) % totalSteps;
                seqCurrentHoldDuration = 1;
                seqChordNoteCount = 0;
                uiDirty = true;
                uiLastKeyEventMs = 0; // Atualiza logo que o acorde é concluído
            }
        }
    }
}

void ui_update()
{
    unsigned long agora = millis();

    // Atualiza barra de progresso durante gravação de áudio (até 6s)
    if (telaAtual == Tela::GRAVANDO && recorder_is_recording())
    {
        if (agora - ultimoUpdateGravandoMs >= 200)
        {
            ultimoUpdateGravandoMs = agora;
            desenharGravando();
        }
    }

    // Atualiza playhead e término automático do preview no editor de recorte
    if (telaAtual == Tela::EDITAR_GRAVACAO && trimPreviewPlaying)
    {
        if (agora - trimPreviewStartMs >= trimPreviewDurationMs)
        {
            trim_stop_preview();
            desenharEditarGravacao();
        }
        else if (agora - ultimoUpdatePreviewMs >= 90)
        {
            ultimoUpdatePreviewMs = agora;
            desenharEditarGravacao();
        }
    }

    // Estende a sustentação da nota/acorde conforme a duração em ms de cada slot (fração do BPM)
    if (telaAtual == Tela::COMPOSITOR && !sequencer_is_playing() && seqHeldCount > 0 && seqChordRecorded)
    {
        uint32_t slotMs = sequencer_get_step_duration_ms();
        uint8_t totalSteps = sequencer_get_total_steps();

        while (slotMs > 0 && (agora - seqHoldStepStartMs) >= slotMs)
        {
            seqHoldStepStartMs += slotMs;

            if (seqRecordStartStep + seqCurrentHoldDuration < totalSteps)
            {
                seqCurrentHoldDuration++;
                for (uint8_t i = 0; i < seqChordNoteCount; i++)
                {
                    uint8_t n = seqChordNotes[i];
                    if (seqHeldKeys[n])
                    {
                        sequencer_set_note_duration(seqRecordStartStep, n, seqActiveSymbol, seqCurrentHoldDuration);
                    }
                }
                seqCursorStep = seqRecordStartStep + seqCurrentHoldDuration - 1;
                uiDirty = true;
                uiLastKeyEventMs = 0;
            }
            else
            {
                break;
            }
        }
    }

    // Atualiza o OLED de forma diferida após a varredura completa da matriz do teclado
    if (uiDirty && (uiLastKeyEventMs == 0 || (agora - uiLastKeyEventMs >= UI_KEY_REDRAW_DEBOUNCE_MS)))
    {
        ui_redraw_current();
    }

    bool joystickPressionado = digitalRead(BOTAO_JOYSTICK) == LOW;
    bool botaoBPressionado = digitalRead(BOTAO_B) == LOW;
    bool botaoAPressionado = digitalRead(BOTAO_A) == LOW;

    static unsigned long ultimaAcaoBotao = 0;

    if (agora - ultimaAcaoBotao < 50)
    {
        return;
    }

    bool okEdge = joystickPressionado && !joystickPressionadoAntes;
    bool aEdge = botaoAPressionado && !botaoAPressionadoAntes;
    bool bEdge = botaoBPressionado && !botaoBPressionadoAntes;

    joystickPressionadoAntes = joystickPressionado;
    botaoBPressionadoAntes = botaoBPressionado;
    botaoAPressionadoAntes = botaoAPressionado;

    // ===== TRATAMENTO DE BOTÕES POR TELA =====
    if (okEdge)
    {
        ultimaAcaoBotao = agora;

        if (telaAtual == Tela::MENU)
        {
            switch (opcaoSelecionada)
            {
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
            case 3:
                ui_show_loading("Carregando samples...");
                sequencer_ensure_samples_loaded();
                seqFocusTopBar = false;
                seqEditingBpm = false;
                resetComposerHeldKeys();
                telaAtual = Tela::COMPOSITOR;
                desenharCompositor();
                break;
            }
            return;
        }
        else if (telaAtual == Tela::TECLADO)
        {
            if (quantidadeInstrumentos > 0)
            {
                instrumentoAtivo = instrumentoSelecionado;
                instrumentoConfirmado = true;
                ultimaNotaTeclado = 0;
                desenharModoTeclado();
            }
            return;
        }
        else if (telaAtual == Tela::EDITAR_GRAVACAO)
        {
            trim_save_clip();
            return;
        }
        else if (telaAtual == Tela::COMPOSITOR)
        {
            if (seqFocusTopBar)
            {
                switch (seqTopBarItem)
                {
                case 0: // Play / Stop
                    resetComposerHeldKeys();
                    sequencer_toggle_play();
                    desenharCompositor();
                    break;
                case 1: // Cicla símbolo ativo
                    seqActiveSymbol = (seqActiveSymbol + 1) % SEQ_NUM_SYMBOLS;
                    sequencer_preview_note(60, seqActiveSymbol);
                    desenharCompositor();
                    break;
                case 2: // Edita BPM
                    seqEditingBpm = !seqEditingBpm;
                    desenharCompositor();
                    break;
                case 3: // Alterna compasso 4/4 <-> 3/4
                    sequencer_toggle_time_signature();
                    desenharCompositor();
                    break;
                case 4: // Configurações do sequenciador
                    resetComposerHeldKeys();
                    sequencer_stop();
                    seqEditingBpm = false;
                    seqStatusMsg[0] = '\0';
                    telaAtual = Tela::SEQ_CONFIG;
                    desenharSeqConfig();
                    break;
                }
            }
            else if (!sequencer_is_playing())
            {
                // Botão central do joystick na partitura apaga todas as notas do slot atual
                sequencer_clear_step(seqCursorStep);
                resetComposerHeldKeys();
                desenharCompositor();
            }
            else
            {
                resetComposerHeldKeys();
                sequencer_stop();
                desenharCompositor();
            }
            return;
        }
        else if (telaAtual == Tela::SEQ_CONFIG)
        {
            seqStatusMsg[0] = '\0';
            switch (seqConfigSel)
            {
            case 0: // Config. Símbolos
                telaAtual = Tela::SEQ_SIMBOLOS;
                desenharSeqSimbolos();
                break;
            case 1: // Compasso 4/4 <-> 3/4
                sequencer_toggle_time_signature();
                desenharSeqConfig();
                break;
            case 2: // Número de compassos 2/4/8
                sequencer_cycle_num_bars();
                desenharSeqConfig();
                break;
            case 3: // Salvar Música no SD
            {
                ui_show_loading("Salvando musica...");
                char savedName[SEQ_FILENAME_LEN];
                if (sequencer_save_current_song(savedName, sizeof(savedName)))
                {
                    snprintf(seqStatusMsg, sizeof(seqStatusMsg), "Salvo: %s", savedName);
                }
                else
                {
                    snprintf(seqStatusMsg, sizeof(seqStatusMsg), "Erro ao salvar!");
                }
                desenharSeqConfig();
                break;
            }
            case 4: // Carregar Música do SD
                storage_scan_songs();
                seqSongSel = 0;
                seqSongOffset = 0;
                telaAtual = Tela::SEQ_ARQUIVOS;
                desenharSeqArquivos();
                break;
            case 5: // Limpar Partitura
                sequencer_clear_song();
                snprintf(seqStatusMsg, sizeof(seqStatusMsg), "Partitura limpa!");
                desenharSeqConfig();
                break;
            }
            return;
        }
        else if (telaAtual == Tela::SEQ_SIMBOLOS)
        {
            seqSampleSel = 0;
            seqSampleOffset = 0;
            telaAtual = Tela::SEQ_ESCOLHER_SAMPLE;
            desenharSeqEscolherSample();
            return;
        }
        else if (telaAtual == Tela::SEQ_ESCOLHER_SAMPLE)
        {
            if (quantidadeInstrumentos > 0 && instrumentos != nullptr && seqSampleSel < quantidadeInstrumentos)
            {
                ui_show_loading("Carregando WAV...");
                if (sequencer_assign_symbol_sample(seqSymbolSel, instrumentos[seqSampleSel]))
                {
                    sequencer_preview_note(60, seqSymbolSel);
                }
            }
            telaAtual = Tela::SEQ_SIMBOLOS;
            desenharSeqSimbolos();
            return;
        }
        else if (telaAtual == Tela::SEQ_ARQUIVOS)
        {
            if (storage_get_song_count() > 0)
            {
                ui_show_loading("Abrindo musica...");
                sequencer_load_song_by_index(seqSongSel);
                seqCursorStep = 0;
                seqScrollStep = 0;
                seqFocusTopBar = false;
                resetComposerHeldKeys();
                telaAtual = Tela::COMPOSITOR;
                desenharCompositor();
            }
            return;
        }
    }

    if (aEdge)
    {
        ultimaAcaoBotao = agora;

        if (telaAtual == Tela::GRAVAR)
        {
            recorder_start();
            ultimoUpdateGravandoMs = agora;
            telaAtual = Tela::GRAVANDO;
            desenharGravando();
            return;
        }
        else if (telaAtual == Tela::GRAVANDO)
        {
            // Para gravação antecipadamente e segue para o editor de forma de onda
            recorder_stop();
            return;
        }
        else if (telaAtual == Tela::EDITAR_GRAVACAO)
        {
            // Ouve o trecho selecionado
            trim_start_preview();
            desenharEditarGravacao();
            return;
        }
        else if (telaAtual == Tela::SOM_GRAVADO)
        {
            telaAtual = Tela::GRAVAR;
            desenharGravarSons();
            return;
        }
        else if (telaAtual == Tela::COMPOSITOR)
        {
            // Atalho rápido Play / Stop no botão A
            seqEditingBpm = false;
            resetComposerHeldKeys();
            sequencer_toggle_play();
            desenharCompositor();
            return;
        }
    }

    if (bEdge && telaAtual != Tela::MENU)
    {
        ultimaAcaoBotao = agora;

        if (telaAtual == Tela::GRAVANDO)
        {
            recorder_cancel();
            telaAtual = Tela::MENU;
            ui_desenharMenu();
        }
        else if (telaAtual == Tela::EDITAR_GRAVACAO)
        {
            trim_stop_preview();
            sequencer_mark_all_slots_dirty();
            telaAtual = Tela::GRAVAR;
            desenharGravarSons();
        }
        else if (telaAtual == Tela::SEQ_ESCOLHER_SAMPLE)
        {
            telaAtual = Tela::SEQ_SIMBOLOS;
            desenharSeqSimbolos();
        }
        else if (telaAtual == Tela::SEQ_SIMBOLOS || telaAtual == Tela::SEQ_ARQUIVOS)
        {
            telaAtual = Tela::SEQ_CONFIG;
            desenharSeqConfig();
        }
        else if (telaAtual == Tela::SEQ_CONFIG)
        {
            telaAtual = Tela::COMPOSITOR;
            desenharCompositor();
        }
        else if (telaAtual == Tela::COMPOSITOR)
        {
            if (seqEditingBpm)
            {
                seqEditingBpm = false;
                desenharCompositor();
            }
            else if (seqFocusTopBar)
            {
                seqFocusTopBar = false;
                desenharCompositor();
            }
            else
            {
                resetComposerHeldKeys();
                sequencer_stop();
                telaAtual = Tela::MENU;
                ui_desenharMenu();
            }
        }
        else
        {
            telaAtual = Tela::MENU;
            ui_desenharMenu();
        }
        return;
    }

    // ===== NAVEGAÇÃO PELO JOYSTICK (X e Y) =====
    if (telaAtual == Tela::MIDI || telaAtual == Tela::GRAVAR ||
        telaAtual == Tela::GRAVANDO || telaAtual == Tela::SOM_GRAVADO)
    {
        return;
    }

    unsigned long intervalo = (telaAtual == Tela::COMPOSITOR)
                                  ? intervaloJoystickSeq
                                  : ((telaAtual == Tela::EDITAR_GRAVACAO) ? 110 : intervaloJoystickMenu);
    if (agora - ultimoMovimento < intervalo)
    {
        return;
    }

    int valorY = analogRead(JOYSTICK_Y);
    int valorX = analogRead(JOYSTICK_X);

    bool moveDown = (valorY < 500);
    bool moveUp = (valorY > 3000);
    bool moveLeft = (valorX < 500);
    bool moveRight = (valorX > 3000);

    if (!moveDown && !moveUp && !moveLeft && !moveRight)
    {
        return;
    }

    if (telaAtual == Tela::MENU)
    {
        if (moveDown)
        {
            opcaoSelecionada = (opcaoSelecionada + 1) % 4;
            ui_desenharMenu();
            ultimoMovimento = agora;
        }
        else if (moveUp)
        {
            opcaoSelecionada = (opcaoSelecionada + 3) % 4;
            ui_desenharMenu();
            ultimoMovimento = agora;
        }
    }
    else if (telaAtual == Tela::TECLADO)
    {
        if (quantidadeInstrumentos <= 0)
            return;

        if (moveDown)
        {
            if (instrumentoSelecionado < quantidadeInstrumentos - 1)
                instrumentoSelecionado++;
            if (instrumentoSelecionado > keyboard_menu_down)
            {
                list_offset++;
                keyboard_menu_up = list_offset;
                keyboard_menu_down = list_offset + 2;
            }
            instrumentoConfirmado = false;
            desenharModoTeclado();
            ultimoMovimento = agora;
        }
        else if (moveUp)
        {
            if (instrumentoSelecionado > 0)
                instrumentoSelecionado--;
            if (instrumentoSelecionado < keyboard_menu_up)
            {
                list_offset--;
                keyboard_menu_up = list_offset;
                keyboard_menu_down = list_offset + 2;
            }
            instrumentoConfirmado = false;
            desenharModoTeclado();
            ultimoMovimento = agora;
        }
    }
    else if (telaAtual == Tela::EDITAR_GRAVACAO)
    {
        const uint32_t stepSamples = 2048; // 0.125 s por passo
        const uint32_t minLen = (trimTotalSamples < 4096) ? trimTotalSamples : 4096;
        const uint32_t maxLen = (trimTotalSamples < TAMANHO_BUFFER) ? trimTotalSamples : TAMANHO_BUFFER;
        bool changed = false;

        if (moveRight)
        {
            uint32_t maxStart = (trimTotalSamples > trimLen) ? (trimTotalSamples - trimLen) : 0;
            if (trimStart < maxStart)
            {
                trimStart = (trimStart + stepSamples <= maxStart) ? (trimStart + stepSamples) : maxStart;
                changed = true;
            }
        }
        else if (moveLeft)
        {
            if (trimStart > 0)
            {
                trimStart = (trimStart >= stepSamples) ? (trimStart - stepSamples) : 0;
                changed = true;
            }
        }
        else if (moveUp)
        {
            if (trimLen < maxLen)
            {
                trimLen = (trimLen + stepSamples <= maxLen) ? (trimLen + stepSamples) : maxLen;
                if (trimStart + trimLen > trimTotalSamples)
                {
                    trimStart = trimTotalSamples - trimLen;
                }
                changed = true;
            }
        }
        else if (moveDown)
        {
            if (trimLen > minLen)
            {
                trimLen = (trimLen >= minLen + stepSamples) ? (trimLen - stepSamples) : minLen;
                changed = true;
            }
        }

        if (changed)
        {
            trim_stop_preview();
            desenharEditarGravacao();
            ultimoMovimento = agora;
        }
    }
    else if (telaAtual == Tela::COMPOSITOR)
    {
        uint8_t totalSteps = sequencer_get_total_steps();

        if (seqEditingBpm)
        {
            if (moveUp)
            {
                sequencer_change_bpm(+5);
                desenharCompositor();
                ultimoMovimento = agora;
            }
            else if (moveDown)
            {
                sequencer_change_bpm(-5);
                desenharCompositor();
                ultimoMovimento = agora;
            }
            return;
        }

        if (seqFocusTopBar)
        {
            if (moveRight)
            {
                seqTopBarItem = (seqTopBarItem + 1) % 5;
                desenharCompositor();
                ultimoMovimento = agora;
            }
            else if (moveLeft)
            {
                seqTopBarItem = (seqTopBarItem + 4) % 5;
                desenharCompositor();
                ultimoMovimento = agora;
            }
            else if (moveDown)
            {
                seqFocusTopBar = false;
                desenharCompositor();
                ultimoMovimento = agora;
            }
        }
        else
        {
            if (moveRight)
            {
                resetComposerHeldKeys();
                if (seqCursorStep < totalSteps - 1)
                    seqCursorStep++;
                else
                    seqCursorStep = 0;
                desenharCompositor();
                ultimoMovimento = agora;
            }
            else if (moveLeft)
            {
                resetComposerHeldKeys();
                if (seqCursorStep > 0)
                    seqCursorStep--;
                else
                    seqCursorStep = totalSteps - 1;
                desenharCompositor();
                ultimoMovimento = agora;
            }
            else if (moveUp)
            {
                // Sobe direto para a barra superior (Play, Símbolo, BPM, Compasso, Config)
                resetComposerHeldKeys();
                seqFocusTopBar = true;
                desenharCompositor();
                ultimoMovimento = agora;
            }
        }
    }
    else if (telaAtual == Tela::SEQ_CONFIG)
    {
        if (moveDown && seqConfigSel < SEQ_CONFIG_ITEMS - 1)
        {
            seqConfigSel++;
            if (seqConfigSel >= seqConfigOffset + 4)
                seqConfigOffset = seqConfigSel - 3;
            desenharSeqConfig();
            ultimoMovimento = agora;
        }
        else if (moveUp && seqConfigSel > 0)
        {
            seqConfigSel--;
            if (seqConfigSel < seqConfigOffset)
                seqConfigOffset = seqConfigSel;
            desenharSeqConfig();
            ultimoMovimento = agora;
        }
    }
    else if (telaAtual == Tela::SEQ_SIMBOLOS)
    {
        if (moveDown && seqSymbolSel < SEQ_NUM_SYMBOLS - 1)
        {
            seqSymbolSel++;
            desenharSeqSimbolos();
            ultimoMovimento = agora;
        }
        else if (moveUp && seqSymbolSel > 0)
        {
            seqSymbolSel--;
            desenharSeqSimbolos();
            ultimoMovimento = agora;
        }
    }
    else if (telaAtual == Tela::SEQ_ESCOLHER_SAMPLE)
    {
        if (quantidadeInstrumentos <= 0)
            return;

        if (moveDown && seqSampleSel < quantidadeInstrumentos - 1)
        {
            seqSampleSel++;
            if (seqSampleSel >= seqSampleOffset + 4)
                seqSampleOffset = seqSampleSel - 3;
            desenharSeqEscolherSample();
            ultimoMovimento = agora;
        }
        else if (moveUp && seqSampleSel > 0)
        {
            seqSampleSel--;
            if (seqSampleSel < seqSampleOffset)
                seqSampleOffset = seqSampleSel;
            desenharSeqEscolherSample();
            ultimoMovimento = agora;
        }
    }
    else if (telaAtual == Tela::SEQ_ARQUIVOS)
    {
        int totalSongs = storage_get_song_count();
        if (totalSongs <= 0)
            return;

        if (moveDown && seqSongSel < totalSongs - 1)
        {
            seqSongSel++;
            if (seqSongSel >= seqSongOffset + 4)
                seqSongOffset = seqSongSel - 3;
            desenharSeqArquivos();
            ultimoMovimento = agora;
        }
        else if (moveUp && seqSongSel > 0)
        {
            seqSongSel--;
            if (seqSongSel < seqSongOffset)
                seqSongOffset = seqSongSel;
            desenharSeqArquivos();
            ultimoMovimento = agora;
        }
    }
}

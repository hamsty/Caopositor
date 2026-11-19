#include "storage.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>
#include <string.h>

static const byte cabecalhoWAV[44] = {
    'R', 'I', 'F', 'F',
    0x24, 0x80, 0x00, 0x00, // Tamanho do arquivo - 8 bytes (36 + 32768 = 32804)
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00, // Tamanho do chunk 'fmt ' (16 bytes para PCM)
    0x01, 0x00,             // Formato do áudio (1 = PCM)
    0x01, 0x00,             // Número de canais (1 = Mono)
    0x00, 0x40, 0x00, 0x00, // Taxa de amostragem (16384 Hz em Little Endian)
    0x00, 0x40, 0x00, 0x00, // Byte Rate (SampleRate * Canais * Bits/8)
    0x01, 0x00,             // Block Align (1 byte por frame)
    0x08, 0x00,             // Bits por amostra (8 bits)
    'd', 'a', 't', 'a',
    0x00, 0x80, 0x00, 0x00 // Tamanho exato dos dados de áudio (32768 bytes)
};

static char *menuSamples[MAX_SAMPLES_MENU] = {nullptr};
static int totalSamplesSD = 0;
static int recordedSampleIndex = 0;

static char songMenu[SEQ_MAX_SONGS_MENU][SEQ_FILENAME_LEN];
static int totalSongsSD = 0;
static int savedSongIndex = 0;

static uint32_t encontrarInicioDados(File &arquivo)
{
  arquivo.seek(0);
  char buffer[4];

  while (arquivo.available() >= 4)
  {
    buffer[0] = arquivo.read();

    if (buffer[0] == 'd')
    {
      buffer[1] = arquivo.read();
      buffer[2] = arquivo.read();
      buffer[3] = arquivo.read();

      if (buffer[1] == 'a' && buffer[2] == 't' && buffer[3] == 'a')
      {
        arquivo.seek(arquivo.position() + 4);
        return arquivo.position();
      }

      arquivo.seek(arquivo.position() - 3);
    }
  }
  return 44;
}

bool storage_init()
{
  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);

  SPI.setMISO(MISO);
  SPI.setMOSI(MOSI);
  SPI.setSCK(SCK);

  SPI.begin();

  if (!SD.begin(SS, SPI))
  {
    Serial.println("SD card initialization failed!");
    return false;
  }

  storage_scan_samples();
  storage_scan_songs();
  Serial.println("SD card initialized.");
  return true;
}

void storage_scan_samples()
{
  for (int i = 0; i < totalSamplesSD; i++)
  {
    delete[] menuSamples[i];
    menuSamples[i] = nullptr;
  }
  totalSamplesSD = 0;

  File raiz = SD.open("/");
  if (!raiz)
  {
    Serial.println("Erro: Nao foi possivel abrir a raiz do SD.");
    return;
  }

  while (true)
  {
    File ficheiro = raiz.openNextFile();
    if (!ficheiro)
    {
      break;
    }

    if (!ficheiro.isDirectory())
    {
      String nomeFicheiro = ficheiro.name();
      String nomeMaiusculo = nomeFicheiro;
      nomeMaiusculo.toUpperCase();

      if (nomeMaiusculo.endsWith(".WAV"))
      {
        String nomeSemExtensao = nomeFicheiro.substring(0, nomeFicheiro.length() - 4);

        if (totalSamplesSD < MAX_SAMPLES_MENU)
        {
          menuSamples[totalSamplesSD] = new char[nomeSemExtensao.length() + 1];
          strcpy(menuSamples[totalSamplesSD], nomeSemExtensao.c_str());
          totalSamplesSD++;
        }
        if (nomeSemExtensao.indexOf("recsample") != -1)
        {
          String indiceStr = nomeSemExtensao.substring(9);
          int indice = indiceStr.toInt();
          if (indice > recordedSampleIndex)
          {
            recordedSampleIndex = indice;
          }
        }
      }
    }
    ficheiro.close();
  }
  raiz.close();

  Serial.print("Encontrados ");
  Serial.print(totalSamplesSD);
  Serial.println(" samples WAV no SD:");

  for (int i = 0; i < totalSamplesSD; i++)
  {
    Serial.print(i + 1);
    Serial.print("- ");
    Serial.println(menuSamples[i]);
  }
}

bool storage_load_sample(const char *nomeArquivo, int8_t *destBuffer, volatile uint32_t *outLength)
{
  if (nomeArquivo == nullptr || destBuffer == nullptr)
  {
    return false;
  }

  File arquivoWav = SD.open(nomeArquivo);

  if (arquivoWav)
  {
    uint32_t tamanhoCabecalho = encontrarInicioDados(arquivoWav);
    Serial.print("Tamanho do cabecalho detetado: ");
    Serial.println(tamanhoCabecalho);
    arquivoWav.seek(tamanhoCabecalho);

    uint32_t idx = 0;

    while (arquivoWav.available() && idx < TAMANHO_BUFFER)
    {
      destBuffer[idx] = arquivoWav.read() - 128;
      idx++;
    }

    // Zera o restante do buffer para evitar resíduo de samples anteriores
    if (idx < TAMANHO_BUFFER)
    {
      memset(destBuffer + idx, 0, TAMANHO_BUFFER - idx);
    }

    if (outLength != nullptr)
    {
      *outLength = idx;
    }

    arquivoWav.close();
    Serial.print("Sample carregado com sucesso: ");
    Serial.println(nomeArquivo);
    return true;
  }

  Serial.print("Erro ao abrir o arquivo no SD: ");
  Serial.println(nomeArquivo);
  return false;
}

bool storage_save_wav(const char *nomeArquivo, const int8_t *srcBuffer, uint32_t length)
{
  if (SD.exists(nomeArquivo))
  {
    SD.remove(nomeArquivo);
  }

  File arquivoWav = SD.open(nomeArquivo, FILE_WRITE);
  if (arquivoWav)
  {
    // Monta cabeçalho WAV com o tamanho exato do trecho selecionado
    byte header[44];
    memcpy(header, cabecalhoWAV, 44);
    uint32_t riffSize = length + 36;
    header[4] = (byte)(riffSize & 0xFF);
    header[5] = (byte)((riffSize >> 8) & 0xFF);
    header[6] = (byte)((riffSize >> 16) & 0xFF);
    header[7] = (byte)((riffSize >> 24) & 0xFF);
    header[40] = (byte)(length & 0xFF);
    header[41] = (byte)((length >> 8) & 0xFF);
    header[42] = (byte)((length >> 16) & 0xFF);
    header[43] = (byte)((length >> 24) & 0xFF);

    arquivoWav.write(header, 44);

    // Escrita em blocos de 512 bytes para maior velocidade no SD
    uint8_t chunk[512];
    uint32_t pos = 0;
    while (pos < length)
    {
      uint32_t n = (length - pos < sizeof(chunk)) ? (length - pos) : sizeof(chunk);
      for (uint32_t i = 0; i < n; i++)
      {
        chunk[i] = (uint8_t)(srcBuffer[pos + i] + 128);
      }
      arquivoWav.write(chunk, n);
      pos += n;
    }

    arquivoWav.close();
    Serial.println("Gravacao salva com sucesso no SD!");
    storage_scan_samples();
    return true;
  }

  Serial.println("Erro ao salvar arquivo no SD.");
  return false;
}

void storage_test_spi()
{
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

  for (int i = 0; i < 10; i++)
  {
    SPI.transfer(0xFF);
  }

  digitalWrite(SS, LOW);
  delay(1);

  char cmd0[6] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
  for (int i = 0; i < 6; i++)
  {
    SPI.transfer(cmd0[i]);
  }

  char resposta = 0xFF;
  int tentativas = 0;

  while (resposta == 0xFF && tentativas < 100)
  {
    resposta = SPI.transfer(0xFF);
    tentativas++;
  }

  digitalWrite(SS, HIGH);
  SPI.endTransaction();

  Serial.print("Resposta recebida: 0x");
  if (resposta < 16)
    Serial.print("0");
  Serial.println(resposta, HEX);

  if (resposta == 0x01)
  {
    Serial.println("\n-> SUCESSO ABSOLUTO!");
    Serial.println("O cartao SD esta se comunicando perfeitamente pelo SPI.");
  }
  else if (resposta == 0xFF)
  {
    Serial.println("\n-> FALHA: Nenhuma resposta (0xFF).");
    Serial.println("O pino MISO (RX) nao esta recebendo nada. Verifique MISO, MOSI, SCK e VCC.");
  }
  else
  {
    Serial.println("\n-> FALHA: Resposta irreconhecivel.");
    Serial.println("Os fios podem estar trocados ou ha muito ruido na linha.");
  }
}

const char **storage_get_sample_list()
{
  return (const char **)menuSamples;
}

int storage_get_sample_count()
{
  return totalSamplesSD;
}

int storage_next_recorded_index()
{
  return ++recordedSampleIndex;
}

// ---------- Persistência do Sequenciador (.cseq) ----------

void storage_scan_songs()
{
  totalSongsSD = 0;

  File raiz = SD.open("/");
  if (!raiz)
  {
    return;
  }

  while (true)
  {
    File ficheiro = raiz.openNextFile();
    if (!ficheiro)
    {
      break;
    }

    if (!ficheiro.isDirectory())
    {
      String nomeFicheiro = ficheiro.name();
      String nomeMaiusculo = nomeFicheiro;
      nomeMaiusculo.toUpperCase();

      if (nomeMaiusculo.endsWith(".CSEQ"))
      {
        String nomeSemExt = nomeFicheiro.substring(0, nomeFicheiro.length() - 5);

        if (totalSongsSD < SEQ_MAX_SONGS_MENU)
        {
          strncpy(songMenu[totalSongsSD], nomeSemExt.c_str(), SEQ_FILENAME_LEN - 1);
          songMenu[totalSongsSD][SEQ_FILENAME_LEN - 1] = '\0';
          totalSongsSD++;
        }

        if (nomeSemExt.startsWith("song") || nomeSemExt.startsWith("SONG"))
        {
          int indice = nomeSemExt.substring(4).toInt();
          if (indice > savedSongIndex)
          {
            savedSongIndex = indice;
          }
        }
      }
    }
    ficheiro.close();
  }
  raiz.close();

  Serial.print("Encontradas ");
  Serial.print(totalSongsSD);
  Serial.println(" musicas (.cseq) no SD.");
}

int storage_get_song_count()
{
  return totalSongsSD;
}

const char *storage_get_song_name(int index)
{
  if (index < 0 || index >= totalSongsSD)
  {
    return "";
  }
  return songMenu[index];
}

int storage_next_song_index()
{
  return ++savedSongIndex;
}

bool storage_save_song(const char *nomeArquivo, const SeqSong &song)
{
  if (nomeArquivo == nullptr)
    return false;

  if (SD.exists(nomeArquivo))
  {
    SD.remove(nomeArquivo);
  }

  File f = SD.open(nomeArquivo, FILE_WRITE);
  if (!f)
  {
    Serial.print("Erro ao salvar musica: ");
    Serial.println(nomeArquivo);
    return false;
  }

  f.println("CAOPOSITOR_SEQ_V1");
  f.print("BPM=");
  f.println(song.bpm);
  f.print("BEATS=");
  f.println(song.beatsPerBar);
  f.print("BARS=");
  f.println(song.numBars);

  for (int s = 0; s < SEQ_NUM_SYMBOLS; s++)
  {
    f.print("SYM");
    f.print(s);
    f.print("=");
    f.println(song.symbolFiles[s]);
  }

  for (int step = 0; step < SEQ_MAX_STEPS; step++)
  {
    for (int slot = 0; slot < SEQ_MAX_NOTES_PER_STEP; slot++)
    {
      const SeqNote &n = song.steps[step][slot];
      if (n.midiNote != 0)
      {
        uint8_t dur = (n.duration >= 1) ? n.duration : 1;
        f.print("N ");
        f.print(step);
        f.print(" ");
        f.print(n.midiNote);
        f.print(" ");
        f.print(n.symbolId);
        f.print(" ");
        f.println(dur);
      }
    }
  }

  f.close();
  storage_scan_songs();
  Serial.print("Musica salva com sucesso: ");
  Serial.println(nomeArquivo);
  return true;
}

bool storage_load_song(const char *nomeArquivo, SeqSong &song)
{
  if (nomeArquivo == nullptr)
    return false;

  File f = SD.open(nomeArquivo);
  if (!f)
  {
    Serial.print("Erro ao abrir musica: ");
    Serial.println(nomeArquivo);
    return false;
  }

  // Zera estrutura antes de carregar
  song.bpm = SEQ_DEFAULT_BPM;
  song.beatsPerBar = 4;
  song.numBars = 4;
  for (int s = 0; s < SEQ_NUM_SYMBOLS; s++)
  {
    song.symbolFiles[s][0] = '\0';
  }
  memset(song.steps, 0, sizeof(song.steps));

  while (f.available())
  {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    if (line.startsWith("BPM="))
    {
      int val = line.substring(4).toInt();
      if (val >= SEQ_MIN_BPM && val <= SEQ_MAX_BPM)
        song.bpm = (uint16_t)val;
    }
    else if (line.startsWith("BEATS="))
    {
      int val = line.substring(6).toInt();
      if (val == 3 || val == 4)
        song.beatsPerBar = (uint8_t)val;
    }
    else if (line.startsWith("BARS="))
    {
      int val = line.substring(5).toInt();
      if (val == 2 || val == 4 || val == 8)
        song.numBars = (uint8_t)val;
    }
    else if (line.startsWith("SYM") && line.length() >= 5 && line.charAt(4) == '=')
    {
      int symIdx = line.charAt(3) - '0';
      if (symIdx >= 0 && symIdx < SEQ_NUM_SYMBOLS)
      {
        String fname = line.substring(5);
        fname.trim();
        strncpy(song.symbolFiles[symIdx], fname.c_str(), SEQ_FILENAME_LEN - 1);
        song.symbolFiles[symIdx][SEQ_FILENAME_LEN - 1] = '\0';
      }
    }
    else if (line.startsWith("N "))
    {
      int step = -1, midi = 0, sym = 0, dur = 1;
      int parsed = sscanf(line.c_str(), "N %d %d %d %d", &step, &midi, &sym, &dur);
      if (parsed >= 3)
      {
        if (dur < 1)
          dur = 1;
        if (dur > SEQ_MAX_STEPS)
          dur = SEQ_MAX_STEPS;

        if (step >= 0 && step < SEQ_MAX_STEPS && midi > 0 && midi <= 127 && sym >= 0 && sym < SEQ_NUM_SYMBOLS)
        {
          for (int slot = 0; slot < SEQ_MAX_NOTES_PER_STEP; slot++)
          {
            if (song.steps[step][slot].midiNote == 0)
            {
              song.steps[step][slot].midiNote = (uint8_t)midi;
              song.steps[step][slot].symbolId = (uint8_t)sym;
              song.steps[step][slot].duration = (uint8_t)dur;
              break;
            }
          }
        }
      }
    }
  }

  f.close();
  Serial.print("Musica carregada com sucesso: ");
  Serial.println(nomeArquivo);
  return true;
}

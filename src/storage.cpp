#include "storage.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>

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

static char *menuSamples[MAX_SAMPLES_MENU]; // Array para guardar os nomes
static int totalSamplesSD = 0;
static int recordedSampleIndex = 0;

static uint32_t encontrarInicioDados(File &arquivo)
{
  arquivo.seek(0);
  char buffer[4];

  // Procura pela sequência de caracteres "data"
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
        // Pula os 4 bytes seguintes (que guardam o tamanho do bloco de áudio)
        arquivo.seek(arquivo.position() + 4);
        return arquivo.position(); // Retorna o tamanho exato do cabeçalho
      }

      // Se não for "data", volta 3 passos atrás para não saltar letras por engano
      arquivo.seek(arquivo.position() - 3);
    }
  }
  return 44; // Retorna o padrão 44 se o bloco não for encontrado
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
  Serial.println("SD card initialized.");
  return true;
}

void storage_scan_samples()
{
  totalSamplesSD = 0; // Zera a contagem atual

  File raiz = SD.open("/");
  if (!raiz)
  {
    Serial.println("Erro: Nao foi possivel abrir a raiz do SD.");
    return;
  }

  // Passa por todos os ficheiros da pasta raiz
  while (true)
  {
    File ficheiro = raiz.openNextFile();

    // Se não houver mais ficheiros, sai do loop
    if (!ficheiro)
    {
      break;
    }

    // Ignora se for uma pasta
    if (!ficheiro.isDirectory())
    {
      String nomeFicheiro = ficheiro.name();
      String nomeMaiusculo = nomeFicheiro;
      nomeMaiusculo.toUpperCase(); // Facilita a verificação (.WAV ou .wav)

      // Verifica se é um ficheiro de áudio WAV
      if (nomeMaiusculo.endsWith(".WAV"))
      {
        // Corta os últimos 4 caracteres (".wav")
        String nomeSemExtensao = nomeFicheiro.substring(0, nomeFicheiro.length() - 4);

        // Guarda no array do menu se ainda houver espaço
        if (totalSamplesSD < MAX_SAMPLES_MENU)
        {
          menuSamples[totalSamplesSD] = new char[nomeSemExtensao.length() + 1];
          strcpy(menuSamples[totalSamplesSD], nomeSemExtensao.c_str());
          totalSamplesSD++;
        }
        if (nomeSemExtensao.indexOf("recsample") != -1)
        {
          // Se o nome do ficheiro contiver "recsample", tenta extrair o índice
          String indiceStr = nomeSemExtensao.substring(9); // Pega a parte após "recsample"
          int indice = indiceStr.toInt();                  // Converte para inteiro
          if (indice > recordedSampleIndex)
          {
            recordedSampleIndex = indice; // Atualiza o índice global se for maior
          }
        }
      }
    }
    ficheiro.close(); // Fecha o ficheiro atual para poupar memória
  }
  raiz.close(); // Fecha o diretório

  // Imprime o resultado no Monitor Serial para teste
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
  File arquivoWav = SD.open(nomeArquivo);

  if (arquivoWav)
  {
    uint32_t tamanhoCabecalho = encontrarInicioDados(arquivoWav);
    Serial.print("Tamanho do cabecalho detetado: ");
    Serial.println(tamanhoCabecalho);
    arquivoWav.seek(tamanhoCabecalho);

    uint32_t idx = 0;

    // Lê os dados brutos até encher o buffer ou o arquivo acabar
    while (arquivoWav.available() && idx < TAMANHO_BUFFER)
    {
      // Lê o byte (0 a 255) e subtrai 128 para centralizar a onda (-128 a 127)
      destBuffer[idx] = arquivoWav.read() - 128;
      idx++;
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

  Serial.println("Erro ao abrir o arquivo no SD!");
  return false;
}

bool storage_save_wav(const char *nomeArquivo, const int8_t *srcBuffer, uint32_t length)
{
  // Remove o arquivo antigo se existir com o mesmo nome
  if (SD.exists(nomeArquivo))
  {
    SD.remove(nomeArquivo);
  }

  File arquivoWav = SD.open(nomeArquivo, FILE_WRITE);
  if (arquivoWav)
  {
    // 1. Escreve os 44 bytes do cabeçalho
    arquivoWav.write(cabecalhoWAV, 44);

    // 2. Escreve os bytes de áudio convertendo para sem sinal (WAV 8-bit padrão)
    for (uint32_t i = 0; i < length; i++)
    {
      byte sampleSemSinal = srcBuffer[i] + 128;
      arquivoWav.write(sampleSemSinal);
    }

    arquivoWav.close();
    Serial.println("Gravacao salva com sucesso no SD!");
    storage_scan_samples(); // Atualiza o menu com o novo arquivo
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

  digitalWrite(SS, LOW); // Abaixa o CS para selecionar o cartão
  delay(1);

  char cmd0[6] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
  for (int i = 0; i < 6; i++)
  {
    SPI.transfer(cmd0[i]);
  }

  // Aguarda a resposta do cartão
  char resposta = 0xFF;
  int tentativas = 0;

  while (resposta == 0xFF && tentativas < 100)
  {
    resposta = SPI.transfer(0xFF);
    tentativas++;
  }

  digitalWrite(SS, HIGH); // Libera o cartão
  SPI.endTransaction();

  // Analisa o resultado
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

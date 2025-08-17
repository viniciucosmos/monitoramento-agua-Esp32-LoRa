#include <RadioLib.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>

//-------- Configurações de Wi-fi -----------
const char* ssid = "********";           // Nome
const char* password = "********";      // Senha

// Definição dos pinos SPI e LoRa
#define LORA_CLK SCK   // Pino de clock SPI
#define LORA_MISO MISO // Pino MISO SPI
#define LORA_MOSI MOSI // Pino MOSI SPI
#define LORA_NSS SS    // Chip Select (CS)

// Configurações LoRa
#define SF 7        // Spreading Factor (7-12)
#define CR 5        // Coding Rate (4/5)
#define BAND 915.0  // Frequência LoRa (MHz)
#define TX_POWER 20 // Potência de Transmissão (dBm)

// Criação do objeto LoRa SX1262 - Ordem: NSS, DIO1, RESET, BUSY
SX1262 Lora = new Module(8, 14, 12, 13);

// Controle por interrupção
volatile bool transmitFlag = false;
volatile bool enableInterrupt = true;

// Interrupção chamada ao receber um pacote
void setFlag(void)
{
  if (!enableInterrupt)
    return;
  transmitFlag = true;
}

void setup()
{
  Serial.begin(115200);

  //:::::: WIFI :::::://

  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");

  // Inicializa SPI
  SPI.begin(LORA_CLK, LORA_MISO, LORA_MOSI, LORA_NSS);

  // Inicializa módulo LoRa
  Serial.print("Iniciando LoRa... ");
  int state = Lora.begin(BAND);

  if (state == RADIOLIB_ERR_NONE)
  {
    Serial.println("Sucesso!");
  }
  else
  {
    Serial.print("Falha, código: ");
    Serial.println(state);
    while (true)
      ; // trava o código se falhar
  }

  // Configura parâmetros LoRa
  Lora.setSpreadingFactor(SF);
  Lora.setCodingRate(CR);
  Lora.setOutputPower(TX_POWER);

  // Configura interrupção para recepção
  Lora.setDio1Action(setFlag);

  // Inicia recepção contínua
  Serial.print("Iniciando recepção... ");
  state = Lora.startReceive();
  if (state == RADIOLIB_ERR_NONE)
  {
    Serial.println("Sucesso!");
  }
  else
  {
    Serial.print("Falha, código: ");
    Serial.println(state);
    while (true)
      ;
  }
}

void loop()
{
  // Se um pacote foi recebido
  if (transmitFlag)
  {
    transmitFlag = false;
    enableInterrupt = false; // Evita conflito durante leitura

    // Tenta ler os dados recebidos
    String receivedData;
    // String receivedDataPH;
    // String receivedDataTDS;

    int state = Lora.readData(receivedData);
    // int statePH = Lora.readData(receivedDataPH);
    // int stateTDS = Lora.readData(receivedDataTDS);

    if (state == RADIOLIB_ERR_NONE)
    {
      Serial.println("\n====================================================");

      Serial.println("::: Dados Recebidos :::");
      Serial.println(receivedData);
      // Serial.println(receivedDataPH);
      // Serial.println(receivedDataTDS);

      // Mostra qualidade do sinal
      int16_t rssi = Lora.getRSSI();
      float snr = Lora.getSNR();
      Serial.print("RSSI: ");
      Serial.print(rssi);
      Serial.print(" dBm | SNR: ");
      Serial.print(snr);
      Serial.println(" dB");

      // Processa o conteúdo da mensagem
    }
    else
    {
      Serial.println("Erro ao receber dados!");
    }

    // Reinicia recepção
    Lora.startReceive();
    enableInterrupt = true;
  }
}

// Função para tratar os dados recebidos
void processReceivedData(const String &data, int rssi, float snr)
{
  int tempIndex = data.indexOf("Temperatura");

  if (tempIndex != -1)
  {
    // Encontra o valor após os dois pontos
    int tempValueStart = data.indexOf(":", tempIndex) + 1;
    String temperatura = data.substring(tempValueStart);
    temperatura.trim(); // Remove espaços extras

    Serial.println("Temperatura interpretada: " + temperatura + " °C");
  }
  else
  {
    Serial.println("Formato inesperado dos dados recebidos.");
  }
}

#include <RadioLib.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

//-------- Configurações de Wi-fi -----------
const char *ssid = "x";           // Nome
const char *password = "x"; // Senha

// URL da API
const char *serverUrl = "url";

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

  Serial.println("Conectando ao WiFi...");
  WiFi.begin(ssid, password);

  // Espera até conectar
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP()); // Mostra IP na rede

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
    while (true);
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
    int state = Lora.readData(receivedData);

    if (state == RADIOLIB_ERR_NONE)
    {
      Serial.println("\n====================================================");

      Serial.println("::: Dados Recebidos :::");
      Serial.println(receivedData);

      // Quebra em partes usando vírgula como separador
      int firstComma = receivedData.indexOf(',');
      int secondComma = receivedData.indexOf(',', firstComma + 1);

      String tempStr = receivedData.substring(0, firstComma);
      String phStr = receivedData.substring(firstComma + 1, secondComma);
      String tdsStr = receivedData.substring(secondComma + 1);

      float temperatureValue = tempStr.toFloat();
      float phValue = phStr.toFloat();
      float tdsValue = tdsStr.toFloat();
      Serial.println("");
      Serial.println("Dados: ");
      Serial.println("Temperatura: " + String(temperatureValue));
      Serial.println("pH: " + String(phValue));
      Serial.println("TDS: " + String(tdsValue));
      Serial.println("");

      // Monta JSON
      String jsonData = "{";
      jsonData += "\"temperature\":" + String(temperatureValue, 2) + ",";
      jsonData += "\"ph\":" + String(phValue, 2) + ",";
      jsonData += "\"tds\":" + String(tdsValue, 2);
      jsonData += "}";

      // Envia para API
      if (WiFi.status() == WL_CONNECTED)
      {
        HTTPClient http;

        http.begin(serverUrl);                              // Endpoint da API
        http.addHeader("Content-Type", "application/json"); // Define header

        int httpResponseCode = http.POST(jsonData); // Envia POST com JSON

        if (httpResponseCode > 0)
        {
          Serial.print("Resposta da API: ");
          Serial.println(httpResponseCode);
          Serial.println(http.getString()); // Corpo da resposta
        }
        else
        {
          Serial.print("Erro ao enviar POST. Código: ");
          Serial.println(httpResponseCode);
        }

        http.end(); // Fecha conexão
      }
      else
      {
        Serial.println("WiFi desconectado, tentando reconectar...");
        WiFi.begin(ssid, password);
      }

      delay(5000); // Envia a cada 5 segundos
    }

    // Mostra qualidade do sinal
    Serial.println("Informações Trasnmissão");
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

void dadosFormatados()
{
}

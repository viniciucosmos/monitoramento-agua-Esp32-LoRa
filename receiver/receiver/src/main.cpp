#include <RadioLib.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include "heltec.h"

//-------- Configurações de Wi-fi -----------
const char *ssid = ".exe";           
const char *password = "sextasexta"; 

// Configurações da API Django 
const char *serverUrl = "https://monitoramento-agua-django-production.up.railway.app/monitoramento/leituras/nova/";

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
    while (true)
      ;
  }
  // Ativa Display, LoRa e Serial
  Heltec.begin(true /*Display ON*/, false /*LoRa ON*/, true /*Serial ON*/);

  // Mostra mensagem inicial
  Heltec.display->clear();
  Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
  Heltec.display->setFont(ArialMT_Plain_16);
  Heltec.display->drawString(64, 20, "LoRa Ativo");
  Heltec.display->display();
}

void loop()
{
  if (transmitFlag)
  {
    transmitFlag = false;
    enableInterrupt = false;

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

      // CORREÇÃO: Manter como float e usar nomes corretos
      float temperatureValue = tempStr.toFloat();
      float phValue = phStr.toFloat();
      float tdsValue = tdsStr.toFloat();
      
      Serial.println("");
      Serial.println("Dados: ");
      Serial.println("Temperatura: " + String(temperatureValue, 2));
      Serial.println("pH: " + String(phValue, 2));
      Serial.println("TDS: " + String(tdsValue, 2));
      Serial.println("");

      String jsonData = "{";
      jsonData += "\"temperatura\":" + String(temperatureValue, 2) + ","; 
      jsonData += "\"ph\":" + String(phValue, 2) + ",";
      jsonData += "\"tds\":" + String(tdsValue, 2);
      jsonData += "}";

      Serial.println("JSON enviado: " + jsonData);

      // Envia para API Django
      if (WiFi.status() == WL_CONNECTED)
      {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");

        int httpResponseCode = http.POST(jsonData);

        if (httpResponseCode > 0)
        {
          Serial.print("Resposta da API Django: ");
          Serial.println(httpResponseCode);
          String response = http.getString();
          Serial.println("Resposta: " + response);
        }
        else
        {
          Serial.print("Erro ao enviar POST. Código: ");
          Serial.println(httpResponseCode);
        }

        http.end();
      }
      else
      {
        Serial.println("WiFi desconectado, tentando reconectar...");
        WiFi.begin(ssid, password);
      }

      delay(20000);
    }
    else
    {
      Serial.println("Erro ao receber dados!");
      delay(5000);
    }

    Lora.startReceive();
    enableInterrupt = true;

    // Display code...
  }
    // Aqui você pode manter o display fixo ou fazer piscar para dar destaque
    static bool on = true;
    Heltec.display->clear();
    Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
    Heltec.display->setFont(ArialMT_Plain_16);

    if (on)
    {
      Heltec.display->drawString(64, 20, "/\\_/\\\n( o.o )\n > ^ <");
    }

    Heltec.display->display();
    on = !on;
    delay(1000); // Pisca a cada 1 segundo
}
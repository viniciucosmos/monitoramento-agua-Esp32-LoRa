#include <RadioLib.h>
#include "heltec.h"

//:::::: LORA - INICIO :::::://
// Definição dos pinos SPI e LoRa -> V3 <-
#define LORA_CLK SCK   // Pino de clock SPI
#define LORA_MISO MISO // Pino MISO SPI
#define LORA_MOSI MOSI // Pino MOSI SPI
#define LORA_NSS SS    // Pino de seleção do módulo LoRa (Chip Select)

// Configurações LoRa 
#define SF 7        // Spreading Factor (7-12)
#define CR 5        // Coding Rate (4/5)
#define BAND 915.0  // Frequência LoRa (Brasil: 915 MHz)
#define TX_POWER 20 // Potência de Transmissão (máximo 22 dBm)

// Criação do objeto LoRa SX1262 - Ordem: NSS, DIO1, RESET, BUSY
SX1262 Lora = new Module(8, 14, 12, 13);

//:::::: LORA - FIM  ::::::

//:::::: TDS e TEMP - INICIO :::::://

#include <OneWire.h>           // Biblioteca para comunicação OneWire
#include <DallasTemperature.h> // Biblioteca para sensores DS18B20

// === Configurações do sensor DS18B20 ===
#define ONE_WIRE_BUS 19             // Pino digital conectado ao DS18B20
OneWire oneWire(ONE_WIRE_BUS);       // Cria instância OneWire
DallasTemperature sensors(&oneWire); // Passa o OneWire para a biblioteca DallasTemperature

// === Configurações do sensor TDS ===
#define TdsSensorPin 20
#define VREF 3.3  // Tensão de referência do ADC (normalmente 3.3V para o esp32)
#define SCOUNT 30 // Número de amostras para cálculo da mediana

// === Variáveis globais ===
int analogBuffer[SCOUNT];     // Buffer de leituras analógicas
int analogBufferTemp[SCOUNT]; // Buffer temporário para ordenação
int analogBufferIndex = 0, copyIndex = 0;
float averageVoltage = 0, tdsValue = 0, temperature = 25.0, ecValue = 0;

int getMedianNum(int bArray[], int iFilterLen); // Assinatura da função

//:::::: TDS e TEMP - FIM:::::://

//:::::: pH - INICIO :::::://
#define pinSensorPh 7

int phValue = 0;
float voltage = 0;
float a = -8.33333;
float b = 34.08237;

//:::::: pH - FIM :::::://

void setup()
{
  analogReadResolution(12); // Define resolução de 12 bits (padrão)
  
  //:::::: LORA - INICIO :::::://

  Serial.begin(115200);

  // Inicializa a interface SPI com pinos definidos
  SPI.begin(LORA_CLK, LORA_MISO, LORA_MOSI, LORA_NSS);

  // Inicializa o módulo LoRa
  Serial.print("Iniciando LoRa... ");
  int state = Lora.begin(BAND);

  if (state == RADIOLIB_ERR_NONE)
  {
    Serial.println("Sucesso!");
  }
  else
  {
    Serial.print("Erro de inicialização. Código: ");
    Serial.println(state);
    while (true)
      ; // Para aqui se falhar
  }

  // Configuração dos parâmetros LoRa
  Lora.setSpreadingFactor(SF);
  Lora.setCodingRate(CR);
  Lora.setOutputPower(TX_POWER);

  Serial.println("LoRa configurado e pronto para enviar!");
  //:::::: LORA - FIM :::::://

  //::::::TDS e TEMP - INICIO :::::://

  pinMode(TdsSensorPin, INPUT);
  // Inicializa sensor DS18B20
  sensors.begin();

  //:::::: TDS e TEMP - FIM :::::://

  //:::::: pH - INICIO :::::://

  pinMode(pinSensorPh, INPUT);

  //:::::: pH - FIM :::::://
}

void loop()
{
  //::::::TDS e TEMP - INICIO :::::://

  // Leitura da temperatura do DS18B20
  sensors.requestTemperatures();            // Envia comando para ler a temperatura
  temperature = sensors.getTempCByIndex(0); // Lê a temperatura em °C (assumindo um único sensor)

  // Amostragem do valor analógico do sensor TDS a cada 40 ms
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U)
  {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT)
      analogBufferIndex = 0;
  }

  // Cálculo e impressão dos valores a cada 800 ms
  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U)
  {
    printTimepoint = millis();

    // Copia o buffer de leitura
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++)
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];

    // Calcula a média (mediana) da tensão lida
    averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 4095.0;
    // Compensação de temperatura
    float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
    float compensationVoltage = averageVoltage / compensationCoefficient;

      //condutividade em micro simens
    ecValue = 133.42*pow(compensationVoltage, 3)
        - 255.86*pow(compensationVoltage, 2)
        + 857.39*compensationVoltage; // EC bruto (µS/cm)

    Serial.println("");
    Serial.print("Condutividade (µS/cm): ");
    Serial.println(ecValue);
    
    
    float calibrationFactor = 5.82; //ecValueCalculado
   // float calibrationFactor = 0.840; 
    float ecCalibrated = ecValue * calibrationFactor;   
    tdsValue = ecCalibrated * 0.5; 

 }

  //::::::TDS e TEMP - FIM :::::://

  //::::::pH - INICIO :::::://

  voltage = analogRead(pinSensorPh) * (5.0 / 4095.0); // ADC de 12 bits no ESP32
  phValue = a * voltage + b;
  delay(1000);

  Serial.print("(ph) Tensão: ");
  Serial.println(voltage);
  Serial.println("");

  //:::::: pH - FIM :::::://

  //::::::LORA - INICIO :::::://

  // Monta mensagem como string
  String dados = String(temperature) + "," + String(phValue) + "," + String(tdsValue);

  // Serial.println("TENSAO de saida: ");
  // Serial.println(voltage);
  //  Serial.println("verdadera tensao");
  //  Serial.println(averageVoltage);

  Serial.print("TDS: ");
  Serial.println(tdsValue);
  Serial.print("Temperatura: ");
  Serial.println(temperature);
  Serial.print("pH: ");
  Serial.println(phValue);

  // Serial.println("\nTemp - pH - TDS");
  // Serial.println("Enviando via LoRa: " + dados);

  //Transmite os dados (modo bloqueante)
  int state = Lora.startTransmit(dados);


  if (state == RADIOLIB_ERR_NONE)
  {
    Serial.println("Mensagem enviada com sucesso!");
  }
  else
  {
    Serial.print("Falha no envio. Código: ");
    Serial.println(state);
  }

  // Tempo de espera
  delay(30000);

  //:::::: LORA - FIM :::::://

}

//::::::TDS e TEMP - INICIO :::::://

// Função que calcula a mediana de um array de inteiros
int getMedianNum(int bArray[], int iFilterLen)
{
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++)
  {
    for (i = 0; i < iFilterLen - j - 1; i++)
    {
      if (bTab[i] > bTab[i + 1])
      {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0)
    bTemp = bTab[(iFilterLen - 1) / 2];
  else
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  return bTemp;
}

//:::::: TDS e TEMP - FIM :::::://
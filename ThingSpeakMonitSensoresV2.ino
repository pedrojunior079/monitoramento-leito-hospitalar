/*
 Arduino --> ThingSpeak Channel via Ethernet
 The ThingSpeak Client sketch is designed for the Arduino and Ethernet.
 This sketch updates a channel feed with an analog input reading via the
 ThingSpeak API (https://thingspeak.com/docs)
 using HTTP POST. The Arduino uses DHCP and DNS for a simpler network setup.
 The sketch also includes a Watchdog / Reset function to make sure the
 Arduino stays connected and/or regains connectivity after a network outage.
 Use the Serial Monitor on the Arduino IDE to see verbose network feedback
 and ThingSpeak connectivity status.
 Getting Started with ThingSpeak:
   * Sign Up for New User Account - https://thingspeak.com/users/new
   * Create a new Channel by selecting Channels and then Create New Channel
   * Enter the Write API Key in this sketch under "ThingSpeak Settings"
 Arduino Requirements:
   * Arduino with Ethernet Shield or Arduino Ethernet
   * Arduino 1.0+ IDE
   
  Network Requirements:
   * Ethernet port on Router    
   * DHCP enabled on Router
   * Unique MAC Address for Arduino
 Created: October 17, 2011 by Hans Scharler (http://www.nothans.com)
 Additional Credits:
 Example sketches from Arduino team, Ethernet by Adrian McEwen
*/
#include "DHT.h"
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <BH1750.h>
#include <Ethernet_W5500.h>
#define DHTPIN 6 // pino que estamos conectado
#define DHTTYPE DHT22 // DHT 22
DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter(0x23);

const int chipSelect = 4;

//Define o MAC Address usando o endereco da placa ou
//atribuindo um MAC manualmente
#if defined(WIZ550io_WITH_MACADDRESS)
;
#else
byte mac[] = {0x70, 0xB3, 0xD5, 0x0A, 0xC2, 0xF4};
#endif


//Altere o IP abaixo de acordo com o endereco IP da sua rede
//IPAddress ip(192,168,0,6);

// ThingSpeak Settings
char thingSpeakAddress[] = "api.thingspeak.com";
String writeAPIKey = "VPLCC0ZPVGEMG7EH";
const int updateThingSpeakInterval = 16 * 1000;      // Time interval in milliseconds to update ThingSpeak (number of seconds * 1000 = interval)

// Variable Setup
long lastConnectionTime = 0; 
boolean lastConnected = false;
int failedCounter = 0;

// Initialize Arduino Ethernet Client
EthernetClient client;

//Inicializa o servidor Web na porta 80
//EthernetServer server(80);

void setup()
{
  // Start Serial for debugging on the Serial Monitor
  Serial.begin(9600);
  
  Serial.println("Inicializando sensor DHT22!");
  
  //Incializa a biblioteca do sensor DHT22
  dht.begin();

    
  //inicializa o barramento I2C (a biblioteca BH1750 não faz isso automaticamente)
  Wire.begin();
  
  // Start Ethernet on Arduino
  startEthernet();

    
  // begin retorna um booleano que pode ser usado para detectar problemas de configuração.
  if(lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)){
     Serial.println("Inicio avançado do sensor de luz BH1750");
  }
  else{
    Serial.println("Erro ao inicializar sensor de luz BH1750");
  }

  while(!Serial){
    ; //aguarde a conexão da porta serial. Necessário apenas para porta USB nativa 
  }

  //Inicializa o cartao SD
  Serial.print("Inicializando cartao SD...");

  //Verifica se o cartao esta presente ou com erro
  if (!SD.begin(chipSelect)) {
    Serial.println("Erro no cartao ou cartao nao inserido!");
    // don't do anything more:
    return;
  }
  Serial.println(" Cartao inicializado.");
  Serial.println();
  Serial.println("Aguardando conexoes....");
  
  
}

void loop()
{
  // A leitura da temperatura ou umidade leva cerca de 250 milissegundos!
  // As leituras do sensor também podem ter até 2 segundos de atraso (é um sensor muito lento)
  float h = dht.readHumidity();
  // Ler a temperatura como Celsius (o padrão)
  float t = dht.readTemperature();
  

  // Verifique se alguma leitura falhou e saia (para tentar novamente).
  if (isnan(h) || isnan(t)) {
      Serial.println(F("Falha ao ler Sensor DHT!"));
      return;
  }

  // Calcular o índice de calor em Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);
  
  float lux = lightMeter.readLightLevel();
  
  String umid = String(h);
  String temp = String(t);
  String indexC = String(hic);
  String luz = String(lux);



  //Grava os valores lidos no cartao SD
  //Abre o arquivo arquivo.txt
  File datafile = SD.open("sensores.txt", FILE_WRITE);
  //Se arquivo.txt estiver disponivel, grava os dados
  if (datafile)
  {
      datafile.print("Umidade: ");
      datafile.print(h);
      datafile.print( "% ");
      datafile.println();
      datafile.print("Temperatura: ");
      datafile.print(t);
      datafile.print("°C ");
      datafile.println();
      datafile.print("Indice de calor: ");
      datafile.print(hic);
      datafile.print("°C ");
      datafile.println();
      datafile.print("Luminosidade: ");
      datafile.print(lux);
      datafile.print(" lux");
      datafile.println();
      datafile.println("-------------------Leitura dos dados salvos-----------------");
      datafile.close();
      //Mensagem de aviso no serial monitor
      Serial.println("Dados gravados no cartao SD");
  }
  //Mensagem de erro caso sensores.txt nao estiver disponivel 
  else {
       Serial.println("Erro ao abrir sensores.txt");
  }
   

  
  // Read value from Analog Input Pin 0
  
  // Print Update Response to Serial Monitor
  if (client.available())
  {
    char c = client.read();
    Serial.print(c);
  }

 
  // Disconnect from ThingSpeak
  if (!client.connected() && lastConnected)
  {
    Serial.println("...desconectado");
    Serial.println();
    
    client.stop();
  }
  
  // Update ThingSpeak
  if(!client.connected() && (millis() - lastConnectionTime > updateThingSpeakInterval))
  {
    updateThingSpeak("field1="+umid+"&field2="+temp+"&field3="+indexC+"&field4="+luz);
  }
  
  // Check if Arduino Ethernet needs to be restarted
  if (failedCounter > 3 ) {startEthernet();}
  
  lastConnected = client.connected();
}

void updateThingSpeak(String tsData)
{
  if (client.connect(thingSpeakAddress, 80))
  {         
  
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: "+writeAPIKey+"\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(tsData.length());
    client.print("\n\n");
    client.print(tsData);
    
    lastConnectionTime = millis();
    
    if (client.connected())
    {
      Serial.println("Connectando ao ThingSpeak...");
      Serial.println();
      
      failedCounter = 0;
    }
    else
    {
      failedCounter++;
  
      Serial.println("Conexao ao ThingSpeak falhou ("+String(failedCounter, DEC)+")");   
      Serial.println();
    }
    
  }
  else
  {
    failedCounter++;
    
    Serial.println("Conexao ao ThingSpeak Falhou ("+String(failedCounter, DEC)+")");   
    Serial.println();
    
    lastConnectionTime = millis(); 
  }
}

void startEthernet()
{
  
  client.stop();

  Serial.println("Connectando Arduino na  rede...");
  Serial.println();  
  Serial.println();

  delay(1000);
  
  // Connect to network amd obtain an IP address using DHCP
  if (Ethernet.begin(mac) == 0)
  {
    Serial.println("DHCP Falhou, reinicie Arduino e tente novamente");
    Serial.println();
  }
  else
  {
    Serial.println("Arduino connectado na rede usando DHCP");
    Serial.println();
    
  }
  
  delay(1000);
}

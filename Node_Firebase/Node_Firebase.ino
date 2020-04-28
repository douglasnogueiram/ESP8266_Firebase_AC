#include <ESP8266WiFi.h>

//Integracao com Firebase
#include <FirebaseArduino.h>

//Preparacao de JSON
#include <stdio.h>
#include <ArduinoJson.h>

//Includes para funcoes do AC Fujitsu
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Fujitsu.h>


//Inicializacao de pino de envio IR
const uint16_t kIrLed = 14;  // ESP8266 GPIO pin to use. Recommended: 14 (D5).
IRFujitsuAC ac(kIrLed);
bool ligado = false;
int valorLigaAnterior = 0;

//Definições para conexão wifi
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PASS"
#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50

#define FIREBASE_HOST "xxxxxxxx.firebaseio.com"
#define FIREBASE_AUTH "KEY"
#define ROOT "/"


//Estrutura do AC: informar sempre os parâmetros atuais de funcionamento do AC
typedef struct {
  String model;
  String power;
  String modeFunc;
  String temperature;
  String fan;
  String clean;
  String filter;
  String swing;
  String command;
  String quiet;
} AC;

AC ac_resource;



void setup()
{
  Serial.begin(115200);
  wifiConnect();
  acStart();
  delay(500);
  firebaseStart();
  delay(500);
  firebaseStream();
}



void loop()
{
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnect();
    acStart();
    delay(500);
    firebaseStart();
    delay(500);
    firebaseStream();

  }
  delay(100);
  recebeAtualizacaoFirebase();
}




void recebeAtualizacaoFirebase() {

  String comandoFirebase = "";
  String path;
  String data;

  if (Firebase.failed()) {
    Serial.println("streaming error");
    Serial.println(Firebase.error());
    Firebase.stream(ROOT);
  }

  if (Firebase.available()) {
    FirebaseObject event = Firebase.readEvent();


    String eventType = event.getString("type");
    eventType.toLowerCase();

    Serial.print("Evento: ");
    Serial.print(eventType);
    Serial.print(", ");

    path = event.getString("path");
    data = event.getString("data");

    Serial.print("Dado recebido: ");
    Serial.print(data);
    Serial.print(", ");
    Serial.print("Caminho: ");
    Serial.println(path);

    if (eventType == "put") {
      //Retira a raiz para comparação
      String transf = path;
      transf.replace("/AC/", "");
      comandoFirebase = transf;
      Serial.println(comandoFirebase);
    }
  }


  if (comandoFirebase == "power") {
    if (data != ac_resource.power) {
      if (data == "On") {
        liga_ac();
      } else if (data == "Off") {
        desliga_ac();
      } else {
        Serial.println("Opção invalida para ligar/desligar");
        Serial.println(printState());
      }
    } else {
      Serial.print("Não houve alteração de estado. Estado atual: ");
      Serial.print(ac_resource.power);
      Serial.print(" Estado informado: ");
      Serial.println(data);
    }
  }

  if (comandoFirebase == "temperature") {
    setTemp_ac(data);
  }

  if (comandoFirebase == "modeFunc") {
    setMode_ac(data);
  }

  if (comandoFirebase == "fan") {
    setFan_ac(data);
  }

  if (comandoFirebase == "swing") {
    setSwing_ac(data);
  }
}




//Ligar Ar Condicionado
void liga_ac() {
  ac.setCmd(kFujitsuAcCmdTurnOn);
  Serial.println("Enviando comando de ligar AC ...");
  delay(100);
#if SEND_FUJITSU_AC
  ac.send();
  ligado = true;

#else  // SEND_FUJITSU_AC
  Serial.println("Can't send because SEND_FUJITSU_AC has been disabled.");
#endif  // SEND_FUJITSU_AC

  //Ativar parametros padrao
  ac.setSwing(kFujitsuAcSwingVert);
  ac.send();
  Serial.println("Ativando swing...");
  delay(1000);

  ac.setMode(kFujitsuAcModeCool);
  ac.send();
  Serial.println("Ativando modo cool...");
  delay(1000);

  ac.setFanSpeed(kFujitsuAcFanAuto);
  ac.send();
  Serial.println("Ativando fan modo auto...");

  ac.setTemp(24);
  ac.send();
  Serial.println("Ativando temperatura em 24C...");

  Serial.println(printState());

}

void desliga_ac() {
  ac.setCmd(kFujitsuAcCmdTurnOff);
  Serial.println("Enviando comando de desligar AC ...");
#if SEND_FUJITSU_AC
  ac.send();
  ligado = false;
#else  // SEND_FUJITSU_AC
  Serial.println("Can't send because SEND_FUJITSU_AC has been disabled.");
#endif  // SEND_FUJITSU_AC

  Serial.println(printState());
}


void setTemp_ac(String temperature) {

  if (temperature.toInt() == 0) {
    Serial.print("Valores possíveis para a opção temperature (em C): 16 a 30. Valor informado: ");
    Serial.println(temperature);

  } else if (temperature.toInt() < 16 || temperature.toInt() > 30 ) {
    Serial.print("Valores possíveis para a opção temperature (em C): 16 a 30. Valor informado: ");
    Serial.println(temperature);

  } else {
    if (ligado) {
      ac.setTemp(temperature.toInt());  // 24C
      Serial.print("Enviando comando de ajuste de temparatura AC. Temperatura infomada: ");
      Serial.println(temperature);
#if SEND_FUJITSU_AC
      ac.send();
#else  // SEND_FUJITSU_AC
      Serial.println("Can't send because SEND_FUJITSU_AC has been disabled.");
#endif  // SEND_FUJITSU_AC
    } else {
      Serial.println("Não foi possível atualizar temperatura. O equipamento nao está ligado.");
    }
  }

  Serial.println(printState());

}



void setMode_ac(String mode) {

  if (ligado) {

    if (mode == "0") {
      ac.setMode(kFujitsuAcModeAuto);
      ac.send();

    } else if (mode.toInt() > 0 && mode.toInt() < 5 ) {

      Serial.print("Valor para switch: ");
      Serial.println(mode.toInt());
      switch (mode.toInt()) {
        case 1:
          ac.setMode(kFujitsuAcModeCool);
          Serial.println("Caiu cool");
          break;
        case 2:
          ac.setMode(kFujitsuAcModeDry);
          Serial.println("Caiu dry");
          break;
        case 3:
          ac.setMode(kFujitsuAcModeFan);
          Serial.println("Caiu Fan");
          break;
        case 4:
          ac.setMode(kFujitsuAcModeHeat);
          Serial.println("Caiu heat");
          break;
        default:
          ac.setMode(kFujitsuAcModeCool);
          Serial.println("Caiu cool default");
          break;
      }
      ac.send();
    } else {
      Serial.print("Valores possíveis para a opção Fan: 0 (Auto), 1 (Cool), 2 (Dry), 3 (Fan), 4 (Heat) - Valor enviado: ");
      Serial.println(mode);
    }

  } else {
    Serial.println("Não foi possível atualizar modo. O equipamento nao está ligado.");
  }

  Serial.println(printState());
}


void setFan_ac(String fan) {
  if (ligado) {

    if (fan == "0") {
      ac.setFanSpeed(kFujitsuAcFanAuto);
      ac.send();

    } else if (fan.toInt() > 0 && fan.toInt() < 5 ) {

      Serial.print("Valor para switch velocidade fan: ");
      Serial.println(fan.toInt());
      switch (fan.toInt()) {
        case 1:
          ac.setFanSpeed(kFujitsuAcFanHigh);
          Serial.println("Caiu high");
          break;
        case 2:
          ac.setFanSpeed(kFujitsuAcFanMed);
          Serial.println("Caiu med");
          break;
        case 3:
          ac.setFanSpeed(kFujitsuAcFanLow);
          Serial.println("Caiu low");
          break;
        case 4:
          ac.setFanSpeed(kFujitsuAcFanQuiet);
          Serial.println("Caiu quiet");
          break;
        default:
          ac.setFanSpeed(kFujitsuAcFanAuto);
          Serial.println("Caiu fan speed default");
          break;
      }
      ac.send();

    } else {
      Serial.print("Valores possíveis para a opção velocidade fan: 0 (Auto), 1 (High), 2 (Med), 3 (Low), 4 (Quiet) - Valor enviado: ");
      Serial.println(fan);

    }

  } else {
    Serial.println("Não foi possível atualizar fan. O equipamento nao está ligado.");
  }

  Serial.println(printState());
}



void setSwing_ac(String swing) {
  if (ligado) {
    if (swing == "0") {
      ac.setSwing(kFujitsuAcSwingOff);
      ac.send();

    } else if (swing == "1") {
      ac.setSwing(kFujitsuAcSwingVert);
      ac.send();

    } else {
      Serial.print("Valores possíveis para a opção Swing: 0 - Desligado e 1 - Ligado. Valor informado: ");
      Serial.println(swing);
    }
  } else {
    Serial.println("Não foi possível atualizar swing. O equipamento nao está ligado.");
  }

  Serial.println(printState());
}


char* printState() {
  //Atualiza os dados do AC para informar no response
  ac_resource.model = ac.getModelName();
  ac_resource.power = ac.getState();
  ac_resource.modeFunc = ac.getModeFunc();
  ac_resource.temperature = ac.getTemperature();
  ac_resource.fan = ac.getFan();
  ac_resource.clean = ac.getCleanAc();
  ac_resource.filter = ac.getFilterAc();
  ac_resource.swing = ac.getSwingAc();
  ac_resource.command = ac.getCommandAc();
  ac_resource.quiet = ac.getQuietAc();

  String transf = ac_resource.model;
  transf.replace("Model: ", "");
  ac_resource.model = transf;

  transf = ac_resource.power;
  transf.replace("Power: ", "");
  ac_resource.power = transf;

  transf = ac_resource.modeFunc;
  transf.replace(", Mode: ", "");
  ac_resource.modeFunc = transf;

  transf = ac_resource.fan;
  transf.replace(", Fan: ", "");
  ac_resource.fan = transf;

  transf = ac_resource.clean;
  transf.replace("Clean: ", "");
  ac_resource.clean = transf;

  transf = ac_resource.filter;
  transf.replace("Filter: ", "");
  ac_resource.filter = transf;

  transf = ac_resource.swing;
  transf.replace("Swing: ", "");
  ac_resource.swing = transf;

  transf = ac_resource.command;
  transf.replace(", Command: ", "");
  ac_resource.command = transf;

  transf = ac_resource.quiet;
  transf.replace("Outside Quiet: ", "");
  ac_resource.quiet = transf;


  //Cria um objeto para o response do post
  StaticJsonBuffer<400> jsonBufferResult;
  JsonObject& jsonObjResult = jsonBufferResult.createObject();
  char JSONmessageBuffer[400];


  jsonObjResult["model"] = ac_resource.model;
  jsonObjResult["power"] = ac_resource.power;
  jsonObjResult["modeFunc"] = ac_resource.modeFunc;
  jsonObjResult["temperature"] = ac_resource.temperature;
  jsonObjResult["fan"] = ac_resource.fan;
  jsonObjResult["clean"] = ac_resource.clean;
  jsonObjResult["filter"] = ac_resource.filter;
  jsonObjResult["swing"] = ac_resource.swing;
  jsonObjResult["command"] = ac_resource.command;
  jsonObjResult["quiet"] = ac_resource.quiet;
  jsonObjResult.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));

  return JSONmessageBuffer;
}



int wifiConnect()
{
  int retries = 0;

  Serial.println("Connecting to WiFi AP..........");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // check the status of WiFi connection to be WL_CONNECTED
  while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY)) {
    retries++;
    delay(WIFI_RETRY_DELAY);
    Serial.print("#");
  }
  Serial.println("");
  return WiFi.status(); // return the WiFi connection status
}

void acStart() {
  //Configuracao inicial para o AC
  ac.begin();
  delay(200);
  Serial.println("Estado padrão para o controle do AC Fujitsu");

  //Incialização de parâmetros AC
  ac.setModel(ARREB1E);
  desliga_ac();
  //ac.setSwing(kFujitsuAcSwingVert);
  //ac.setMode(kFujitsuAcModeCool);
  //ac.setFanSpeed(kFujitsuAcFanAuto);
}

void firebaseStart() {

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  //Define o BD com parâmetros iniciais desligados
  Firebase.setString("/AC/model", ac_resource.model);
  Firebase.setString("/AC/power", ac_resource.power);
  Firebase.setString("/AC/modeFunc", ac_resource.modeFunc.substring(0, 1));
  Firebase.setString("/AC/temperature", ac_resource.temperature);
  Firebase.setString("/AC/fan", ac_resource.fan.substring(0, 1));
  Firebase.setString("/AC/clean", ac_resource.clean);
  Firebase.setString("/AC/filter", ac_resource.filter);
  Firebase.setString("/AC/swing", ac_resource.swing.substring(0, 1));
  Firebase.setString("/AC/command", ac_resource.command);
  Firebase.setString("/AC/quiet", ac_resource.quiet);

}

void firebaseStream() {
  Firebase.stream(ROOT);

}

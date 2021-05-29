/*
 * Контроллер туалета на ESP-01s
 * 
 * Подключение:
 * GPIO0 - пин реле гигиенического душа
 * GPIO1 - (TXD0) пин управления адресной лентой
 * GPIO2 - пин реле электрокрана воды 
 * GPIO3 - (RXD0) пин датчика движения
 * 
 * VCC - пиание +3.3V
 * CH_PD - питание +3.3V через резистор 10к (без него модуль не работает)
 * GND - питание GND
 * 
 * Настройки компиляции:
 * - Плата: Generic ESP8266 Module
 * - Flash Size: 1M(64K SPIFFS)
 * - Flash Mode: DOUT(compatible)
 * - Reset Metod: NodeMCU
 * 
 * v01 - 17/10/19
 * - радуга на светодиодной ленте
 * - чтение датчика движения
 * - управление реле по UDP (w1 - кран открыт, w0 - кран закрыт)
 * - управление через Blynk
 * 
 * v02 - 26/10/19
 * - добавлены функции OTA
 * 
 * v03 - 31/10/19
 * - изменен алгоритм подключения к wi-fi в функции Connect
 * - добавлены эффекты адресной-ленты
 * - функции OTA заменены на WEB-update
 * 
 * v05 - 19/02/2020
 * - корректировка под connect_func_v05
 * - изменены ip адреса устройств
 * - добавлен приоритет ручного управления 
 * 
 * v05x - 03/03/2020
 * 051 - закомментированы функции Blynk
 * 052 - закомментированы сетевые функции кроме OTA update
 * 053 - закомментированы все сетевые функции 
 * 054 - функции OTA действуют первые 5 ми после ребута, затем отключаются
 * 055 - отключена только проверка на статус подключения к wi-fi
 * 
 * v10 - 17/03/2020
 * переход на connect_func_v10
 * Отказ от Blynk, переход на MQTT
 * 
 * v101 - 18/03/2020
 * перенесены настройки Mqtt из connect_func в основную программу
 * 
 * v102 - 20/03/2020
 * устранена ошибка стипом переменной mqtt_fader_step
 * добавлено управление mqtt_vortex_step
 * 
 * v103 - 23/03/2020
 * исправлена функция mqtt_get
 * 
 * v104 - 24/03/2020
 * добавлены тестовые топики для обратной связи
 * увеличена общая яркость ленты
 * 
 * v105 - 25/03/2020
 * функции передачи MQTT_publish_int и MQTT_publish_float перенесены в connect_mqtt
 * добавлена подписка на топики отладки водоворота
 * удалено время на OTA update
 * 
 * v106
 * яркость лены уменьшена до 200
 * 
 * v11 - 04/04/2020 - стабильная
 * Удалены тестовые параметры для mqtt
 * вермя ON_TIME умендено до 90с
 * 
 * v111 - 05/04/2020
 * добавлен топик синхронизации с MQTT
 * 
 * v12 - 18/04/2020
 * изменено управление реле крана: состояние Normal Closed реле (Relay_ON=0) соответствует положению коан открыт 
 * 
 * v13 - 20/04/2020
 * добавлен таймер на изменение состояния крана воды (не чаще чем 1 раз в 15 сек) 
 * 
 * v14 - 22/04/2020
 * добавлен таймер на на изменение состояния реле туалета (защита от глюка с частым переключением)
 * 
 * v15 - 05/05/2020
 * добавлена синхронизация по изменению эффекта ленты
 * 
 * v16 - 22/05/2020
 * топики mqtt разделены на топики управления ctrl и топики статуса state
 * добавлены флаги retain вместо топиков синхронизации (connect_mqtt_v03)
 * 
 * v20 - 31/05/2020
 * добавлено получение времни ntp
 * добавлен эффект плавного приведения к нужному цвету FADE_TO_COLOR (effects_v14)
 * добавлена защита от ложных срабатываний в топике topic_led_ctrl при включении
 */


#define PIN_toilet_relay 0  // пин реле гигиенического душа
#define PIN_led_strip    1  // пин подключения адресной ленты
#define PIN_water_relay  2  // пин реле электрокрана воды 
#define PIN_motion_sens  3  // пин подключения датчика движения

#define OFF           0
#define RAINBOW       1
#define HELLO         4
#define FADE_TO_COLOR 5
#define ALARM         10


#include <FastLED.h>
#define LEDS_count 32                  // число пикселей в адресной ленте
CRGB Strip[LEDS_count];


#include <ESP8266WiFi.h>
const char ssid[] = "welcome's wi-fi";
const char pass[] = "27101988";
const bool NEED_STATIC_IP = true;
IPAddress IP_Node_MCU          (192, 168, 1, 71);
IPAddress IP_Fan_controller    (192, 168, 1, 41);
IPAddress IP_Water_sensor_bath (192, 168, 1, 135); 
IPAddress IP_Toilet_controller (192, 168, 1, 54);


#include <WiFiUdp.h>
WiFiUDP Udp;
unsigned int localPort = 8888;      
char Buffer[UDP_TX_PACKET_MAX_SIZE];  


#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;


#include <PubSubClient.h>
WiFiClient NodeMCU;
PubSubClient client(NodeMCU);
const char* mqtt_client_name = "Toilet_esp8266";    // Имя клиента


#include <NTPClient.h>
WiFiUDP Udp2;
NTPClient timeClient(Udp2, "europe.pool.ntp.org");


const int WATER_BLOCK_TIME = 60*1000;         // дительность блокировки крана воды после пропадания тревоги
const int ON_TIME = 90 * 1000;                // длительность работы после пропадания движения
const int CHECK_PERIOD = 2 * 60 * 1000;       // периодичность проверки на подключение к сервисам
const int RESTART_PERIOD = 30*60*1000;        // время до ребута, если не удается подключиться к wi-fi
const int MAX_MANUAL_PERIOD = 1000*60*30;     // максимальное время работы в ручном режиме 30 мин
const int MANUAL_TOILET_DELAY = 60*1000;      // время приоретета в ручном режиме для реле гиг душа
const int MANUAL_WATER_DELAY = 60*1000;       // время приоретета в ручном режиме для реле воды
const int VALVE_MIN_CHANGE_TIME = 15*1000;     // время переключения помпы (из ON в OFF или наоборот)
const int GET_NTP_TIME_PERIOD = 60*1000;      // период получения времни с сервера NTP

unsigned long Last_online_time;               // время когда модуль был онлайн
unsigned long Manual_Toilet_relay_time;       // время ручного управления реле гиг душа
unsigned long Manual_Water_relay_time;        // время ручного управления реле воды 
unsigned long Last_check_time;                // время крайней проверки подключения к сервисам
unsigned long Manual_mode_time;               // время включения ручного режима управления лентой 
unsigned long Motion_time = ON_TIME;          // время срабатывания датчика движения
unsigned long Last_get_ntp_time;              // время крайнего получения вермени NTP
unsigned long Water_alarm_time;               // время сигнала протечки воды
unsigned long Last_change_Water_relay;        // время переключения гидрострелки

byte Manual_mode = OFF;                       // режим ленты, управляемый через MQTT
bool Toilet_relay_ON = false;                 // состояние реле гигиенического душа
bool Water_relay_ON = true;                   // состояние реле воды (гидрострелка)

byte LED_effect = OFF;                        // текущий эфффект светодиодной ленты
byte last_LED_effect = OFF;                   // эфффект светодиодной ленты на предыдущем такте 
bool Night = false;                           // признак НОЧЬ по серверу NTP
bool Water_alarm_flag = false;                // признак протечки

// топики управления реле и управления лентой
const char topic_water_relay_ctrl[] = "user_1502445e/toilet/water_ctrl";
const char topic_toilet_relay_ctrl[] = "user_1502445e/toilet/toilet_ctrl";
const char topic_led_ctrl[] = "user_1502445e/toilet/led_ctrl";

// топики статуса реле и эффекта ленты
const char topic_water_relay_state[] = "user_1502445e/toilet/water";
const char topic_toilet_relay_state[] = "user_1502445e/toilet/toilet";
const char topic_led_state[] = "user_1502445e/toilet/led";

const char topic_water_alarm[] = "user_1502445e/bath/alarm";

//=========================================================================================

void setup() {
  pinMode(PIN_motion_sens, INPUT);  
  pinMode(PIN_water_relay, OUTPUT);  digitalWrite(PIN_water_relay, HIGH);  //реле воды по умолчанию включено
  pinMode(PIN_toilet_relay, OUTPUT); digitalWrite(PIN_toilet_relay, HIGH);

  FastLED.addLeds<WS2811, PIN_led_strip, GRB>(Strip, LEDS_count).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(200); //0-255
  FastLED.clear();
 
  Connect_WiFi(IP_Toilet_controller, NEED_STATIC_IP);
  Connect_mqtt(mqtt_client_name);
  MQTT_subscribe();
 
  // защита от ложных срабатываний
  MQTT_publish_int(topic_led_ctrl, OFF); 
  MQTT_publish_int(topic_led_state, OFF);
  
  timeClient.begin();
  timeClient.setTimeOffset(4*60*60);   //смещение на UTC+4
}

//========================================================================================

void loop() {
  
  // получение признака Ночь
  if ((long)millis() - Last_get_ntp_time > GET_NTP_TIME_PERIOD) {
    Last_get_ntp_time = millis(); 
    timeClient.update();
    if ((timeClient.getHours() > 20) || (timeClient.getHours() < 7))
      Night = true;
    else  
      Night = false;
  }  

  // проверка сигнала от датчика движения
  bool Motion_flag = Motion();

  // управление светодиодной лентой
  if ((long)millis() - Manual_mode_time > MAX_MANUAL_PERIOD)   
    Manual_mode = OFF;
    
  if (!Water_relay_ON)   LED_effect = ALARM; 
  else if (Manual_mode)  LED_effect = Manual_mode;  
  else if (Motion_flag) {
    if (Night)           LED_effect = FADE_TO_COLOR;
    else                 LED_effect = HELLO;     
  }
  else                   LED_effect = OFF;
  LED_strip(LED_effect);
  
  if (LED_effect != last_LED_effect) {
    MQTT_publish_int(topic_led_state, LED_effect);
    last_LED_effect = LED_effect;   
  }
  
    
  // сетевые функции
  httpServer.handleClient();          // для обновления по воздуху   
  client.loop();                      // для функций MQTT 
  Receive_UDP();                      // получение данных от датчиков протечки
  
  // управляем реле воды по сигналу UDP только если не ручной режим
  if ((long)millis() - Manual_Water_relay_time > MANUAL_WATER_DELAY) {
    if (Water_alarm_flag) Water_relay_ON = false;
    else                  Water_relay_ON = true;
  }
  
  // управляем гиг душем по датчику движения только если не ручной режим 
  if ((long)millis() - Manual_Toilet_relay_time > MANUAL_TOILET_DELAY) {    
    if (Motion_flag) Toilet_relay_ON = true;
    else             Toilet_relay_ON = false;
  }

  // Управление реле
  Relay_control(); 
  
  // проверка подключений к wifi и серверам
  if ((long)millis() - Last_check_time > CHECK_PERIOD) {
    Last_check_time = millis(); 
    // wi-fi  
    if (WiFi.status() != WL_CONNECTED) { 
      Connect_WiFi(IP_Toilet_controller, NEED_STATIC_IP);
      Restart(Last_online_time, RESTART_PERIOD);
    }
    else
      Last_online_time = millis();     
    // mqtt
    if (!client.connected()) {
      Connect_mqtt(mqtt_client_name);
      MQTT_subscribe();
    }      
  } 
}


//=========================================================================================
// функция определения движения

bool Motion () {
  if (digitalRead(PIN_motion_sens)) { 
    Motion_time = millis(); 
    return true; 
  }
  if ((long)millis() - Motion_time < ON_TIME){
    return true;     
  }
  return false;   
}


//=========================================================================================
// функция управления реле

bool last_state_Toilet_relay = false;
bool last_state_Water_relay = true;

void Relay_control (void) {
  if (Toilet_relay_ON != last_state_Toilet_relay) {      
    digitalWrite(PIN_toilet_relay, !Toilet_relay_ON);     // реле управляется низким уровнем 0=кран гиг душа открыт  
    last_state_Toilet_relay = Toilet_relay_ON;
    MQTT_publish_int(topic_toilet_relay_state, Toilet_relay_ON); // публикация данных в MQTT                 
  }
  if ((Water_relay_ON != last_state_Water_relay) && ((long)millis() - Last_change_Water_relay > VALVE_MIN_CHANGE_TIME)) { 
    Last_change_Water_relay = millis();                   // запоминаем время изменения состояния крана
    digitalWrite(PIN_water_relay, Water_relay_ON);        // реле управляется низким уровнем 0=кран выключен 
    last_state_Water_relay = Water_relay_ON;  
    MQTT_publish_int(topic_water_relay_state, Water_relay_ON);  // публикация данных в MQTT    
  }
}

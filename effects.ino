
//===================================================================================================
// функция управления режимами светодиодной ленты

byte          Last_type;             // предыдущий режим ленты
byte          LED_cycle_time = 20;   // длительность одного цикла для адреской ленты (задает скорость)
unsigned long Last_cycle_time;       // время крайнего цикла адреской ленты
unsigned long LED_start_time;        // время начала эффекта

CRGB mid_RGBcolor(255, 255, 255);    // RBG цвет вспышка 
CRGB back_RGBcolor(80, 0, 60);       // RBG цвет финальный (оранжевый) 

// управление лентой
void LED_strip (byte type) {  
  if ((long)millis() - Last_cycle_time > LED_cycle_time) {
    Last_cycle_time = millis(); 
    
    if (type != Last_type) {    
      // очищаем ленту от старых эффектов
      if (type) FastLED.clear();              
      Last_type = type ;
      
      // запоминаем время начала эффекта
      LED_start_time = millis();

      // установка цикла работы светодиодной ленты
      switch (type) {
        case 0:  LED_cycle_time = 30; break;
        case 1:  LED_cycle_time = 20; break;
        case 2:  LED_cycle_time = 20; break;
        case 3:  LED_cycle_time = 30; break;
        case 4:  LED_cycle_time = 0;  break;
        case 5:  LED_cycle_time = 40; break;
        case 10: LED_cycle_time = 20; break;        
      }
    }
    
    switch (type) {             
      case 0:  fader(1);                      break;      //  FastLED.clear();
      case 1:  Rainbow_mode();                break; //RAINBOW
      case 2:  Sparkles_mode();               break;
      case 3:  Sea_mode();                    break;
      case 4:  Hello_mode(LED_start_time);    break; // HELLO
      case 5:  Fade_to_color_mode(back_RGBcolor, 1); break;
      case 10: Alarm_mode();                  break; // ALARM
    }
    FastLED.show();
  }
}


// ================================= ЭФФЕКТЫ ====================================

// --------------------------------- конфетти ------------------------------------

void Sparkles_mode(void) {
  byte number = random(1, LEDS_count);
  if (Strip[number].r == 0 && Strip[number].g == 0 && Strip[number].b == 0) {  // если  пиксель не горит
    Strip[number] = CHSV(random(0, 255), 255, 255);
  }
  fader(25);
}

// функция плавного угасания цвета для всех пикселей
void fader(byte step) {
  for (byte i = 0; i < LEDS_count; i++ ) { fadePixel(i, step); }
}

void fadePixel(byte number, byte step) {     // новый фейдер
  if (Strip[number].r == 0 && Strip[number].g == 0 && Strip[number].b == 0) return;
  if (Strip[number].r >= step || Strip[number].g >= step || Strip[number].b >= step) {
    Strip[number].fadeToBlackBy(step);
  } 
  else { Strip[number] = 0; }
}

//===================================================================================================
// Радуга 
byte Rainbow_counter = 0;         

void Rainbow_mode (void) {  
  byte Rainbow_step = 256 / LEDS_count;  // одна радуга на всю длинну ленты
  for (byte i = 0; i < LEDS_count; i++ ) { Strip[i] = CHSV(Rainbow_counter + i*Rainbow_step, 255, 255); }
  Rainbow_counter += 4;                  // задает скорость перемещения радуги по ленте
}


//===================================================================================================
// тревога (моргание красным цветом)

byte Alarm_speed = 12;            // скорость нарастания для режима тревоги
byte RED_color_counter = 100;     // значение красного цвета

void Alarm_mode (void) {
  RED_color_counter += Alarm_speed;
  if ((RED_color_counter > 230) || (RED_color_counter < 50))  Alarm_speed = -Alarm_speed;  
  for (byte i = 0; i < LEDS_count; i++ ) { Strip[i].r = RED_color_counter; }
}


//===================================================================================================
// Море 
const int HUE_PERIOD = 3000;                   // период рандомного изменения цвета
const int BRIGHTNESS_PERIOD = 2000;            // период рандомного изменения яркости
int Wave_step_period = 40;                     // период одного цикла вычислений волны (задет скорость волны)
int Wave_period = 1000;                        // период запуска новой волны

unsigned long Last_set_brightness_time;        // время крайнего изменения яркости
unsigned long Last_set_hue_time;               // время крайнего изменения цвета
unsigned long Last_wave_step_time;             // время крайнего цикла вычислений волны
unsigned long Wave_end_time;                   // время окончания волны

byte fade_power = 3;                    // сила приведения к нужному цвету
byte next_hue, next_brightness;
bool Wave_end;                          //конец волны

CRGB target_RGBcolor(0,0,0);
CHSV target_HSVcolor(133,255,90);


void Sea_mode() { 
  
  //вычисление основного цвета 
  if ((long)millis() - Last_set_hue_time > HUE_PERIOD) { 
    Last_set_hue_time = millis();
    next_hue = random (128, 138);
  }  
  if ((long)millis() - Last_set_brightness_time > BRIGHTNESS_PERIOD) { 
    Last_set_brightness_time = millis();
    next_brightness = random (70, 85);
  }
  CHSV target_HSVcolor (next_hue, 255, next_brightness);  
  hsv2rgb_spectrum(target_HSVcolor, target_RGBcolor); 

  //запуск волны
  if (Wave_end && (long)millis()-Wave_end_time > Wave_period)  
    Wave_end = false;      
  if (!Wave_end && (long)millis()-Last_wave_step_time > Wave_step_period) { 
    Last_wave_step_time = millis();
    Wave_end = Wave_glare();
    if (Wave_end) { 
      Wave_end_time = millis(); 
      Wave_period = random (0, 1000);
    }
  }

  //угасание цвета до основного
  for (byte i = 0; i < LEDS_count; i++ ) { Fade_to_color(i, target_RGBcolor, 3); }
}


void Fade_to_color (byte number, CRGB target_color, byte fade_power) {
  for (byte i=0; i<fade_power; i++) {
    if (Strip[number].r < target_color.r) Strip[number].r += CRGB(1, 0, 0);
    if (Strip[number].r > target_color.r) Strip[number].r -= CRGB(1, 0, 0);
    if (Strip[number].r > 200           ) Strip[number].r -= CRGB(1, 0, 0); 
    
    if (Strip[number].g < target_color.g) Strip[number].g += CRGB(0, 1, 0);
    if (Strip[number].g > target_color.g) Strip[number].g -= CRGB(0, 1, 0);
    if (Strip[number].g > 200           ) Strip[number].g -= CRGB(0, 1, 0);
         
    if (Strip[number].b < target_color.b) Strip[number].b += CRGB(0, 0, 1);
    if (Strip[number].b > target_color.b) Strip[number].b -= CRGB(0, 0, 1);
    if (Strip[number].b > 200           ) Strip[number].b -= CRGB(0, 0, 1); 
  }
}


byte wave_step = 0;
byte wave_power;
int  wave_pozition;     // индекс
bool wave_direction;    // true - вправо (+) , false - влево (-)
byte wave_lenght;       // длина волны
byte wave_foarm[5] = {1,2,3,2,1} ; // форма волны

bool Wave_glare(){   
  
  //новый шаг волны
  if (wave_step) {    
    //сила волны
    if ((wave_step < wave_lenght/2) && (wave_power < 6)) wave_power++;
    if ((wave_step > wave_lenght/2) && (wave_power > 1)) wave_power--;    
    //позиция волны
    if (wave_direction) wave_pozition++;
    else                wave_pozition--;
  } 
  
  // если новая волна 
  else {                                             
    wave_pozition  = random (0, LEDS_count);            // индекс появления волны
    wave_direction = random (0,2);                      // направление волны
    wave_lenght = random (LEDS_count-10,LEDS_count); 
    //wave_lenght = random (LEDS_count-5,LEDS_count*2); // длинна волны
    wave_power = 1;                                     // сила волны
  }

  // меняем 5 пикселей в соответстии с формой волны
  for (byte i=0; i<5; i++) {                     
    byte add_brightness = wave_power * wave_foarm[i];
    byte reg_saturation = wave_power * wave_foarm[i];
    int index = wave_pozition+i;
    index %= LEDS_count;
    for (int j=0; j<add_brightness; j++) Strip[index] += CHSV(0,0,1);
    for (int j=0; j<reg_saturation; j++) Strip[index] -= CHSV(0,1,0);
  }  


  wave_step++;  
  if (wave_step > wave_lenght) { 
    wave_step = 0;
    return true; // если волна закончилась
  }  
  else 
    return false;
}


//===================================================================================================
// Приветствие

byte x1 = 0;
byte x2 = LEDS_count/2;
int vortex_time = 1000*4;           // время водоворота 

float vortex_fader_step;            // шаг угасания яркости
float vortex_step_time;             // начальный период времени шага водоворота
byte vortex_brightness;             // начальная яркость водоворота
byte vortex_saturation;             // начальная насыщенность водоворота
byte vortex_color;                  // начальный цвет водоворота

unsigned long Last_vortex_step_time; // время последнего шага
bool first_vortex_step = true;       // флаг первого цикла водоворота

void Hello_mode(unsigned long start_time) 
{
  // обнуляем переменные водоворота  
  if (((long)millis() - Last_vortex_step_time > vortex_time) || first_vortex_step) {   // если первый шаг или водоворот давно не выполнялся 
    if ((long)millis() - start_time < vortex_time) {                                   // и если приветствие выполняется дольше чем водоворот
      vortex_step_time = 80; 
      vortex_brightness = 70;
      vortex_saturation = 255; 
      vortex_fader_step = 20;    
      vortex_color = random (0,256);
    }
  }
  
  //  водоворот
  if ((long)millis() - start_time < vortex_time) {   // время выполнения водоворота
    
    if (((long)millis() - Last_vortex_step_time > vortex_step_time) || first_vortex_step) {       
      first_vortex_step = false;
      Last_vortex_step_time = millis(); 
              
      // гасим яркость у пикселей
      fader((byte)vortex_fader_step);
      vortex_fader_step *= 0.99;
      
      //уменьшаем время шага для ускорения водоворота
      vortex_step_time *= 0.98;  
      
      // инкрементируем координаты
      x1++; x1 %= LEDS_count;
      x2++; x2 %= LEDS_count;     
      
      // инкрементируем яркость, насыщенность, цвет
      vortex_brightness ++; if (vortex_brightness>255) vortex_brightness = 255;
      vortex_saturation --; if (vortex_saturation<50) vortex_saturation = 50;
      vortex_color += 2;
      
      // зажигаем пиксели
      Strip[x1] = CHSV(vortex_color, vortex_saturation, vortex_brightness); 
      Strip[x2] = CHSV(vortex_color, vortex_saturation, vortex_brightness);       
    }
  }
  else if ((long)millis() - start_time < vortex_time + 300) {
    for (byte i = 0; i < LEDS_count; i++ ) { Fade_to_color(i, mid_RGBcolor, 1); }
    LED_cycle_time = 10; 
  }
  else {  // после водоворота угасание до нужной яркости и цвета
    for (byte i = 0; i < LEDS_count; i++ ) { Fade_to_color(i, back_RGBcolor, 1); }
  }  
    
}


//===================================================================================================
// fade to color mode
void Fade_to_color_mode (CRGB RGBcolor, byte fade_power) {
  for (byte i = 0; i < LEDS_count; i++ ) {
    Fade_to_color(i, RGBcolor, fade_power);
  }
}

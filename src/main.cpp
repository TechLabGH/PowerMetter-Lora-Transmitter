#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <RadioLib.h>
#include <U8g2lib.h>

// OLED display
#define SCREEN_WIDTH   128  // OLED display width, in pixels
#define SCREEN_HEIGHT  64   // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define OledSDA 17
#define OledSCL 18
#define OledRST 21   
U8G2_SSD1306_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, OledSCL, OledSDA, OledRST);

// LORA radiolib setup

// SX1262 has the following connections:
// NSS pin:   8
// DIO1 pin:  14
// NRST pin:  12
// BUSY pin:  13
  SX1262 radio = new Module(8, 14, 12, 13);

// meters data
uint32_t pow_a = 0;
uint32_t pow_b = 0;
float temp     = 0.01;
float hum      = 0.01;

// timers
hw_timer_t *timer_min = NULL;
hw_timer_t *timer_sec = NULL;
uint8_t time_min  = 0;

// flags
uint8_t f_upload  = 0;  // upload data over LORA
uint8_t f_refresh = 0;  // refresh display
uint8_t menu_b    = 0;  // pressed menu button
uint8_t wifi_on   = 0;  // Wifi terminal disabled by default
uint8_t LORA_st   = 0;  // status of LORA

void F_upload(){
    // function uploading data over LORA
    String   S_DATA = "#" + String(pow_a);
    S_DATA = S_DATA + "#" + String(pow_b);
    S_DATA = S_DATA + "#" + String(float(int(temp*100))/100);
    S_DATA = S_DATA + "#" + String(float(int(hum*100))/100);
    S_DATA = S_DATA + "#";

    int state = radio.transmit(S_DATA);

    pow_a     = 0;
    pow_b     = 0;
    f_upload  = 0;

}

void F_refresh(){
    // function refreshing display
    
    u8g2.firstPage();
    int w = time_min * 4;
    do {
        u8g2.setFont(u8g2_font_haxrcorp4089_t_cyrillic);
        u8g2.setCursor(5,18);
        u8g2.print("A:" + String(pow_a));
        u8g2.setCursor(5,28);
        u8g2.print("B:" + String(pow_a));
        u8g2.setCursor(75,18);
        u8g2.print(String(temp) + "*C");
        u8g2.setCursor(75,28);
        u8g2.print(String(hum) + "%");
        u8g2.drawFrame(0,1,124,7);
        u8g2.drawBox(2,3,w,3);
        u8g2.setFont(u8g2_font_helvR08_tr);
        u8g2.drawFrame(0,49,40,14);
        u8g2.drawFrame(42,49,40,14);
        u8g2.drawFrame(84,49,40,14);
        u8g2.setCursor(7,60);
        u8g2.print("PING");
        u8g2.setCursor(47,60);
        u8g2.print("SYNC");
        u8g2.setCursor(95,60);
        u8g2.print("WiFi");
        u8g2.setCursor(15,45);
        u8g2.print("LORA");
        u8g2.setCursor(80,45);
        u8g2.print("WiFi");
        u8g2.setFont(u8g2_font_unifont_t_symbols);
        if (LORA_st == 1) {
            u8g2.drawGlyph(0,47,9745);
        } else {
            u8g2.drawGlyph(0,47,9744);
        }
        
        if (wifi_on == 1) {
            u8g2.drawGlyph(64,47,9745);
        } else {
            u8g2.drawGlyph(64,47,9746);
        }
    } while (u8g2.nextPage());

    f_refresh = 0;
}

void IRAM_ATTR onTimer_sec(){
    digitalWrite(35, !digitalRead(35));
}

void IRAM_ATTR onTimer_min(){
    time_min++;
    f_refresh = 1;
    if (time_min % 5 == 0) {
        f_upload = 1;
    }
    if (time_min == 30) {
        time_min =0;
    }
    
}

void CountAInc(){
  pow_a++;
}

void CountBInc(){
  pow_b++;
}

void menu_pInc(){
    menu_b = 1;
}

void menu_sInc(){
    menu_b = 2;
}

void menu_wInc(){
    menu_b = 3;
}

void setup() {
    // flashing LED
    pinMode(35, OUTPUT);

    // zero initial temp and hum
    temp = 0;
    hum  = 0;

    // power reader pins
    pinMode(45, INPUT);
    pinMode(46, INPUT);
    attachInterrupt(digitalPinToInterrupt(45), CountAInc, RISING);
    attachInterrupt(digitalPinToInterrupt(46), CountBInc, RISING);

    // menu buttons
    pinMode(38, INPUT); // ping
    pinMode(39, INPUT); // sync
    pinMode(40, INPUT); // wifi
    attachInterrupt(digitalPinToInterrupt(38), menu_pInc, RISING);
    attachInterrupt(digitalPinToInterrupt(39), menu_sInc, RISING);
    attachInterrupt(digitalPinToInterrupt(40), menu_wInc, RISING);

    // init OLED display
    u8g2.begin();
    u8g2.setDisplayRotation(U8G2_R2);
    u8g2.firstPage();
        do {
            u8g2.setFont(u8g2_font_haxrcorp4089_t_cyrillic);
            u8g2.drawStr(2,10,"Call trans opt: received.");
            u8g2.setFont(u8g2_font_haxrcorp4089_t_cyrillic);
            u8g2.drawStr(2,22,"9-18-99 14:32:21 REC:Log>");
        } while (u8g2.nextPage());
    delay(3000);
    f_refresh = 1;

    int state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
        LORA_st = 1;
    } else {
        LORA_st = 0;
    }


    // timer_min definition
    timer_min = timerBegin(0, 80, true); //uint8_t num, uint16_t prescalar 80, bool countUp . our clock is 80MHz 
    timerAttachInterrupt(timer_min, &onTimer_min, true); 
    timerAlarmWrite(timer_min, 60000000, true);
    //timerAlarmWrite(timer_min, 14400000000LL, true); //the callback function will be executed when timer ticks 1000000 times
    timerAlarmEnable(timer_min);

    // timer_sec definition
    timer_sec = timerBegin(1, 80, true); //uint8_t num, uint16_t prescalar 80, bool countUp . our clock is 80MHz 
    timerAttachInterrupt(timer_sec, &onTimer_sec, true); 
    timerAlarmWrite(timer_sec, 1000000, true);
    //timerAlarmWrite(timer_min, 14400000000LL, true); //the callback function will be executed when timer ticks 1000000 times
    timerAlarmEnable(timer_sec);

}

void loop() {

    if (f_upload == 1) F_upload();
    if (f_refresh == 1) F_refresh();

    if (menu_b > 0) {   // menu function
        delay(30);
        if (menu_b ==1) {

        }

        if (menu_b ==2) {
            F_upload();
            F_refresh();
            time_min  = 0;
            menu_b    = 0;
        }

        if (menu_b ==3) {
            wifi_on |= wifi_on;
            F_refresh();
        }

    }

}


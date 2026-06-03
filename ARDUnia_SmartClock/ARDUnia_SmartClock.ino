/*
 * =========================================================================
 * راهنمای اتصالات ماژول‌ها (Wiring Guide):
 * =========================================================================
 * 1. نمایشگر OLED (I2C):
 * - VCC  -> 3.3V / 5V
 * - GND  -> GND
 * - SDA  -> A4 (در Arduino Nano/Uno)
 * - SCL  -> A5 (در Arduino Nano/Uno)
 *
 * 2. سنسور دما و رطوبت DHT11:
 * - VCC  -> 5V (از طریق مقاومت 1 کیلو اهم)
 * - GND  -> GND
 * - DATA -> D12 (پایه دیجیتال 12)
 *
 * 3. ماژول تاچ TTP223:
 * - VCC  -> 3.3V / 5V
 * - GND  -> GND
 * - SIG  -> D2 (پایه دیجیتال 2)
 *
 * 4. ماژول ساعت (RTC DS1307):
 * - VCC  -> 5V
 * - GND  -> GND
 * - SDA  -> A4
 * - SCL  -> A5
 *
 * 5. خوانش ولتاژ باتری (تقسیم‌کننده مقاومتی):
 * - نقطه اتصال دو مقاومت -> A0 (پایه آنالوگ 0)
 * (مقاومت‌ها برای کاهش ولتاژ باتری به محدوده 0-5 ولت)
 * =========================================================================
 */
#include <Wire.h>
#include <RTClib.h>
#include <U8g2lib.h>
#include <DHT.h>
#include <avr/wdt.h>

#define TOUCH_PIN 2      // پایه ماژول تاچ TTP223
#define DHTPIN 12        // پایه ماژول دما و رطوبت DHT11
#define DHTTYPE DHT11    // نوع سنسور DHT
#define BATTERY_PIN A0   // پایه خوانش ولتاژ باتری
#define VOLTAGE_DIVIDER 2

// #define TOUCH_INVERT

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C oled(U8G2_R0); 
RTC_DS1307 rtc; 
DHT dht(DHTPIN, DHTTYPE);

// ========== متغیرهای عمومی سنسور دما و رطوبت ==========
float dhtTemperature = 0.0;
float dhtHumidity = 0.0;
int TmpDef = -5; // کالیبراسیون دما
int HumDef = 0; // کالیبراسیون رطوبت
unsigned long lastDHTReadTime = 0;
const unsigned long DHT_READ_INTERVAL = 10000; // خوانش سنسور هر 10 ثانیه یکبار

// ========== آرایه فازهای ماه در ابعاد 10 در 10 پیکسل ==========
const unsigned char moon_phases[] = {
  // 0: ماه نو
  0x3F, 0x00, 0x40, 0x80, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x80, 0x40, 0x40, 0x80, 0x3F, 0x00,
  // 1: هلال فزاینده
  0x23, 0x00, 0x41, 0x80, 0x81, 0xC0, 0x81, 0xC0, 0x81, 0xC0, 0x81, 0xC0, 0x81, 0xC0, 0x81, 0xC0, 0x41, 0x80, 0x23, 0x00,
  // 2: تربیع اول
  0x3F, 0x00, 0x47, 0x80, 0x87, 0xC0, 0x87, 0xC0, 0x87, 0xC0, 0x87, 0xC0, 0x87, 0xC0, 0x87, 0xC0, 0x47, 0x80, 0x3F, 0x00,
  // 3: کوژماه فزاینده
  0x3F, 0x00, 0x5F, 0x80, 0xBF, 0xC0, 0xBF, 0xC0, 0xBF, 0xC0, 0xBF, 0xC0, 0xBF, 0xC0, 0xBF, 0xC0, 0x5F, 0x80, 0x3F, 0x00,
  // 4: ماه کامل
  0x3F, 0x00, 0x7F, 0x80, 0xFF, 0xC0, 0xFF, 0xC0, 0xFF, 0xC0, 0xFF, 0xC0, 0xFF, 0xC0, 0xFF, 0xC0, 0x7F, 0x80, 0x3F, 0x00,
  // 5: کوژماه کاهنده
  0x3F, 0x00, 0x7E, 0x80, 0xFF, 0x40, 0xFF, 0x40, 0xFF, 0x40, 0xFF, 0x40, 0xFF, 0x40, 0xFF, 0x40, 0x7E, 0x80, 0x3F, 0x00,
  // 6: تربیع سوم
  0x3F, 0x00, 0x78, 0x80, 0xF8, 0x40, 0xF8, 0x40, 0xF8, 0x40, 0xF8, 0x40, 0xF8, 0x40, 0xF8, 0x40, 0x78, 0x80, 0x3F, 0x00,
  // 7: هلال کاهنده
  0x31, 0x00, 0x60, 0x80, 0xE0, 0x40, 0xE0, 0x40, 0xE0, 0x40, 0xE0, 0x40, 0xE0, 0x40, 0xE0, 0x40, 0x60, 0x80, 0x31, 0x00
};

// ========== توابع تبدیل میلادی به شمسی ==========
bool isGregorianLeap(int year) {
  return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)); 
}

int getDayOfYear(int year, int month, int day) {
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; 
  if (isGregorianLeap(year)) daysInMonth[1] = 29; 
  int doy = 0;
  for (int i = 0; i < month - 1; i++) doy += daysInMonth[i];
  doy += day; 
  return doy;
}

bool isJalaliLeap(int jy) {
  if (jy < 1372) return (jy % 4 == 2); 
  else return (jy % 4 == 1); 
}

void gregorianToJalali(int gy, int gm, int gd, int &jy, int &jm, int &jd) {
  int doy = getDayOfYear(gy, gm, gd); 
  bool gregLeap = isGregorianLeap(gy); 
  int threshold = gregLeap ? 81 : 80; 
  if (doy < threshold) jy = gy - 622; 
  else jy = gy - 621; 

  int jalaliDayOfYear;
  if (doy >= threshold) { 
    jalaliDayOfYear = doy - threshold + 1; 
  } else {
    int prevYear = gy - 1; 
    int prevYearDays = isGregorianLeap(prevYear) ? 366 : 365; 
    jalaliDayOfYear = doy + (prevYearDays - threshold + 1); 
  }

  int monthDays[12] = {31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29}; 
  if (isJalaliLeap(jy)) monthDays[11] = 30; 

  int remain = jalaliDayOfYear;
  for (int i = 0; i < 12; i++) {
    if (remain <= monthDays[i]) {
      jm = i + 1;
      jd = remain; 
      return;
    }
    remain -= monthDays[i];
  }
  jm = 12; jd = 30; 
}

// ========== تابع محاسبه دقیق فاز ماه ==========
int getMoonPhase(int year, int month, int day) {
  if (month < 3) {
    year--;
    month += 12;
  }
  month++;
  long c = 365.25 * year;
  long e = 30.6 * month;
  long jd = c + e + day - 694039; 
  double cycles = (double)jd / 29.530588853;
  double frac = cycles - (long)cycles;
  if (frac < 0) frac += 1.0;
  int phase = (frac * 8.0) + 0.5;
  return phase % 8;
}

// ========== توابع مربوط به باتری ==========
float batteryVoltage = 3.7;  
unsigned long lastVoltageUpdate = 0; 

void updateBatteryVoltage() {
  unsigned long now = millis(); 
  if (now - lastVoltageUpdate >= 1000) {  
    lastVoltageUpdate = now; 
    int raw = analogRead(BATTERY_PIN); 
    float voltageAtPin = (raw / 1023.0) * 5.0; 
    batteryVoltage = voltageAtPin * VOLTAGE_DIVIDER; 
  }
}

void drawBatteryAndVoltage() {
  int x = 128 - 6 - 1; 
  int yTop = 2;          
  int height = 20; 

  oled.drawFrame(x, yTop, 6, height); 
  oled.drawBox(x + 2, yTop - 2, 2, 2); 

  int percent = constrain((int)((batteryVoltage - 3.0) / 1.2 * 100.0), 0, 100);
  int fillHeight = map(percent, 0, 100, 0, height - 2); 

  if (fillHeight > 0) { 
    for (int i = 0; i < fillHeight; i++) {
      oled.drawLine(x + 1, yTop + height - 1 - i, x + 4, yTop + height - 1 - i); 
    }
  }

  oled.setFont(u8g2_font_5x8_tn); 
  String voltStr = String(percent); 
  
  char voltBuf[6];
  voltStr.toCharArray(voltBuf, 6); 
  oled.drawStr(118, 24, voltBuf); 
}

// ========== مدیریت وضعیت‌ها ==========
enum AppMode { MODE_CLOCK, MODE_STOPWATCH }; 
AppMode currentMode = MODE_CLOCK; 

enum ClockDisplayState { SHOW_TIME, SHOW_DATE, SHOW_TEMP_HUM };
ClockDisplayState clockState = SHOW_TIME;

bool stopwatchRunning = false; 
unsigned long stopwatchStartMs = 0; 
unsigned long stopwatchElapsedMs = 0; 

bool invertMode = false; 
int lastSecond = -1; 

unsigned long touchStart = 0; 
bool wasTouching = false; 
unsigned long singleTapTime = 0; 
bool waitingForDoubleTap = false; 

char timeBuf[6]; 
char secBuf[3]; 
char dateBuf[12]; 
char stopwatchBuf[12]; 

void setup() {
  Serial.begin(9600); 
  Wire.begin(); 
  oled.begin(); 
  oled.setFontPosTop(); 
  dht.begin();          
  
  pinMode(TOUCH_PIN, INPUT); 
  pinMode(BATTERY_PIN, INPUT); 
  pinMode(LED_BUILTIN, OUTPUT); 

  // شروع بخش خوش‌آمدگویی
  oled.clearBuffer();
  oled.setFont(u8g2_font_10x20_tr);
  oled.drawStr(25, 0, "ARDUnia"); // خط اول
  oled.setFont(u8g2_font_10x20_tr);
  oled.drawStr(5, 15, "Smart Clock"); // خط دوم
  oled.sendBuffer();
  delay(5000); // 5 ثانیه وقفه برای نمایش پیام
  // پایان بخش خوش‌آمدگویی
  
  if (!rtc.begin()) while (1); 
  updateBatteryVoltage(); 

  lastDHTReadTime = millis();
}

void blinkLED() {
  digitalWrite(LED_BUILTIN, HIGH); 
  delay(50); 
  digitalWrite(LED_BUILTIN, LOW); 
}

void loop() {
  DateTime now = rtc.now(); 
  sprintf(timeBuf, "%02d:%02d", now.hour(), now.minute()); 
  sprintf(secBuf, "%02d", now.second()); 

  int jy, jm, jd;
  gregorianToJalali(now.year(), now.month(), now.day(), jy, jm, jd); 
  sprintf(dateBuf, "%04d/%02d/%02d", jy, jm, jd); 

  handleTouch(); 
  updateBatteryVoltage();  

  // ======= منطق زمانبندی غیرمسدودکننده خوانش سنسور DHT11 =======
  unsigned long currentMillis = millis();
  if (currentMillis - lastDHTReadTime >= DHT_READ_INTERVAL) {
    lastDHTReadTime = currentMillis;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if (!isnan(t) && !isnan(h)) {
      dhtTemperature = t;
      dhtHumidity = h;
    }
  }

  if (currentMode == MODE_CLOCK) { 
    if (now.second() != lastSecond) { 
      lastSecond = now.second(); 
    }
  }

  if (currentMode == MODE_STOPWATCH && stopwatchRunning) { 
    stopwatchElapsedMs = millis() - stopwatchStartMs; 
  }

  if (currentMode == MODE_CLOCK) { 
    if (clockState == SHOW_DATE) displayDate(); 
    else if (clockState == SHOW_TEMP_HUM) displayTempHum();
    else displayTime(now);
  } else {
    displayStopwatch(); 
  }

  delay(10); 
}

void displayTime(DateTime now) {
  oled.clearBuffer(); 
  setColorMode(); 

  oled.setFont(u8g2_font_logisoso28_tn); 
  int wTime = oled.getStrWidth(timeBuf); 
  oled.setFont(u8g2_font_9x15_tn); 
  int wSec = oled.getStrWidth(secBuf); 
  int gap = 2; 
  int totalW = wTime + gap + wSec; 
  int startX = (128 - totalW) / 2; 
  int yBig = 2; 
  int ySmall = yBig + 28 - 17;  

  oled.setFont(u8g2_font_logisoso28_tn); 
  oled.drawStr(startX, yBig, timeBuf); 

  // ======= منطق دو نقطه (:) چشمک‌زن هوشمند بدون لرزش متن =======
  if (now.second() % 2 != 0) { 
    char hourBuf[3];
    sprintf(hourBuf, "%02d", now.hour());
    int hourW = oled.getStrWidth(hourBuf); 
    int colonW = oled.getStrWidth(":");    
    
    if (invertMode) oled.setDrawColor(1); 
    else oled.setDrawColor(0); 
    
    oled.drawBox(startX + hourW, yBig, colonW, 30); 
    
    if (invertMode) oled.setDrawColor(0); 
    else oled.setDrawColor(1);
  }

  // رسم ثانیه‌شمار
  oled.setFont(u8g2_font_9x15_tn); 
  oled.drawStr(startX + wTime + gap, ySmall, secBuf); 

  // رسم فاز ماه (10x10 پیکسل)
  int currentPhase = getMoonPhase(now.year(), now.month(), now.day());
  int moonX = startX + wTime + gap + (wSec - 10) / 2; 
  int moonY = 2; 
  oled.drawBitmap(moonX, moonY, 2, 10, moon_phases + (currentPhase * 20));

  drawBatteryAndVoltage(); 
  oled.sendBuffer(); 
  resetColorMode(); 
}

void displayDate() {
  oled.clearBuffer(); 
  setColorMode(); 
  oled.setFont(u8g2_font_10x20_tn); 
  int w = oled.getStrWidth(dateBuf); 
  int x = (128 - w) / 2; 
  int y = (32 - 20) / 2; 
  oled.drawStr(x, y, dateBuf); 
  drawBatteryAndVoltage(); 
  oled.sendBuffer(); 
  resetColorMode(); 
}

void displayTempHum() {
  oled.clearBuffer();
  setColorMode();
  
  char tempHumBuf[20];
  if (dhtTemperature == 0.0 && dhtHumidity == 0.0) {
    strcpy(tempHumBuf, "Reading..."); // فقط در ۲ ثانیه اول راه‌اندازی نمایش داده می‌شود
  } else {
    sprintf(tempHumBuf, "%dC  %d%%", (int)dhtTemperature+TmpDef, (int)dhtHumidity+HumDef);
  }

  oled.setFont(u8g2_font_10x20_tr);
  int w = oled.getStrWidth(tempHumBuf);
  int x = (128 - w) / 2; 
  int y = (32 - 20) / 2;
  
  oled.drawStr(x, y+5, tempHumBuf);
  drawBatteryAndVoltage();
  oled.sendBuffer();
  resetColorMode();
}

void displayStopwatch() {
  unsigned long totalMs = stopwatchElapsedMs; 
  unsigned long totalSec = totalMs / 1000; 
  unsigned int minutes = totalSec / 60; 
  unsigned int seconds = totalSec % 60; 
  unsigned int hundredths = (totalMs % 1000) / 10; 
  sprintf(stopwatchBuf, "%02d:%02d.%02d", minutes, seconds, hundredths); 

  oled.clearBuffer(); 
  setColorMode(); 
  oled.setFont(u8g2_font_logisoso22_tn); 
  int w = oled.getStrWidth(stopwatchBuf); 
  int x = (128 - w) / 2; 
  int y = (32 - 22) / 2; 
  oled.drawStr(x, y, stopwatchBuf); 
  drawBatteryAndVoltage(); 
  oled.sendBuffer(); 
  resetColorMode(); 
}

// ========== مدیریت فرامین تاچ ==========
void handleTouch() {
  bool touching = digitalRead(TOUCH_PIN); 
#ifdef TOUCH_INVERT
  touching = !touching; 
#endif
  unsigned long nowMs = millis(); 

  if (touching && !wasTouching) { 
    wasTouching = true; 
    touchStart = nowMs; 
    blinkLED();
  }
  else if (!touching && wasTouching) { 
    wasTouching = false; 
    unsigned long duration = nowMs - touchStart; 
    blinkLED(); 

   // بررسی زمان لمس برای ریست
    if (duration >= 5000) { 
        asm volatile (" jmp 0"); 
    }
    if (duration >= 1000) { 
      if (currentMode == MODE_CLOCK) { 
        invertMode = !invertMode; 
      } else if (currentMode == MODE_STOPWATCH) { 
        stopwatchElapsedMs = 0; 
        stopwatchRunning = false; 
      }
      waitingForDoubleTap = false;  
      singleTapTime = 0; 
    } else {
      if (waitingForDoubleTap) { 
        waitingForDoubleTap = false; 
        if (currentMode == MODE_CLOCK) { 
          currentMode = MODE_STOPWATCH; 
          stopwatchRunning = false; 
          stopwatchElapsedMs = 0; 
        } else {
          currentMode = MODE_CLOCK; 
          DateTime now = rtc.now(); 
          lastSecond = now.second(); 
        }
      } else {
        waitingForDoubleTap = true; 
        singleTapTime = nowMs; 
      }
    }
  }

  if (waitingForDoubleTap && (nowMs - singleTapTime > 300)) { 
    waitingForDoubleTap = false; 
    
    if (currentMode == MODE_CLOCK) {
      if (clockState == SHOW_TIME) {
        clockState = SHOW_DATE;
      } else if (clockState == SHOW_DATE) {
        clockState = SHOW_TEMP_HUM;
      } else {
        clockState = SHOW_TIME;
        DateTime now = rtc.now();
        lastSecond = now.second();
      }
    } else if (currentMode == MODE_STOPWATCH) { 
      if (stopwatchRunning) stopwatchRunning = false; 
      else {
        stopwatchStartMs = millis() - stopwatchElapsedMs; 
        stopwatchRunning = true; 
      }
    }
  }
}

void setColorMode() {
  if (invertMode) { 
    oled.setDrawColor(1); 
    oled.drawBox(0, 0, 128, 32); 
    oled.setDrawColor(0); 
  } else {
    oled.setDrawColor(1); 
  }
}

void resetColorMode() {
  oled.setDrawColor(1); 
}

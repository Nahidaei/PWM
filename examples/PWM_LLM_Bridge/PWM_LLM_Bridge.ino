/*
 PWM_LLM_Bridge - drive PWM outputs from a fully local LLM ("no cloud, no API").

 A host computer (Raspberry Pi / Jetson / mini PC) runs a local LLM (e.g. Ollama)
 and the companion script bridge.py. The script turns natural language into short
 serial commands; this sketch applies them with the PWM library.

 Pin / timer map (Arduino Uno / Nano, ATmega328P):
   pin 9  - Timer1 (16 bit) @ 240 Hz, ~15 bit resolution -> warm LED (via MOSFET)
   pin 10 - Timer1 (shares pin 9's frequency)            -> cool LED (via MOSFET)
   pin 3  - Timer2 (8 bit)  @ 20 kHz, ~5.6 bit resolution -> fan / DC motor (via driver)
   pin 11 - NOT usable: once Timer2's frequency is changed, OCR2A becomes TOP
   Timer0 (pins 5, 6) is left untouched so millis()/delay() keep working.

 Serial protocol (115200 baud, one command per line):
   W <0-100>   warm LED percent        -> "OK W <v>"
   C <0-100>   cool LED percent        -> "OK C <v>"
   F <0-100>   fan percent             -> "OK F <v>"
   P           heartbeat               -> "OK P"
   S           stop all outputs        -> "OK S"
   ?           status                  -> "ST W<v> C<v> F<v>"

 Safety: values are clamped to 0-100 and, if no command (including the "P"
 heartbeat) arrives for WATCHDOG_MS, every output is switched off.

 Runs on ATmega328P boards (Uno, Nano). For servos (50 Hz) use a Mega 2560 and
 its extra 16-bit timers, or an external PCA9685 board.
 */

#include <PWM.h>

const uint8_t PIN_WARM = 9;    // Timer1
const uint8_t PIN_COOL = 10;   // Timer1 (same frequency as pin 9)
const uint8_t PIN_FAN  = 3;    // Timer2 (pin 11 unusable after this)

const uint32_t LED_FREQ_HZ = 240;     // flicker free, ~15 bits of resolution
const uint32_t FAN_FREQ_HZ = 20000;   // above human hearing, no motor whine
const unsigned long WATCHDOG_MS = 3000;

uint8_t level[3] = {0, 0, 0};  // warm, cool, fan in percent
char buf[24];
uint8_t len = 0;
unsigned long lastCmd = 0;

// Perceptually even LED dimming (gamma 2.2), mapped to 0-65535 for pwmWriteHR
uint16_t gammaHR(uint8_t pct)
{
  float x = pct / 100.0;
  return (uint16_t)(pow(x, 2.2) * 65535.0 + 0.5);
}

void apply()
{
  pwmWriteHR(PIN_WARM, gammaHR(level[0]));
  pwmWriteHR(PIN_COOL, gammaHR(level[1]));
  pwmWriteHR(PIN_FAN, (uint16_t)(level[2] * 655.35 + 0.5));  // linear for motors
}

void stopAll()
{
  level[0] = level[1] = level[2] = 0;
  apply();
}

void setup()
{
  Serial.begin(115200);

  InitTimersSafe();                                        // keep Timer0 for millis()
  bool ok = SetPinFrequencySafe(PIN_WARM, LED_FREQ_HZ);    // sets pins 9 and 10
  ok &= SetPinFrequencySafe(PIN_FAN, FAN_FREQ_HZ);
  stopAll();

  Serial.print(ok ? F("READY") : F("ERR FREQ"));
  Serial.print(F(" resLED="));
  Serial.print(GetPinResolution(PIN_WARM));
  Serial.print(F(" resFAN="));
  Serial.println(GetPinResolution(PIN_FAN));
  lastCmd = millis();
}

void handle(char *line)
{
  char cmd = line[0];
  int v = constrain(atoi(line + 1), 0, 100);   // hard clamp on the MCU as well

  switch (cmd) {
    case 'W': level[0] = v; break;
    case 'C': level[1] = v; break;
    case 'F': level[2] = v; break;
    case 'P':
      Serial.println(F("OK P"));
      return;
    case 'S':
      stopAll();
      Serial.println(F("OK S"));
      return;
    case '?':
      Serial.print(F("ST W")); Serial.print(level[0]);
      Serial.print(F(" C"));   Serial.print(level[1]);
      Serial.print(F(" F"));   Serial.println(level[2]);
      return;
    default:
      Serial.println(F("ERR CMD"));
      return;
  }

  apply();
  Serial.print(F("OK "));
  Serial.print(cmd);
  Serial.print(' ');
  Serial.println(v);
}

void loop()
{
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len) {
        buf[len] = 0;
        handle(buf);
        len = 0;
        lastCmd = millis();
      }
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    }
  }

  // Host crashed or model hung: fail safe
  if (millis() - lastCmd > WATCHDOG_MS && (level[0] | level[1] | level[2])) {
    stopAll();
    Serial.println(F("WDT STOP"));
  }
}

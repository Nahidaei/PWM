# PWM.h

v 5.0

Created by Sam Knight

Modify PWM frequency on AVR (arduino) platform



Modified by Terry Myers:

1. Modified examples to be in their own folders
2. Corrected a line feed character in two of the libraries so that visual studio would not error when opening them
3. Added README.MD, library.properties, and library.json files to better "integrate" to IDEs espencially Arduino IDE for begginers
4. Added new example for high resolution control of an LED
5. Added "{ 0, 0, 0, 0, 0 }, //TIMER1C" to line 107 in ATimerDefs.cpp as suggested by texmit from arduino forms

Examples:

- `PWM` - fade with a custom frequency
- `PWM_pwmWriteHR_LED` - high resolution LED dimming
- `PWM_resolution_example` - frequency / resolution combinations
- `PWM_LLM_Bridge` - control PWM outputs in plain language from a fully local LLM (no cloud, no API) over serial, with a watchdog failsafe

Original repository
https://code.google.com/archive/p/arduino-pwm-frequency-library/downloads

Original Arduino Forum library post by Runnerup (Sam Knight):
https://forum.arduino.cc/index.php?topic=117425.0

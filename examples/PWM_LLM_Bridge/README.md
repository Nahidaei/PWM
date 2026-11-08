# PWM_LLM_Bridge

Control PWM outputs with plain language using a **fully local LLM**: no cloud, no API keys, works offline.

```
 you (text/voice) ─► host (Pi 5 / Jetson / PC)                     ─► Arduino Uno/Nano + PWM.h
                     Ollama + small model (e.g. qwen2.5:3b)            pin 9  warm LED  240 Hz, ~15 bit
                     bridge.py: JSON schema → validate → serial        pin 10 cool LED  240 Hz (shared Timer1)
                                 heartbeat every 1 s                   pin 3  fan       20 kHz, ~5.6 bit
                                                                       watchdog: 3 s silence → all off
```

The LLM works out what you meant, `bridge.py` decides whether it is allowed, and the Arduino handles real-time PWM and the failsafe.

## Files
| File | Runs on | Purpose |
|---|---|---|
| `PWM_LLM_Bridge.ino` | Arduino Uno / Nano | Serial command parser, `pwmWriteHR` outputs, watchdog |
| `bridge.py` | Host computer | Local LLM (Ollama) → validated serial commands |

## Pin / timer notes (ATmega328P)
- **Pins 9 and 10** share Timer1, so they always run at the same frequency.
- **Pin 11 is unusable** once Timer2's frequency is changed, because `OCR2A` becomes the timer TOP.
- **Timer0** (pins 5 and 6) is left alone by `InitTimersSafe()` so `millis()` keeps working.
- **Servos** need 50 Hz. Use a Mega 2560 (Timers 3/4/5) or a PCA9685 board.

## Serial protocol (115200 baud)
| Command | Meaning | Reply |
|---|---|---|
| `W <0-100>` | Warm LED % | `OK W 40` |
| `C <0-100>` | Cool LED % | `OK C 0` |
| `F <0-100>` | Fan % | `OK F 75` |
| `P` | Heartbeat | `OK P` |
| `S` | Stop all | `OK S` |
| `?` | Status | `ST W40 C0 F75` |

You can also test the sketch without an LLM: open the Serial Monitor (115200 baud, newline line ending) and type the commands.

## Run
1. Upload `PWM_LLM_Bridge.ino` to the board.
2. On the host:
   ```sh
   pip install pyserial requests
   ollama pull qwen2.5:3b
   python3 bridge.py --port /dev/ttyACM0    # Windows: --port COM3
   ```
3. Type, for example: `make it cozy and turn the fan down a bit`.

## Wiring and safety
- Drive the LED strips through logic-level MOSFETs (e.g. IRLZ44N, 220 Ω gate resistor, 10 kΩ pulldown).
- Drive the fan or motor through a driver (e.g. TB6612FNG) or a MOSFET with a flyback diode.
- Use a separate 12 V supply for the loads and connect **its ground to the Arduino's ground**.
- Safety layers: JSON schema output → bridge validation → clamping on the MCU → watchdog.

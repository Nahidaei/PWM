#!/usr/bin/env python3
"""Local LLM -> Arduino PWM bridge ("no cloud, no API").

Runs on the host computer next to the Arduino. Sends your text to a local
Ollama model, forces the answer into a JSON schema, validates it, and sends
short serial commands to the PWM_LLM_Bridge sketch.

Setup:
    pip install pyserial requests
    ollama pull qwen2.5:3b
    python3 bridge.py --port /dev/ttyACM0      # Windows: --port COM3
"""

import argparse
import json
import threading
import time

import requests
import serial

SCHEMA = {
    "type": "object",
    "properties": {
        "actions": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "channel": {"enum": ["warm", "cool", "fan"]},
                    "level": {"type": "integer", "minimum": 0, "maximum": 100},
                },
                "required": ["channel", "level"],
            },
        },
        "reply": {"type": "string"},
    },
    "required": ["actions", "reply"],
}

SYSTEM = (
    "You control a room device with channels: warm (warm light), cool (cool light), "
    "fan. Levels are 0-100 percent. Respond only with JSON matching the schema. "
    "If the request is unrelated, return an empty actions list and explain in reply."
)

CODE = {"warm": "W", "cool": "C", "fan": "F"}
MAX_ACTIONS = 3


class Board:
    """Thread-safe serial link with a heartbeat that feeds the sketch's watchdog."""

    def __init__(self, port, baud=115200):
        self.ser = serial.Serial(port, baud, timeout=1)
        self.lock = threading.Lock()
        time.sleep(2)  # the board resets when the port opens
        print("board:", self.ser.readline().decode(errors="replace").strip())
        threading.Thread(target=self._heartbeat, daemon=True).start()

    def send(self, line):
        with self.lock:
            self.ser.write((line + "\n").encode())
            return self.ser.readline().decode(errors="replace").strip()

    def _heartbeat(self):
        while True:
            self.send("P")
            time.sleep(1)


def validate(actions):
    """Keep only well-formed actions; never trust the model blindly."""
    safe = []
    for a in actions:
        if not isinstance(a, dict):
            continue
        ch, lv = a.get("channel"), a.get("level")
        if ch in CODE and isinstance(lv, int) and not isinstance(lv, bool) and 0 <= lv <= 100:
            safe.append((CODE[ch], lv))
    return safe[:MAX_ACTIONS]


def ask(text, model, url):
    r = requests.post(
        f"{url}/api/chat",
        json={
            "model": model,
            "stream": False,
            "format": SCHEMA,
            "messages": [
                {"role": "system", "content": SYSTEM},
                {"role": "user", "content": text},
            ],
        },
        timeout=120,
    )
    r.raise_for_status()
    return json.loads(r.json()["message"]["content"])


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default="/dev/ttyACM0")
    p.add_argument("--model", default="qwen2.5:3b")
    p.add_argument("--ollama", default="http://localhost:11434")
    args = p.parse_args()

    board = Board(args.port)
    print("Type a request (e.g. 'make it cozy, fan low'), 'stop', or Ctrl+C to quit.")
    try:
        while True:
            text = input("you> ").strip()
            if not text:
                continue
            if text.lower() == "stop":
                print("  ->", board.send("S"))
                continue
            try:
                data = ask(text, args.model, args.ollama)
            except (requests.RequestException, ValueError, KeyError) as e:
                print("LLM error:", e)
                continue
            for code, lv in validate(data.get("actions", [])):
                print("  ->", board.send(f"{code} {lv}"))
            print("bot>", data.get("reply", ""))
    except (KeyboardInterrupt, EOFError):
        print("\nstopping outputs")
        board.send("S")


if __name__ == "__main__":
    main()

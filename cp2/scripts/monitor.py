"""Monitor ao vivo das inferências da ESP32 (para demonstração / vídeo).

Mostra, linha a linha, a distância lida pelo sensor, as probabilidades do modelo
e a classe escolhida, com cores. Ctrl+C para sair.

    python scripts/monitor.py            # acha a porta da ESP32 sozinho
    python scripts/monitor.py COM5
"""
import os
import re
import sys
import time
from pathlib import Path

import serial

sys.path.insert(0, str(Path(__file__).resolve().parent))
from coleta_guiada import achar_porta_esp32  # noqa: E402

os.system("")  # liga as cores ANSI no console do Windows
COR = {"livre": "\033[92m", "aproximando": "\033[93m", "obstaculo": "\033[91m", "incerto": "\033[90m"}
FIM = "\033[0m"


def main():
    porta = sys.argv[1] if len(sys.argv) > 1 else achar_porta_esp32()
    if not porta:
        print("Nao encontrei a ESP32 (carrinho ligado? USB-C da camera no PC?).")
        return
    print(f"ESP32 na {porta} - Ctrl+C para sair\n")
    print(f"{'sensor':>8}  {'aproximando':>11} {'livre':>6} {'obstaculo':>9}   classe do modelo")
    ultima = "--"
    with serial.Serial(porta, 115200, timeout=1) as ser:
        while True:
            linha = ser.readline().decode(errors="ignore").strip()
            m = re.fullmatch(r"\d+,(\d+)", linha)
            if m:
                ultima = m.group(1)
                continue
            m = re.match(r"# aproximando=([\d.]+) livre=([\d.]+) obstaculo=([\d.]+).*-> (\w+)", linha)
            if m:
                a, l, o, classe = m.groups()
                print(f"{ultima:>5} cm  {float(a):>11.2f} {float(l):>6.2f} {float(o):>9.2f}   "
                      f"{COR.get(classe, '')}{classe.upper()}{FIM}")
            elif linha.startswith("#"):
                print(linha)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
    except serial.SerialException as e:
        print(f"Erro na porta serial: {e}")
        time.sleep(3)

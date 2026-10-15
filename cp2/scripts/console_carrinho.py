"""Envia comandos JSON à R3 através da ESP32 da câmera e mostra a saída da ESP32.

A ESP32 (esp32/carrinho_tinyml) repassa à R3 tudo que chega pela USB entre { e }.

Exemplos:
    python scripts/console_carrinho.py --porta COM5 --segundos 5                   # só escuta
    python scripts/console_carrinho.py --porta COM5 '{"N":101,"D1":4}' --segundos 3 --parar
"""
import argparse
import time

import serial


def ler_por(ser, segundos):
    buf = b""
    fim = time.time() + segundos
    while time.time() < fim:
        n = ser.in_waiting
        if n:
            buf += ser.read(n)
        time.sleep(0.02)
    return buf.decode(errors="ignore").splitlines()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--porta", required=True)
    p.add_argument("comandos", nargs="*", help='ex.: {"N":101,"D1":4}')
    p.add_argument("--segundos", type=float, default=3.0, help="tempo escutando após os comandos")
    p.add_argument("--parar", action="store_true", help='manda {"N":100} (standby) no fim')
    p.add_argument("--amostras", action="store_true", help="mostra também as linhas millis,cm")
    a = p.parse_args()

    with serial.Serial(a.porta, 115200, timeout=0, write_timeout=1) as ser:
        ser.reset_input_buffer()
        for c in a.comandos:
            ser.write(c.encode())
            time.sleep(0.1)
        linhas = ler_por(ser, a.segundos)
        if a.parar:
            for _ in range(3):
                ser.write(b'{"N":100}')
                time.sleep(0.1)
            linhas += ler_por(ser, 0.5)

    amostras = [l for l in linhas if l[:1].isdigit()]
    for l in linhas:
        if a.amostras or not l[:1].isdigit():
            print(l)
    if amostras:
        print(f"[{len(amostras)} amostras; últimas: {amostras[-5:]}]")


if __name__ == "__main__":
    main()

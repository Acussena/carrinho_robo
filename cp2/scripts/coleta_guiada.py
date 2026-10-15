"""Coleta guiada das 3 classes, com instruções na tela. Usa scripts/coleta_serial.py.

Rodar com dois cliques em COLETAR.bat (pasta do projeto).
Antes de cada amostra mostra a distância ao vivo; depois confere se a amostra
combina com a classe e oferece refazer.
"""
import csv
import re
import sys
import threading
import time
from pathlib import Path

import serial
from serial.tools import list_ports

sys.path.insert(0, str(Path(__file__).resolve().parent))
from coleta_serial import RAIZ, ler_amostra  # noqa: E402

AMOSTRAS = 20
DURACAO = 2.0

CLASSES = [
    ("livre",
     "Vire a frente do carrinho (o sensor) para um espaco ABERTO,\n"
     "  sem nada a menos de 1 metro (o sensor deve marcar perto de 100-150 cm).\n"
     "  A cada amostra, mude um pouco a direcao ou o lugar.",
     lambda v: min(v) >= 70,
     "tem algo a menos de 70 cm - aponte para um espaco mais aberto"),
    ("aproximando",
     "Comece com a caixa (ou a mao) a ~1 metro do sensor. Aperte Enter e LOGO\n"
     "  EM SEGUIDA aproxime ate ~25 cm, durante os 2 segundos.\n"
     "  Varie a velocidade: as vezes devagar, as vezes rapido.",
     lambda v: v[0] - v[-1] >= 25 and v[0] >= 50,
     "a distancia nao caiu o suficiente - comece mais longe (~1 m) e aproxime durante a gravacao"),
    ("obstaculo",
     "Deixe uma caixa, parede ou a mao PARADA bem perto do sensor (5 a 25 cm).\n"
     "  A cada amostra, mude o objeto, a distancia ou o angulo.",
     lambda v: max(v) <= 30,
     "teve leitura acima de 30 cm - deixe o objeto parado mais perto"),
]


def achar_porta_esp32():
    portas = [p.device for p in list_ports.comports() if "CH340" in p.description]
    for porta in portas:
        try:
            with serial.Serial(porta, 115200, timeout=0) as s:
                bruto = b""
                fim = time.time() + 6  # se a ESP32 reiniciar ao abrir a porta, ela religa o envio da R3
                while time.time() < fim:
                    bruto += s.read(s.in_waiting or 1)
                    if re.search(rb"\n\d+,\d+\r?\n", bruto):
                        return porta
                    time.sleep(0.02)
        except serial.SerialException:
            pass
    return None


def esperar_enter_mostrando_distancia(ser, texto):
    """Mostra a distância atual na mesma linha até o usuário apertar Enter."""
    resposta = []
    t = threading.Thread(target=lambda: resposta.append(input()), daemon=True)
    ultima = "--"
    t.start()
    while t.is_alive():
        n = ser.in_waiting
        if n:
            for linha in ser.read(n).decode(errors="ignore").splitlines():
                m = re.fullmatch(r"\d+,(\d+)", linha.strip())
                if m:
                    ultima = m.group(1)
        print(f"\r{texto}  [sensor agora: {ultima:>3} cm]   ", end="", flush=True)
        time.sleep(0.1)
    print()
    return resposta[0] if resposta else ""


def main():
    print("Procurando a ESP32 da camera...")
    porta = achar_porta_esp32()
    if not porta:
        print("\nNao encontrei a ESP32 enviando leituras.")
        print("Confira: carrinho ligado, cabo USB-C da camera no PC, nenhum outro programa usando a porta.")
        input("\nEnter para sair.")
        return
    print(f"ESP32 encontrada na {porta}.\n")

    with serial.Serial(porta, 115200, timeout=1) as ser:
        time.sleep(1)
        for label, instrucao, confere, dica in CLASSES:
            pasta = RAIZ / "dados" / "csv" / label
            pasta.mkdir(parents=True, exist_ok=True)
            ja_tem = len(list(pasta.glob(f"{label}.*.csv")))
            print("=" * 70)
            print(f" CLASSE: {label.upper()}   ({ja_tem} amostras ja gravadas, meta {AMOSTRAS})")
            print("=" * 70)
            print(f"  {instrucao}\n")
            if ja_tem >= AMOSTRAS:
                print("  Ja tem amostras suficientes, pulando.\n")
                continue
            resp = input(f"Enter para comecar '{label}' (ou digite p e Enter para pular): ")
            if resp.strip().lower() == "p":
                continue

            n = ja_tem
            while n < AMOSTRAS:
                esperar_enter_mostrando_distancia(ser, f"  [{n + 1}/{AMOSTRAS}] posicione e aperte Enter (grava 2 s)")
                dados = ler_amostra(ser, DURACAO, 1)
                if len(dados) < 30:
                    print(f"    so chegaram {len(dados)} leituras, tente de novo")
                    continue
                v = [d[1] for d in dados]
                print(f"    inicio={v[0]:.0f} cm  fim={v[-1]:.0f} cm  min={min(v):.0f}  max={max(v):.0f}")
                if not confere(v):
                    r = input(f"    ATENCAO: {dica}.\n    Enter = descartar e refazer | g = gravar mesmo assim: ")
                    if r.strip().lower() != "g":
                        continue
                n += 1
                arq = pasta / f"{label}.{n:03d}.csv"
                with arq.open("w", newline="") as f:
                    w = csv.writer(f)
                    w.writerow(["timestamp", "distancia"])
                    w.writerows([[t, int(x)] for t, x in dados])
                print(f"    gravada: {arq.name}")
            print()

    print("Coleta terminada! Pode avisar o Claude.")
    input("Enter para sair.")


if __name__ == "__main__":
    main()

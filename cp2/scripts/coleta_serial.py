"""Coleta leituras do sensor via Serial e salva amostras CSV no formato do Edge Impulse.

O ESP32 deve imprimir uma linha por leitura no formato:
    <millis>,<valor1>[,<valor2>...]

Cada execução grava N amostras de `--duracao` segundos em dados/csv/<label>/<label>.<n>.csv,
com a coluna `timestamp` (ms, começando em 0) seguida das colunas do sensor.
O nome "<label>.<algo>.csv" faz o Edge Impulse usar o label automaticamente no upload.

Exemplo:
    python scripts/coleta_serial.py --porta COM5 --label obstaculo_perto --colunas distancia --amostras 20
"""
import argparse
import csv
import time
from pathlib import Path

import serial
from serial.tools import list_ports

RAIZ = Path(__file__).resolve().parent.parent


def ler_amostra(ser, duracao, n_colunas):
    linhas = []
    fim = time.time() + duracao
    ser.reset_input_buffer()
    while time.time() < fim:
        bruto = ser.readline().decode(errors="ignore").strip()
        partes = bruto.split(",")
        if len(partes) != n_colunas + 1:
            continue
        try:
            valores = [float(p) for p in partes]
        except ValueError:
            continue
        linhas.append(valores)
    if not linhas:
        return []
    t0 = linhas[0][0]
    return [[int(l[0] - t0)] + l[1:] for l in linhas]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--porta", help="ex.: COM5 (omitir para listar portas)")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--label", required=False, help="classe da amostra, ex.: livre, obstaculo_perto")
    p.add_argument("--colunas", default="distancia", help="nomes separados por vírgula")
    p.add_argument("--duracao", type=float, default=2.0, help="segundos por amostra")
    p.add_argument("--amostras", type=int, default=10)
    a = p.parse_args()

    if not a.porta or not a.label:
        print("Portas disponíveis:")
        for porta in list_ports.comports():
            print(f"  {porta.device}  {porta.description}")
        print("\nUse --porta e --label para coletar.")
        return

    colunas = [c.strip() for c in a.colunas.split(",")]
    pasta = RAIZ / "dados" / "csv" / a.label
    pasta.mkdir(parents=True, exist_ok=True)
    inicio = len(list(pasta.glob(f"{a.label}.*.csv")))

    with serial.Serial(a.porta, a.baud, timeout=1) as ser:
        time.sleep(3)  # ESP32 reinicia ao abrir a porta e religa o envio da R3
        for i in range(a.amostras):
            input(f"[{i + 1}/{a.amostras}] Posicione o carrinho para '{a.label}' e pressione Enter...")
            dados = ler_amostra(ser, a.duracao, len(colunas))
            if not dados:
                print("  nenhuma leitura válida, confira o formato da Serial")
                continue
            arq = pasta / f"{a.label}.{inicio + i + 1:03d}.csv"
            with arq.open("w", newline="") as f:
                w = csv.writer(f)
                w.writerow(["timestamp"] + colunas)
                w.writerows(dados)
            valores = [d[1] for d in dados]
            print(f"  {len(dados)} leituras  min={min(valores):.0f} max={max(valores):.0f} "
                  f"media={sum(valores) / len(valores):.0f}  inicio={valores[0]:.0f} fim={valores[-1]:.0f}"
                  f"  -> {arq.relative_to(RAIZ)}")


if __name__ == "__main__":
    main()

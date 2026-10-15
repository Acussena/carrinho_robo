"""Coleta contínua guiada por fases: grava o sensor sem parar e recorta amostras rotuladas.

1. Grava ~60 s de leituras reais (20 Hz). A tela guia 3 fases, e a fase define a classe:
     obstaculo   (15 s) -> segurar um objeto PARADO perto do sensor
     livre       (15 s) -> deixar o sensor ver um espaço aberto
     aproximando (30 s) -> aproximar um objeto de ~1 m até perto, afastar, repetir
2. Salva a gravação inteira em dados/brutos/sessao_<data>.csv (evidência da coleta),
   com a coluna "fase".
3. Recorta janelas de 1 s (passo de 0,5 s) dentro de cada fase e só mantém as que
   conferem com a classe (ex.: na fase "aproximando", os trechos em que o objeto se
   afasta são descartados). Regras: livre = tudo >= 60 cm; obstaculo = tudo <= 30 cm;
   aproximando = cai >= 15 cm em 1 s, sem voltar a subir. A conferência usa a distância com filtro de mediana, que
   ignora leituras soltas sem eco; a amostra salva é a leitura original do sensor.
4. Grava as janelas em dados/csv/<classe>/<classe>.s<sessao>_<n>.csv (formato Edge Impulse).

Rodar com dois cliques em COLETAR_RAPIDO.bat.
"""
import csv
import re
import statistics
import sys
import time
from datetime import datetime
from pathlib import Path

import serial

sys.path.insert(0, str(Path(__file__).resolve().parent))
from coleta_guiada import achar_porta_esp32  # noqa: E402
from coleta_serial import RAIZ  # noqa: E402

FASES = [
    (15, "obstaculo", "SEGURE a caixa/mao PARADA perto do sensor (10 a 20 cm)"),
    (15, "livre", "TIRE tudo da frente: o sensor deve ver um espaco ABERTO (> 1 m)"),
    (30, "aproximando", "APROXIME de ~1 m ate perto, AFASTE, e repita (varie a velocidade)"),
]
JANELA = 20        # 1 s a 20 Hz
PASSO = 10         # 0,5 s
TRANSICAO = 20     # ignora o 1o segundo de cada fase (tempo de reagir à tela)
CLASSES = ("livre", "aproximando", "obstaculo")
META = 20


def mediana_movel(v, k=5):
    return [statistics.median(v[max(0, i - k // 2):i + k // 2 + 1]) for i in range(len(v))]


def rotular(v):
    """Classe de uma janela pelas regras (ou None se não se encaixar em nenhuma)."""
    f = mediana_movel(v)
    inicio, fim = sum(f[:4]) / 4, sum(f[-4:]) / 4
    if min(f) >= 60:
        return "livre"
    if max(f) <= 30:
        return "obstaculo"
    subidas = sum(1 for a, b in zip(f, f[1:]) if b - a > 5)
    if inicio >= 30 and inicio - fim >= 15 and subidas <= 2:
        return "aproximando"
    return None


def gravar(ser):
    leituras = []  # (t, distancia, fase)
    ser.reset_input_buffer()
    resto = ""
    for duracao, classe, instrucao in FASES:
        print(f"\n>>> {instrucao}")
        fim = time.time() + duracao
        while time.time() < fim:
            n = ser.in_waiting
            if n:
                resto += ser.read(n).decode(errors="ignore")
                *linhas, resto = resto.split("\n")
                for linha in linhas:
                    m = re.fullmatch(r"(\d+),(\d+)", linha.strip())
                    if m:
                        leituras.append((int(m.group(1)), int(m.group(2)), classe))
            ultima = leituras[-1][1] if leituras else 0
            print(f"\r    [{classe}] faltam {int(fim - time.time()) + 1:2d} s   sensor: {ultima:3d} cm   ",
                  end="", flush=True)
            time.sleep(0.05)
    print()
    return leituras


def recortar(leituras, sessao):
    contagem = dict.fromkeys(CLASSES, 0)
    inicio_fase = 0
    for i in range(1, len(leituras) + 1):
        if i < len(leituras) and leituras[i][2] == leituras[inicio_fase][2]:
            continue
        fase = leituras[inicio_fase]
        for j in range(inicio_fase + TRANSICAO, i - JANELA + 1, PASSO):
            trecho = leituras[j:j + JANELA]
            if rotular([d for _, d, _ in trecho]) != fase[2]:
                continue
            contagem[fase[2]] += 1
            pasta = RAIZ / "dados" / "csv" / fase[2]
            pasta.mkdir(parents=True, exist_ok=True)
            with (pasta / f"{fase[2]}.s{sessao}_{contagem[fase[2]]:03d}.csv").open("w", newline="") as f:
                w = csv.writer(f)
                w.writerow(["timestamp", "distancia"])
                w.writerows([(t - trecho[0][0], d) for t, d, _ in trecho])
        inicio_fase = i
    return contagem


def main():
    print("Procurando a ESP32 da camera...")
    porta = achar_porta_esp32()
    if not porta:
        print("Nao encontrei a ESP32 enviando leituras (carrinho ligado? USB-C da camera no PC?).")
        return
    print(f"ESP32 na {porta}.\n")
    print("Com o carrinho PARADO, siga as instrucoes que vao aparecer na tela:")
    for duracao, classe, instrucao in FASES:
        print(f"  {duracao:2d} s  {instrucao}")
    input("\nTenha uma caixa (ou a mao) pronta e aperte Enter para comecar...")

    with serial.Serial(porta, 115200, timeout=0) as ser:
        leituras = gravar(ser)
    if len(leituras) < 200:
        print("Quase nenhuma leitura chegou. Confira o carrinho e tente de novo.")
        return

    sessao = datetime.now().strftime("%Y%m%d_%H%M%S")
    brutos = RAIZ / "dados" / "brutos"
    brutos.mkdir(parents=True, exist_ok=True)
    t0 = leituras[0][0]
    with (brutos / f"sessao_{sessao}.csv").open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["timestamp", "distancia", "fase"])
        w.writerows([(t - t0, d, fase) for t, d, fase in leituras])

    contagem = recortar(leituras, sessao)
    total = {c: len(list((RAIZ / "dados" / "csv" / c).glob("*.csv"))) for c in CLASSES}
    print(f"\n{len(leituras)} leituras gravadas em dados/brutos/sessao_{sessao}.csv")
    print("Amostras aproveitadas nesta sessao: " + ", ".join(f"{c}={n}" for c, n in contagem.items()))
    print("Total no dataset:                   " + ", ".join(f"{c}={n}" for c, n in total.items()))
    falta = [c for c, n in total.items() if n < META]
    if falta:
        print(f"\nAinda faltam amostras de: {', '.join(falta)}. Rode de novo (mais 1 minuto).")
    else:
        print(f"\nDataset com {META}+ amostras por classe. Pode avisar o Claude!")


if __name__ == "__main__":
    main()

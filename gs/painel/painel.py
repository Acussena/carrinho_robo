"""
GS CitizenTech - Painel do "Fiscal de Calçadas" com IA (LLM Gemini + voz).

Recebe a telemetria e as ocorrências da ESP32 do carrinho e:
  1. mostra tudo ao vivo num painel web (http://localhost:8000);
  2. manda cada ocorrência (obstáculo, desnível, trecho crítico, bateria, falha) para um LLM,
     que classifica a gravidade e gera um alerta em linguagem natural, falado pelo navegador;
  3. responde perguntas sobre a patrulha ("a calçada é acessível?");
  4. gera um relatório para a gestão pública (Markdown) e salva o log das ocorrências (CSV).

Sem chave de API ou sem internet, a IA cai para regras locais (modo offline) e o painel
continua funcionando - o robô em si nunca depende do painel.

Fontes de dados:
  python painel.py --ip 192.168.0.50      ESP32 pelo Wi-Fi (API /status, /eventos, /cmd)
  python painel.py --porta COM5           ESP32 pela USB (linhas @T e @E)
  python painel.py --simular              sem robô: dados simulados (para testar o painel e a IA)

Chave do Gemini (gratuita em https://aistudio.google.com/apikey): arquivo .env nesta pasta
  GEMINI_API_KEY=sua_chave
"""

import argparse
import csv
import json
import os
import queue
import random
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PASTA = os.path.dirname(os.path.abspath(__file__))
PASTA_SAIDA = os.path.join(PASTA, "saida")

EVENTOS_ANALISADOS = {"obstaculo", "desnivel", "trecho_critico", "bateria_baixa",
                      "falha_r3", "r3_reconectada", "sensor_piso_indisponivel"}

DESCRICAO_EVENTO = {
    "obstaculo": "o modelo TinyML (sensor ultrassônico) reconheceu um objeto parado bloqueando a passagem à frente do robô",
    "desnivel": "o sensor de piso (seguidor de linha infravermelho voltado para o chão) deixou de ver o piso à frente: buraco, degrau, meio-fio sem rampa ou borda",
    "trecho_critico": "o robô encontrou 2 ou mais barreiras em menos de 30 segundos e entrou sozinho em modo cauteloso (velocidade reduzida)",
    "bateria_baixa": "a bateria do robô ficou abaixo de 7,0 V por mais de 5 segundos; ele reduziu a velocidade para economizar",
    "falha_r3": "a ESP32 parou de receber dados da placa R3 (cabo solto ou R3 reiniciando); os motores param sozinhos por segurança",
    "r3_reconectada": "a comunicação com a placa R3 voltou ao normal",
    "sensor_piso_indisponivel": "a placa R3 não está enviando o sensor de piso (firmware antigo); o robô segue apenas com o ultrassônico",
    "patrulha": "a patrulha foi iniciada ou parada pelo operador",
}


# ============================ utilidades ============================

def carregar_env():
    caminho = os.path.join(PASTA, ".env")
    if not os.path.exists(caminho):
        return
    with open(caminho, encoding="utf-8") as f:
        for linha in f:
            linha = linha.strip()
            if not linha or linha.startswith("#") or "=" not in linha:
                continue
            chave, valor = linha.split("=", 1)
            os.environ.setdefault(chave.strip(), valor.strip().strip('"').strip("'"))


def agora():
    return datetime.now().strftime("%H:%M:%S")


# ============================ estado compartilhado ============================

class Estado:
    def __init__(self, local):
        self.trava = threading.Lock()
        self.local = local
        self.status = {}
        self.ultima_atualizacao = 0.0
        self.ocorrencias = []      # eventos vindos do robô, já com a análise da IA
        self.alertas = []          # mensagens para falar/mostrar (id crescente)
        self.proximo_alerta = 1
        self.fonte = ""
        self.novo_log()

    def novo_log(self):
        self.inicio = datetime.now()
        os.makedirs(PASTA_SAIDA, exist_ok=True)
        self.arquivo_csv = os.path.join(PASTA_SAIDA, "ocorrencias_%s.csv" % self.inicio.strftime("%Y%m%d_%H%M%S"))
        with open(self.arquivo_csv, "w", newline="", encoding="utf-8") as f:
            csv.writer(f).writerow(["hora", "id", "tipo", "dist_cm", "piso_l", "piso_m", "piso_r",
                                    "bateria_v", "confianca", "severidade", "alerta", "recomendacao", "fonte_ia"])

    def reiniciar(self):
        """Zera ocorrências, alertas e contadores (nova patrulha/gravação); a conexão com o robô continua."""
        with self.trava:
            self.ocorrencias = []
            self.alertas = []
            self.novo_log()
        self.adicionar_alerta("Painel reiniciado. Pronto para uma nova patrulha.", "info", True, "sistema", interromper=True)

    def contagem(self):
        cont = {}
        for ev in self.ocorrencias:
            cont[ev["tipo"]] = cont.get(ev["tipo"], 0) + 1
        return cont

    def atualizar_status(self, status):
        with self.trava:
            self.status = status
            self.ultima_atualizacao = time.time()

    def adicionar_ocorrencia(self, ev):
        with self.trava:
            ev["hora"] = agora()
            ev["recebido"] = time.time()
            ev["analise"] = None
            self.ocorrencias.append(ev)
            return ev

    def adicionar_alerta(self, texto, severidade="info", falar=True, origem="sistema", interromper=False):
        with self.trava:
            alerta = {"id": self.proximo_alerta, "hora": agora(), "ts": time.time(), "texto": texto,
                      "severidade": severidade, "falar": falar, "origem": origem, "interromper": interromper}
            self.proximo_alerta += 1
            self.alertas.append(alerta)
            self.alertas = self.alertas[-100:]
            return alerta

    def salvar_csv(self, ev):
        a = ev.get("analise") or {}
        piso = ev.get("piso") or [0, 0, 0]
        with open(self.arquivo_csv, "a", newline="", encoding="utf-8") as f:
            csv.writer(f).writerow([ev.get("hora"), ev.get("id"), ev.get("tipo"), ev.get("dist"),
                                    piso[0], piso[1], piso[2], ev.get("bateria"), ev.get("confianca"),
                                    a.get("severidade"), a.get("alerta_voz"), a.get("recomendacao"), a.get("fonte")])

    def conectado(self):
        return time.time() - self.ultima_atualizacao < 3

    def resumo(self):
        with self.trava:
            cont = {}
            for ev in self.ocorrencias:
                cont[ev["tipo"]] = cont.get(ev["tipo"], 0) + 1
            return {
                "local": self.local,
                "inicio": self.inicio.strftime("%d/%m/%Y %H:%M"),
                "duracao_min": round((datetime.now() - self.inicio).total_seconds() / 60, 1),
                "contagem": cont,
                "ocorrencias": [
                    {"hora": e["hora"], "tipo": e["tipo"], "dist_cm": e.get("dist"),
                     "piso": e.get("piso"), "bateria_v": e.get("bateria"),
                     "severidade": (e.get("analise") or {}).get("severidade")}
                    for e in self.ocorrencias if e["tipo"] != "patrulha"][-60:],
                "status_atual": self.status,
            }


# ============================ IA (LLM Gemini + regras offline) ============================

SISTEMA = (
    "Você é a IA embarcada no 'Fiscal de Calçadas', um robô CitizenTech que percorre calçadas, "
    "corredores e passagens públicas para mapear barreiras de acessibilidade (para pessoas com "
    "deficiência, idosos, carrinhos de bebê) e apoiar a manutenção urbana pela prefeitura. "
    "Sensores: ultrassônico HC-SR04 com modelo TinyML (classes livre/aproximando/obstaculo), "
    "sensor de piso infravermelho de 3 canais (valor analógico 0-1023; acima do limiar = sem piso: "
    "buraco, degrau, meio-fio) e medição da bateria. Responda sempre em português do Brasil, "
    "de forma objetiva, sem inventar dados que não foram informados."
)


class IA:
    def __init__(self, estado):
        self.estado = estado
        self.chave = os.environ.get("GEMINI_API_KEY", "").strip()
        modelo_env = os.environ.get("GEMINI_MODEL", "").strip()
        # em ordem de preferência; se um estiver descontinuado (404), sobrecarregado (503),
        # sem cota (429) ou lento, o próximo é tentado
        self.modelos = [m for m in [modelo_env, "gemini-3.5-flash", "gemini-3.5-flash-lite",
                                    "gemini-flash-lite-latest", "gemini-flash-latest"] if m]
        # alertas em tempo real usam modelos "lite" (~1 s); relatório e perguntas usam o modelo maior
        modelo_rapido_env = os.environ.get("GEMINI_MODEL_RAPIDO", "").strip()
        self.modelos_rapidos = [m for m in [modelo_rapido_env, "gemini-3.5-flash-lite", "gemini-flash-lite-latest",
                                            "gemini-3.1-flash-lite", "gemini-3.5-flash"] if m]
        self.modelo_ok = None
        self.modelo_rapido_ok = None
        self.ultimo_erro = ""
        self.chamadas = 0
        self.ultimo_modelo = None
        self.ultima_latencia = 0.0
        self.fila = queue.Queue()
        self.ultimo_por_tipo = {}
        threading.Thread(target=self._trabalhar, daemon=True).start()

    @property
    def online(self):
        return bool(self.chave)

    def info(self):
        return {"llm": "Gemini" if self.chave else None, "modelo": self.modelo_rapido_ok or self.modelo_ok,
                "online": self.online, "ultimo_erro": self.ultimo_erro, "chamadas": self.chamadas}

    # ---------- chamada ao Gemini (REST, sem dependências) ----------
    def _gemini(self, prompt, json_saida=True, temperatura=0.4, rapido=False):
        if not self.chave:
            raise RuntimeError("sem GEMINI_API_KEY")
        corpo = {
            "systemInstruction": {"parts": [{"text": SISTEMA}]},
            "contents": [{"role": "user", "parts": [{"text": prompt}]}],
            "generationConfig": {"temperature": temperatura},
        }
        if json_saida:
            corpo["generationConfig"]["responseMimeType"] = "application/json"
        # o modelo que funcionou por último vem primeiro, os outros ficam de reserva
        lista, ok = (self.modelos_rapidos, self.modelo_rapido_ok) if rapido else (self.modelos, self.modelo_ok)
        modelos = ([ok] if ok else []) + [m for m in lista if m != ok]
        erro = None
        for modelo in modelos:
            url = "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent" % modelo
            req = urllib.request.Request(url, data=json.dumps(corpo).encode("utf-8"), method="POST",
                                         headers={"Content-Type": "application/json", "x-goog-api-key": self.chave})
            try:
                with urllib.request.urlopen(req, timeout=12) as resp:
                    dados = json.loads(resp.read().decode("utf-8"))
                self.chamadas += 1
                if rapido:
                    self.modelo_rapido_ok = modelo
                else:
                    self.modelo_ok = modelo
                self.ultimo_modelo = modelo
                self.ultimo_erro = ""
                partes = dados["candidates"][0]["content"]["parts"]
                return "".join(p.get("text", "") for p in partes)
            except urllib.error.HTTPError as e:
                detalhe = e.read().decode("utf-8", "ignore")[:200]
                erro = "HTTP %d em %s: %s" % (e.code, modelo, detalhe)
                if e.code in (404, 429, 500, 503):
                    continue          # descontinuado, sem cota ou sobrecarregado: tenta o próximo
                break                 # 400/401/403: chave ou pedido inválido, não adianta insistir
            except urllib.error.URLError as e:
                erro = "sem conexão: %s" % e.reason
                break                 # sem internet: vai direto para as regras offline
            except Exception as e:     # timeout: tenta um modelo mais leve
                erro = "%s: %s" % (type(e).__name__, e)
                continue
        self.ultimo_erro = erro or "erro desconhecido"
        raise RuntimeError(self.ultimo_erro)

    # ---------- análise de cada ocorrência ----------
    def analisar_offline(self, ev):
        tipo = ev["tipo"]
        dist = ev.get("dist")
        regras = {
            "desnivel": ("alta", "Atenção: desnível detectado no piso. Possível buraco, degrau ou meio-fio sem rampa. Robô parou e recuou.",
                         "Registrar o ponto para a prefeitura e sinalizar o local; avaliar rampa de acessibilidade ou reparo do piso."),
            "obstaculo": ("media", "Obstáculo bloqueando a passagem a %s centímetros. Desviando." % dist,
                          "Verificar se o objeto bloqueia a faixa livre da calçada (mínimo de 1,20 m) e solicitar remoção."),
            "trecho_critico": ("alta", "Trecho crítico: várias barreiras seguidas. Reduzindo a velocidade.",
                               "Priorizar a vistoria deste trecho; a concentração de barreiras indica rota inacessível."),
            "bateria_baixa": ("media", "Bateria baixa. Velocidade reduzida para economizar energia.",
                              "Encerrar a patrulha e recarregar as baterias."),
            "falha_r3": ("alta", "Falha de comunicação com a placa de controle. Motores parados por segurança.",
                         "Verificar o cabo entre o shield e o módulo da câmera e a alimentação da placa R3."),
            "r3_reconectada": ("baixa", "Comunicação com a placa de controle restabelecida.", "Nenhuma ação necessária."),
            "sensor_piso_indisponivel": ("media", "Sensor de piso indisponível. Operando só com o ultrassônico.",
                                         "Gravar o firmware da GS na placa R3 para habilitar a detecção de desníveis."),
        }
        sev, alerta, rec = regras.get(tipo, ("baixa", "Ocorrência registrada: %s." % tipo, "Acompanhar."))
        return {"severidade": sev, "categoria": tipo, "alerta_voz": alerta,
                "explicacao": DESCRICAO_EVENTO.get(tipo, ""), "recomendacao": rec, "fonte": "regras (offline)"}

    def analisar_llm(self, ev):
        resumo = self.estado.resumo()
        prompt = (
            "Nova ocorrência detectada pelo robô.\n"
            "Local da patrulha: %s\nHorário: %s\nTipo: %s (%s)\n"
            "Dados dos sensores no momento: distância à frente = %s cm; sensor de piso [esq, centro, dir] = %s "
            "(limiar de 'sem piso' = %s); bateria = %s V; confiança do modelo = %s.\n"
            "Histórico desta patrulha (contagem por tipo): %s\n\n"
            "Responda APENAS com um JSON com as chaves:\n"
            "  severidade: 'baixa' | 'media' | 'alta' (risco para pedestres com deficiência ou mobilidade reduzida)\n"
            "  categoria: uma categoria curta do problema urbano\n"
            "  alerta_voz: frase curta (máx. 25 palavras) para ser FALADA em voz alta agora, para quem está no local\n"
            "  explicacao: 1 frase explicando como os sensores levaram a essa conclusão\n"
            "  recomendacao: 1 ação concreta para a prefeitura/gestão do espaço\n"
        ) % (self.estado.local, ev.get("hora"), ev["tipo"], DESCRICAO_EVENTO.get(ev["tipo"], ""),
             ev.get("dist"), ev.get("piso"), (resumo["status_atual"].get("piso") or {}).get("limiar", 550),
             ev.get("bateria"), ev.get("confianca"), json.dumps(resumo["contagem"], ensure_ascii=False))
        inicio = time.time()
        texto = self._gemini(prompt, rapido=True, temperatura=0.3)
        self.ultima_latencia = round(time.time() - inicio, 1)
        texto = texto.strip().strip("`")
        if texto.startswith("json"):
            texto = texto[4:]
        dados = json.loads(texto)
        dados["severidade"] = str(dados.get("severidade", "media")).lower().replace("é", "e")
        dados["fonte"] = "LLM %s (%.1f s)" % (self.ultimo_modelo, self.ultima_latencia)
        return dados

    def enfileirar(self, ev):
        self.fila.put(ev)

    def _trabalhar(self):
        while True:
            ev = self.fila.get()
            tipo = ev["tipo"]
            # mesma ocorrência repetida em menos de 8 s: reaproveita a análise (poupa a cota do LLM)
            anterior = self.ultimo_por_tipo.get(tipo)
            if anterior and time.time() - anterior[0] < 8:
                analise = dict(anterior[1])
                analise["fonte"] = analise.get("fonte", "") + " (repetição)"
                nomes = {"desnivel": "desnível", "obstaculo": "obstáculo", "trecho_critico": "trecho crítico",
                         "bateria_baixa": "bateria baixa", "falha_r3": "falha na placa de controle"}
                analise["alerta_voz"] = "Novo %s detectado." % nomes.get(tipo, tipo.replace("_", " "))
            else:
                try:
                    analise = self.analisar_llm(ev) if self.online else self.analisar_offline(ev)
                except Exception as e:
                    print("[IA] LLM indisponível (%s) - usando regras offline" % e)
                    analise = self.analisar_offline(ev)
                self.ultimo_por_tipo[tipo] = (time.time(), analise)
            ev["analise"] = analise
            self.estado.salvar_csv(ev)
            # a voz acompanha o robô: alerta atrasado (> 10 s) ou com o carrinho já parado
            # fica só na tela e no log, não é falado
            atrasado = time.time() - ev.get("recebido", time.time()) > 10
            parado = self.estado.status.get("patrulhando") is False
            falar = not atrasado and not parado
            self.estado.adicionar_alerta(analise.get("alerta_voz", ""), analise.get("severidade", "media"),
                                         falar, analise.get("fonte", "") + ("" if falar else " (não falado: atrasado ou robô parado)"))
            print("[IA] %s -> %s | %s" % (tipo, analise.get("severidade"), analise.get("alerta_voz")))
            time.sleep(0.5)   # espaço mínimo entre chamadas (limite do plano gratuito)

    # ---------- relatório e perguntas ----------
    def relatorio(self):
        resumo = self.estado.resumo()
        nome = os.path.join(PASTA_SAIDA, "relatorio_%s.md" % datetime.now().strftime("%Y%m%d_%H%M%S"))
        try:
            if not self.online:
                raise RuntimeError("offline")
            prompt = (
                "Gere um relatório técnico em Markdown para a prefeitura/gestão do espaço sobre esta patrulha de "
                "acessibilidade. Seções: Resumo, Ocorrências por gravidade (tabela), Pontos críticos, "
                "Recomendações priorizadas, Estado do robô. Use somente os dados abaixo.\n\n%s"
            ) % json.dumps(resumo, ensure_ascii=False, indent=1)
            texto = self._gemini(prompt, json_saida=False, temperatura=0.3)
            fonte = "LLM %s" % self.ultimo_modelo
        except Exception:
            texto = self._relatorio_offline(resumo)
            fonte = "regras (offline)"
        texto += "\n\n---\n_Gerado pelo painel do Fiscal de Calçadas em %s (%s)._\n" % (
            datetime.now().strftime("%d/%m/%Y %H:%M"), fonte)
        with open(nome, "w", encoding="utf-8") as f:
            f.write(texto)
        return {"arquivo": nome, "texto": texto, "fonte": fonte}

    def _relatorio_offline(self, r):
        linhas = ["# Relatório de patrulha - Fiscal de Calçadas", "",
                  "**Local:** %s  " % r["local"], "**Início:** %s  " % r["inicio"],
                  "**Duração:** %s min" % r["duracao_min"], "", "## Ocorrências", "",
                  "| Tipo | Quantidade |", "|---|---|"]
        for tipo, n in sorted(r["contagem"].items()):
            linhas.append("| %s | %d |" % (tipo, n))
        linhas += ["", "## Detalhes", "", "| Hora | Tipo | Distância (cm) | Piso [E,C,D] | Gravidade |", "|---|---|---|---|---|"]
        for o in r["ocorrencias"]:
            linhas.append("| %s | %s | %s | %s | %s |" % (o["hora"], o["tipo"], o["dist_cm"], o["piso"], o["severidade"]))
        linhas += ["", "## Recomendações", "",
                   "- Desníveis: sinalizar e avaliar rampa/reparo do piso.",
                   "- Obstáculos: garantir a faixa livre de 1,20 m da calçada (NBR 9050).",
                   "- Trechos críticos: priorizar vistoria presencial."]
        return "\n".join(linhas)

    def perguntar(self, pergunta):
        resumo = self.estado.resumo()
        if not self.online:
            c = resumo["contagem"]
            return ("(IA offline) Nesta patrulha: %d desnível(is), %d obstáculo(s), %d trecho(s) crítico(s). "
                    "Distância atual à frente: %s cm." % (c.get("desnivel", 0), c.get("obstaculo", 0),
                                                         c.get("trecho_critico", 0), resumo["status_atual"].get("dist_cm")))
        prompt = ("Dados da patrulha até agora:\n%s\n\nPergunta do cidadão/operador: %s\n"
                  "Responda em até 3 frases curtas, próprias para serem faladas.") % (
            json.dumps(resumo, ensure_ascii=False), pergunta)
        try:
            return self._gemini(prompt, json_saida=False, temperatura=0.5).strip()
        except Exception as e:
            return "Não consegui falar com o LLM agora (%s)." % e


# ============================ fontes de dados ============================

class Fonte:
    nome = ""

    def __init__(self, estado, ia):
        self.estado = estado
        self.ia = ia
        self.ultimo_id = 0
        self.estava_conectado = None

    def novo_evento(self, ev):
        if ev.get("id", 0) <= self.ultimo_id:
            return
        self.ultimo_id = ev["id"]
        ev = self.estado.adicionar_ocorrencia(ev)
        if ev["tipo"] in EVENTOS_ANALISADOS:
            self.ia.enfileirar(ev)
        elif ev["tipo"] == "patrulha":
            self.estado.adicionar_alerta("Patrulha iniciada." if ev.get("confianca") else "Patrulha encerrada.", "info",
                                         True, "sistema", interromper=True)

    def vigiar_conexao(self):
        while True:
            ok = self.estado.conectado()
            if ok != self.estava_conectado:
                if ok:
                    self.estado.adicionar_alerta("Conectado ao robô (%s)." % self.nome, "info")
                elif self.estava_conectado is not None:
                    self.estado.adicionar_alerta("Sem dados do robô. Ele continua autônomo; verifique Wi-Fi ou cabo.", "media")
                self.estava_conectado = ok
            time.sleep(1)

    def iniciar(self):
        threading.Thread(target=self.rodar, daemon=True).start()
        threading.Thread(target=self.vigiar_conexao, daemon=True).start()


class FonteHTTP(Fonte):
    def __init__(self, estado, ia, ip):
        super().__init__(estado, ia)
        self.base = "http://%s" % ip
        self.nome = "Wi-Fi %s" % ip

    def _get(self, caminho):
        with urllib.request.urlopen(self.base + caminho, timeout=2) as r:
            return json.loads(r.read().decode("utf-8"))

    def rodar(self):
        while True:
            try:
                status = self._get("/status")
                self.estado.atualizar_status(status)
                ultimo = status.get("ultimo_evento", 0)
                if ultimo < self.ultimo_id:
                    self.ultimo_id = 0       # ESP32 reiniciou: recomeça a contagem
                if ultimo > self.ultimo_id:
                    for ev in self._get("/eventos?desde=%d" % self.ultimo_id):
                        self.novo_evento(ev)
            except Exception:
                pass
            time.sleep(0.5)

    def comando(self, acao, valor=0):
        q = urllib.parse.urlencode({"acao": acao, "valor": valor})
        return self._get("/cmd?" + q)


class FonteSerial(Fonte):
    def __init__(self, estado, ia, porta):
        super().__init__(estado, ia)
        import serial  # pyserial
        self.serial = serial.Serial(porta, 115200, timeout=1)
        self.nome = "USB %s" % porta

    def rodar(self):
        while True:
            try:
                linha = self.serial.readline().decode("utf-8", "ignore").strip()
            except Exception:
                time.sleep(1)
                continue
            if linha.startswith("@T "):
                try:
                    self.estado.atualizar_status(json.loads(linha[3:]))
                except ValueError:
                    pass
            elif linha.startswith("@E "):
                try:
                    ev = json.loads(linha[3:])
                    if ev["id"] < self.ultimo_id:
                        self.ultimo_id = 0
                    self.novo_evento(ev)
                except (ValueError, KeyError):
                    pass
            elif linha.startswith("#"):
                print(linha)

    def comando(self, acao, valor=0):
        mapa = {"iniciar": '{"G":1}', "parar": '{"G":0}', "pausar": '{"P":1}', "retomar": '{"P":0}',
                "limiar": '{"N":112,"D1":%d}' % int(valor)}
        if acao in mapa:
            self.serial.write(mapa[acao].encode())
        return {"ok": acao in mapa, "acao": acao}


class FonteSimulada(Fonte):
    """Gera dados parecidos com os do robô para testar painel, IA e voz sem o hardware."""
    nome = "simulação"

    def __init__(self, estado, ia):
        super().__init__(estado, ia)
        self.id = 0
        self.ativo = True
        self.cont = {"obstaculo": 0, "desnivel": 0, "trecho_critico": 0, "bateria_baixa": 0, "falha_r3": 0}

    def _evento(self, tipo, dist, piso, bat, conf):
        self.id += 1
        if tipo in self.cont:
            self.cont[tipo] += 1
        self.novo_evento({"id": self.id, "t": int(time.time() * 1000), "tipo": tipo, "dist": dist,
                          "piso": piso, "bateria": bat, "confianca": conf})

    def rodar(self):
        dist, bat, t0 = 150, 8.1, time.time()
        roteiro = [(6, "obstaculo"), (14, "desnivel"), (17, "obstaculo"), (40, "bateria_baixa"), (55, "desnivel")]
        feitos = set()
        while True:
            t = time.time() - t0
            dist = max(8, min(150, dist + random.choice([-12, -6, 0, 6, 12])))
            bat = max(6.8, bat - 0.004)
            piso = [random.randint(0, 15) for _ in range(3)]
            for i, (instante, tipo) in enumerate(roteiro):
                if self.ativo and t >= instante and i not in feitos:
                    feitos.add(i)
                    if tipo == "desnivel":
                        piso = [random.randint(730, 750), random.randint(730, 750), random.randint(0, 750)]
                    self._evento(tipo, dist if tipo != "obstaculo" else random.randint(8, 18), piso, round(bat, 1),
                                 round(random.uniform(0.7, 0.98), 2) if tipo == "obstaculo" else 1.0)
                    if tipo == "obstaculo" and self.cont["obstaculo"] + self.cont["desnivel"] >= 2 and not self.cont["trecho_critico"]:
                        self._evento("trecho_critico", dist, piso, round(bat, 1), 0)
            classe = "livre" if dist > 60 else ("aproximando" if dist > 20 else "obstaculo")
            self.estado.atualizar_status({
                "uptime_ms": int(t * 1000), "r3_ok": True, "dist_cm": dist,
                "piso": {"disponivel": True, "l": piso[0], "m": piso[1], "r": piso[2], "limiar": 550,
                         "desnivel": max(piso) > 550},
                "patrulhando": self.ativo, "bateria_v": round(bat, 1), "bateria_ligada": True,
                "bateria_baixa": bat < 7.0,
                "modelo": {"classe": classe, "confianca": 0.9, "tempo_ms": 0,
                           "probs": {"aproximando": 0.9 if classe == "aproximando" else 0.05,
                                     "livre": 0.9 if classe == "livre" else 0.05,
                                     "obstaculo": 0.9 if classe == "obstaculo" else 0.05}},
                "decisao": {"classe": classe, "dir": 1, "vel": 150 if classe == "livre" else 90, "manobra": "nenhuma",
                            "cautela": self.cont["trecho_critico"] > 0, "pausado": not self.ativo},
                "contagem": self.cont, "ultimo_evento": self.id, "wifi_rssi": -55})
            time.sleep(0.5)

    def comando(self, acao, valor=0):
        if acao in ("pausar", "parar"):
            self.ativo = False
        elif acao in ("retomar", "iniciar"):
            self.ativo = True
        if acao in ("iniciar", "parar"):
            self._evento("patrulha", 150, [0, 0, 0], 8.0, 1 if acao == "iniciar" else 0)
        return {"ok": True, "acao": acao}


# ============================ servidor web do painel ============================

def criar_handler(estado, ia, fonte):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def _json(self, dados, codigo=200):
            corpo = json.dumps(dados, ensure_ascii=False).encode("utf-8")
            self.send_response(codigo)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(corpo)))
            self.end_headers()
            self.wfile.write(corpo)

        def _corpo(self):
            n = int(self.headers.get("Content-Length", 0) or 0)
            if not n:
                return {}
            try:
                return json.loads(self.rfile.read(n).decode("utf-8", "replace") or "{}")
            except ValueError:
                return {}

        def do_GET(self):
            url = urllib.parse.urlparse(self.path)
            if url.path in ("/", "/index.html"):
                with open(os.path.join(PASTA, "index.html"), "rb") as f:
                    corpo = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(corpo)))
                self.end_headers()
                self.wfile.write(corpo)
            elif url.path == "/api/estado":
                with estado.trava:
                    dados = {
                        "status": estado.status, "conectado": estado.conectado(), "fonte": fonte.nome,
                        "local": estado.local, "ia": ia.info(),
                        "ocorrencias": [e for e in estado.ocorrencias if e["tipo"] != "patrulha"][-30:][::-1],
                        "contagem": estado.contagem(),
                        "alertas": estado.alertas[-30:],
                    }
                self._json(dados)
            elif url.path == "/api/cmd":
                q = urllib.parse.parse_qs(url.query)
                acao = q.get("acao", [""])[0]
                valor = int(q.get("valor", ["0"])[0] or 0)
                try:
                    self._json(fonte.comando(acao, valor))
                except Exception as e:
                    self._json({"ok": False, "erro": str(e)}, 502)
            else:
                self.send_error(404)

        def do_POST(self):
            if self.path == "/api/reiniciar":
                estado.reiniciar()
                ia.ultimo_por_tipo.clear()
                print("[painel] reiniciado - novo log: %s" % estado.arquivo_csv)
                self._json({"ok": True, "log": estado.arquivo_csv})
            elif self.path == "/api/relatorio":
                self._json(ia.relatorio())
            elif self.path == "/api/perguntar":
                pergunta = str(self._corpo().get("pergunta", ""))[:500]
                resposta = ia.perguntar(pergunta)
                estado.adicionar_alerta(resposta, "info", True, "resposta")
                self._json({"resposta": resposta})
            else:
                self.send_error(404)

    return Handler


def main():
    carregar_env()
    ap = argparse.ArgumentParser(description="Painel com IA do Fiscal de Calçadas (GS CitizenTech)")
    grupo = ap.add_mutually_exclusive_group(required=True)
    grupo.add_argument("--ip", help="IP da ESP32 no Wi-Fi (ex.: 192.168.0.50 ou fiscal.local)")
    grupo.add_argument("--porta", help="porta USB da ESP32 (ex.: COM5)")
    grupo.add_argument("--simular", action="store_true", help="dados simulados, sem o robô")
    ap.add_argument("--local", default=os.environ.get("LOCAL_PATRULHA", "Calçada de teste - FIAP"),
                    help="nome do local patrulhado (vai para os alertas e o relatório)")
    ap.add_argument("--http", type=int, default=8000, help="porta do painel web (padrão 8000)")
    args = ap.parse_args()

    estado = Estado(args.local)
    ia = IA(estado)
    if args.ip:
        fonte = FonteHTTP(estado, ia, args.ip)
    elif args.porta:
        fonte = FonteSerial(estado, ia, args.porta)
    else:
        fonte = FonteSimulada(estado, ia)
    estado.fonte = fonte.nome
    fonte.iniciar()

    print("=" * 60)
    print(" Fiscal de Calçadas - painel com IA")
    print(" Fonte de dados : %s" % fonte.nome)
    print(" IA             : %s" % ("Gemini (LLM)" if ia.online else "regras offline (sem GEMINI_API_KEY no .env)"))
    print(" Log            : %s" % estado.arquivo_csv)
    print(" Abra no navegador: http://localhost:%d" % args.http)
    print("=" * 60)
    servidor = ThreadingHTTPServer(("0.0.0.0", args.http), criar_handler(estado, ia, fonte))
    try:
        servidor.serve_forever()
    except KeyboardInterrupt:
        print("\nencerrado")


if __name__ == "__main__":
    main()

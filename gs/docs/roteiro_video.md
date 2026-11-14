# Roteiro do vídeo (máx. 6 min)

Sugestão: celular na horizontal; quando possível, mostrar o carrinho e a tela do painel lado a lado (ou cortar entre os dois). Volume do PC alto, para a voz da IA aparecer no vídeo.

| Tempo | Cena | O que mostrar / falar |
|---|---|---|
| 0:00–0:40 | **Problema** | Fotos ou imagens de calçadas com buraco, degrau, meio-fio sem rampa e entulho. "Para quem usa cadeira de rodas, é idoso ou empurra um carrinho, essas barreiras impedem a circulação. Hoje o mapeamento é manual e lento." |
| 0:40–1:20 | **Solução** | Apresentar o "Fiscal de Calçadas": o carrinho do CP2 evoluído. Mostrar o diagrama de arquitetura do README da GS. Citar: ultrassônico + TinyML (CP2), **sensor de piso (novo)**, LLM Gemini, voz e relatório. |
| 1:20–2:00 | **Sensor adicional** | Close no módulo seguidor de linha embaixo do carrinho. Explicar a função nova (piso = valor baixo, sem piso = valor alto). No painel: valores no chão vs. carrinho levantado; "DESNÍVEL À FRENTE" acende; o robô levantado recusa andar. |
| 2:00–3:00 | **Demo 1: desnível** | Mesa baixa = degrau/meio-fio. Iniciar a patrulha; o carrinho vai até a borda, **para, LED roxo, ré e giro**. Cortar para o painel: ocorrência com gravidade **alta**, explicação e recomendação; **a voz fala o alerta**. |
| 3:00–3:50 | **Demo 2: obstáculo + autonomia** | Caixa no caminho: desacelera (laranja), desvia (vermelho). Em seguida, outra barreira: o painel mostra **"modo cauteloso"** (trecho crítico) e o carrinho anda mais devagar. |
| 3:50–4:30 | **IA conversando** | Digitar "Esta calçada é acessível para cadeirantes?" e mostrar a resposta falada. Clicar em **Gerar relatório para a prefeitura** e mostrar o relatório. |
| 4:30–5:20 | **Falhas** | Soltar o cabo R3↔ESP32: motores param e o painel registra `falha_r3`. Desligar o Wi-Fi: o robô continua desviando sozinho e o painel avisa. (Opcional) desligar a internet: a IA cai para as regras offline. |
| 5:20–6:00 | **Fechamento** | Impacto: mapa de barreiras barato, reaproveitando o kit, útil para prefeituras, escolas e o campo. Próximos passos: GPS do celular, foto da câmera para o LLM, TinyML para o piso. Mostrar o link do repositório. |

## Checklist antes de gravar

- [ ] R3 e ESP32 gravadas com o firmware da GS (`gs/r3/carrinho_r3`, `gs/esp32/carrinho_gs`)
- [ ] `config_wifi.h` com o hotspot; o PC na mesma rede; IP anotado (Monitor Serial)
- [ ] `gs/painel/.env` com a chave do Gemini; testado em `--simular` antes
- [ ] Sensor de piso calibrado (testes P1–P3)
- [ ] Baterias carregadas
- [ ] Mesa baixa e alguém pronto para segurar o carrinho no teste da borda
- [ ] Depois de gravar: colocar o link do vídeo no topo do `README.md` e do `gs/README.md`

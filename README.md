# SPACE VOCAL — Vocal Processor · Simple Edition
*YOUR VOICE. ANOTHER DIMENSION.*

Plugin VST3 nativo (C++/JUCE 8) para Windows e macOS: INPUT → COMP → MAGIC → (REVERB ‖ DELAY ‖ CORAL) → OUTPUT. DSP local em tempo real, sem internet, sem contas, sem telemetria.

## Estado real (leia primeiro)

| Item | Estado |
|---|---|
| DSP (compressor, Magic, reverb FDN, delay, Coral) | Escrito. **Testado** com `Tests/DspStandalone.cpp` (g++, Linux): estabilidade, NaN, decay, tempo do delay, feedback, GR, 5 taxas de amostragem. Todos passaram. |
| Parâmetros, processor, presets, estado | Escrito. **Verificação sintática passou** (g++ `-fsyntax-only`). **Não foi executado nem ligado**: o ambiente de desenvolvimento não tinha headers X11/ALSA nem acesso ao apt, logo o JUCE não compilou por completo. |
| Editor gráfico | Escrito, **sintaxe verificada, nunca renderizado**. O aspeto real, o layout e a legibilidade ainda não foram vistos. |
| Testes JUCE (`Tests/Tests.cpp`, ~15 grupos: bypass, ganhos, BPM, presets, estado, mono/estéreo…) | Escritos, **ainda não executados**. |
| Build VST3 Windows / macOS | **Não compilado.** Nenhum binário é entregue. |
| Testes em DAWs (FL Studio, Ableton, Cubase, REAPER) | **Não realizados.** Compatibilidade não garantida. |
| Assinatura / notarização macOS | Não realizada. |
| Afinação auditiva dos presets | **Não realizada.** Os valores são pontos de partida razoáveis, não resultados validados. |

## Compilar

Requer CMake ≥ 3.22 e internet **apenas** para o primeiro download do JUCE 8.0.4 (FetchContent).

**Windows 10/11** — Visual Studio 2022 (workload "Desktop development with C++"):
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target SpaceVocal_VST3
cmake --build build --config Release --target SpaceVocalTests && build\SpaceVocalTests_artefacts\Release\SpaceVocalTests.exe
```
**macOS** — Xcode + Command Line Tools (universal arm64 + x86_64 por defeito):
```
cmake -S . -B build -G Xcode
cmake --build build --config Release --target SpaceVocal_VST3
```
**Linux (só testes/desenvolvimento)** — instalar `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev`.

Teste DSP sem dependências: `g++ -std=c++17 -O2 -ISource/DSP Tests/DspStandalone.cpp -o dsp_test && ./dsp_test`

## Instalar
- Windows: copiar `SPACE VOCAL.vst3` para `C:\Program Files\Common Files\VST3\` e reanalisar plugins na DAW. Para atualizar, feche a DAW e substitua a pasta.
- macOS: `~/Library/Audio/Plug-Ins/VST3/` (ou `/Library/Audio/Plug-Ins/VST3/`). Para distribuir a terceiros é necessário assinar com Developer ID e notarizar; não foi feito. Sem isso, o Gatekeeper pode bloquear (não contornar).
- Presets de utilizador: `…/SpaceVocal/Presets/*.svpreset` (AppData no Windows, `~/Library/Application Support` no macOS).

## Uso
- **SIMPLE** (por defeito): INPUT/OUTPUT GAIN, AMOUNT de COMP/MAGIC/REVERB/DELAY, tipo de reverb, divisão do delay (1/16, 1/8, 1/4), bypass por módulo e geral, presets, SAVE/LOAD/DEL, CORAL.
- **ADVANCED**: revela threshold/ratio/attack/release/knee/makeup; drive/presença/tom/harmónicos/mix; decay/pre-delay/damping/tom/largura/mix; tempo manual, feedback, filtros, mix, host sync, ping-pong. Mudar de modo não altera o som (os mesmos parâmetros estão sempre ativos).
- Duplo clique num knob repõe o valor por defeito; clique no texto do valor para escrever um número.

## Decisões de DSP
- **COMP**: feed-forward, detetor de pico ligado em estéreo, joelho suave, suavização da redução de ganho em dB. AMOUNT escala o ratio (`1 + (ratio−1)·min(1,5; 2·amount)`); 0% = sem compressão. Makeup automático moderado (`0,5·max(0,−6−thr)·(1−1/ratio)`) somado ao makeup manual, para o volume não cair com a compressão.
- **MAGIC**: DSP convencional, **não é IA**. Peak EQ 3,5 kHz + shelf 6 kHz + saturação tanh com viés (ganho unitário em sinais pequenos) + excitador de harmónicos (>2,5 kHz). Saída = `lerp(original, processado, amount·mix)`; 0% é passagem exata (testado).
- **REVERB**: FDN de 8 linhas, matriz de Hadamard, amortecimento por linha, difusão de entrada, modulação. ROOM/HALL/PLATE mudam comprimento das linhas, multiplicador de decay, damping, difusão e modulação. Sem alocações na thread de áudio.
- **DELAY**: tempo = `60000/BPM × {0,25; 0,5; 1}` ms; sem BPM do anfitrião usa **120 BPM** (fallback). Tempo suavizado (80 ms), feedback limitado a 92% com `tanh`, filtros no feedback, ping-pong.
- **CORAL**: 4 cópias da *mesma* gravação com atrasos 14–35 ms, modulação lenta (~2–5 cents) e panorama estéreo. **Não cria cantores independentes nem harmonias reais.** Compatibilidade mono não foi medida.
- **Saída**: indicador de clipping (≥ 0 dBFS), **sem limiter**. Clique no medidor para limpar. Medidor de entrada é pós-ganho.
- Efeitos desligados: a entrada do efeito fecha com rampa e a cauda continua; bypass geral é um crossfade de 30 ms e o processamento continua em segundo plano (mais CPU, mas sem estalos ao reativar).

## Limitações conhecidas
- Nada disto foi ouvido nem visto numa DAW; rever valores de preset por audição com vozes reais.
- Sem comparação A/B além do bypass. Sem opções extra de medição (ex. RMS/LUFS).
- CORAL fica na faixa inferior (não é uma das 6 secções); Magic/Reverb/Delay/Comp mostram o mesmo texto de ajuda no modo SIMPLE.
- Tooltips, foco por teclado e escalas HiDPI dependem do JUCE; não verificados. Ajuste fino com tecla modificadora não verificado.
- Mudar o tipo de reverb faz as linhas deslizarem em ~0,25 s (sem clique, com pequena variação de altura).
- Fontes: usa a fonte por defeito do sistema.

## Dependências e licenças
- **JUCE 8.0.4** (descarregado por CMake): AGPLv3 ou licença comercial. Distribuir o plugin comercialmente exige licença JUCE comercial (ou cumprir a AGPLv3).
- **VST3 SDK** (incluído no JUCE): licença Steinberg VST3 / GPLv3. Sem outras dependências. Nome e logótipo (vetorial, desenhado em código) são originais.
- "VST" é marca registada da Steinberg Media Technologies GmbH.

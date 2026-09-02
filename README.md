# BG Remover Offline

Removedor de fundo de imagens **100% offline** — roda inteiramente na sua máquina, sem enviar nenhuma imagem para servidores externos. Diferente de serviços como remove.bg ou Photoroom, todo o processamento acontece localmente via CPU, usando um modelo de segmentação leve em ONNX Runtime.

## Por que offline?

Ferramentas de remoção de fundo consolidadas (remove.bg, Photoroom, Canva) processam as imagens na nuvem — ou seja, qualquer foto que você sobe passa pelo servidor de terceiros. Para uso em lote (catálogos de produtos, processamento de dezenas/centenas de imagens) ou para quem trabalha com conteúdo sensível, isso é uma limitação real. Este projeto nasceu como resposta a essa lacuna: um app desktop nativo, sem contas, sem limites de créditos, sem upload — só o modelo rodando local em CPU.

## Funcionalidades

- **Remoção de fundo** via modelo U²-Net / U²-Netp (ONNX)
- **Troca de modelo** em tempo real: U²-Netp (rápido, ~5 MB) ou U²-Net (qualidade, ~176 MB), ou caminho customizado
- **Escolha de fundo de saída**: transparente, branco, preto, ou cor personalizada
- **Refinamento de bordas** (blur suave no canal alpha) e **limpeza de ruído** (threshold no alpha) — dois filtros pós-processamento independentes e combináveis
- **Pincel de ajuste manual**: corrige a máscara pintando diretamente sobre o resultado, em dois modos — "Restaurar" (traz de volta pixels que o modelo removeu por engano) e "Apagar" (remove resíduos que o modelo deixou passar) — com 1 nível de desfazer
- **Processamento em lote**: seleciona uma pasta inteira de imagens e processa todas de uma vez, com barra de progresso e log de erros por arquivo
- **CLI standalone** (`bg_remover_cli`): processa uma pasta inteira via linha de comando, sem abrir a interface gráfica — útil para scripts e automações
- **Empacotamento `.deb`**: instalável como aplicativo Linux nativo, com ícone e entrada no menu de aplicativos

## Stack técnico

| Componente | Tecnologia |
|---|---|
| Interface gráfica | Qt6 (Widgets) |
| Inferência do modelo | ONNX Runtime 1.19.0 (C++ API) |
| Modelo de segmentação | U²-Net / U²-Netp |
| Build system | CMake + Ninja/Make |
| Empacotamento | CPack (gerador DEB) |
| Testes | CTest (teste de inferência automatizado) |
| Linguagem | C++17 |

Ambiente de desenvolvimento: Xubuntu, CPU-only (sem GPU dedicada) — o projeto foi desenhado desde o início para rodar bem sem aceleração de hardware.

## Como funciona (pipeline técnico)

1. **Carregamento**: a imagem é lida via `QImage` (Qt suporta JPG, PNG, BMP, WEBP, TIFF nativamente).
2. **Pré-processamento**: a imagem é redimensionada para 320×320 (tamanho de entrada do modelo), convertida para tensor CHW (canal-altura-largura) e normalizada com estatísticas ImageNet (`mean=[0.485, 0.456, 0.406]`, `std=[0.229, 0.224, 0.225]`).
3. **Inferência**: o ONNX Runtime roda o modelo sobre o tensor e retorna um **mapa de saliência** — uma imagem em escala de cinza onde branco = "isso é o objeto principal" e preto = "isso é fundo".
4. **Pós-processamento**: o mapa (320×320) é redimensionado de volta ao tamanho original da imagem e se torna o **canal alpha** (transparência).
5. **Filtros opcionais**: se ativados, aplica-se limpeza de ruído (zera pixels com alpha abaixo de um limiar) e/ou refinamento de bordas (blur 3×3 misturado com o alpha original por uma intensidade configurável).
6. **Composição**: imagem original + canal alpha calculado = PNG com fundo transparente (ou composto com uma cor sólida, se selecionado).
7. **Ajuste manual (opcional)**: o usuário pode pintar diretamente sobre o resultado para corrigir erros pontuais da máscara.

Toda essa lógica central vive encapsulada na classe `BackgroundRemover`, reutilizada de forma idêntica pela GUI, pelo processamento em lote e pelo CLI — garantindo que o resultado seja sempre o mesmo, não importa por qual caminho a imagem é processada.

## Estrutura do projeto
bg-remover/
├── backgroundremover.h/.cpp # Núcleo: pipeline de inferência (pré/pós-processamento, ONNX)
├── mainwindow.h/.cpp # GUI principal (Qt6)
├── maskeditcanvas.h/.cpp # Widget do pincel de ajuste manual
├── batchprocessor.h/.cpp # Lógica de processamento em lote (usado pelo CLI)
├── main.cpp # Entry point da GUI
├── main_cli.cpp # Entry point do CLI
├── test_inference.cpp # Teste automatizado (CTest)
├── bg-remover.desktop # Arquivo de menu de aplicativos (Linux)
├── bg-remover.svg # Ícone do aplicativo
├── CMakeLists.txt # Build + packaging (.deb via CPack)
└── onnxruntime-linux-x64-1.19.0/ # Biblioteca ONNX Runtime extraída (não versionada)

**Não versionados no Git** (veja `.gitignore`): `build/`, arquivos `*.onnx` (modelos, baixados à parte), a pasta extraída do ONNX Runtime, e imagens de teste locais.

## Como compilar

Pré-requisitos:
```bash
sudo apt install qt6-base-dev qt6-base-private-dev build-essential cmake
```

Baixe o ONNX Runtime (C++ API) e extraia na raiz do projeto:
```bash
wget https://github.com/microsoft/onnxruntime/releases/download/v1.19.0/onnxruntime-linux-x64-1.19.0.tgz
tar -xzf onnxruntime-linux-x64-1.19.0.tgz
```

Baixe os modelos (também na raiz do projeto):
```bash
wget https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2netp.onnx
# Opcional, para qualidade superior (176 MB):
# https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2net.onnx
```

Compile:
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Como usar

**GUI:**
```bash
./bg_remover_gui
```

**CLI (processamento em lote sem interface gráfica):**
```bash
./bg_remover_cli <pasta_entrada> [pasta_saida] [modelo.onnx]
```

**Instalar como pacote Debian:**
```bash
cd build
cpack -G DEB
sudo dpkg -i bg-remover_1.0.0_amd64.deb
```

## Histórico de desenvolvimento

O projeto foi construído de forma incremental, com um fluxo de trabalho onde uma IA de contexto maior propunha o código e o Claude revisava/corrigia bugs antes da compilação — o histórico de commits reflete aproximadamente esses marcos:

- **M1** — Validação do pipeline em Python (preprocess → inferência ONNX → postprocess → composite), confirmando viabilidade 100% offline em CPU (293ms por imagem)
- **M2** — Port completo para C++, encapsulado na classe `BackgroundRemover`
- **M3** — Primeira GUI Qt6 (abrir imagem → remover fundo → salvar PNG)
- **M4** — Processamento em lote com barra de progresso
- **M5** — Combo de seleção de modelo, refinamento de bordas, limpeza de ruído
- **Etapa 1** — Seleção de cor de fundo (transparente/branco/preto/personalizado)
- **Etapa 2** — Pincel de ajuste manual da máscara (restaurar/apagar)
- **M6** — Empacotamento `.deb`, testes automatizados, ícone próprio
- **M7** — CLI para processamento em lote sem interface gráfica

## Limitações conhecidas

- A qualidade da segmentação depende do modelo escolhido: U²-Netp é rápido mas pode deixar resíduos em bordas complexas (cabelo, transparências finas); U²-Net é mais preciso mas ~35x mais pesado
- O pincel de ajuste manual oferece apenas 1 nível de desfazer
- Testado e empacotado apenas para Linux (Debian/Ubuntu via `.deb`); build para Windows ainda não implementado
- CPU-only: sem suporte a aceleração por GPU (não era um requisito do projeto, dado o hardware alvo)

## Créditos

Modelos U²-Net / U²-Netp: [Qin et al., 2020](https://github.com/xuebinqin/U-2-Net). Arquivos ONNX pré-convertidos via [rembg](https://github.com/danielgatis/rembg).

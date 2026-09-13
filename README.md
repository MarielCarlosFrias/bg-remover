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
- **Pacotes portáteis**: `.deb` para Debian/Ubuntu e `.zip` para Windows x64, ambos com runtime e modelo leve incluídos

## Stack técnico

| Componente | Tecnologia |
|---|---|
| Interface gráfica | Qt6 (Widgets) |
| Inferência do modelo | ONNX Runtime 1.19.0 (C++ API) |
| Modelo de segmentação | U²-Net / U²-Netp |
| Build system | CMake + Ninja/Make |
| Empacotamento | CPack (gerador DEB) |
| Testes | CTest (pós-processamento e inferência automatizada) |
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

```text
bg-remover/
├── .github/workflows/              # CI e publicação de releases no GitHub Actions
├── backgroundremover.h/.cpp       # Inferência ONNX e máscara alpha
├── imagepostprocessor.h/.cpp      # Threshold, bordas e composição de fundo
├── processingoptions.h             # Opções compartilhadas entre GUI e CLI
├── batchprocessor.h/.cpp           # Processamento de diretórios e salvamento seguro
├── mainwindow.h/.cpp               # Interface gráfica Qt6
├── maskeditcanvas.h/.cpp           # Edição manual da máscara
├── main.cpp / main_cli.cpp          # Pontos de entrada GUI e CLI
├── test_inference.cpp               # Teste de inferência com modelo real
├── test_postprocessing.cpp          # Teste unitário dos filtros
├── bg-remover.desktop / .svg        # Integração e ícone Linux
└── CMakeLists.txt                   # Build, testes e pacote .deb
```

Arquivos grandes e locais — modelos `*.onnx`, ONNX Runtime extraído, diretórios `build*` e resultados gerados — ficam fora do Git. O repositório contém somente fonte, testes, metadados de build e automação. Os pacotes de release baixam e incluem suas dependências verificadas durante a criação; o usuário final não precisa baixar ONNX Runtime.

## Como compilar

Pré-requisitos:
```bash
sudo apt install qt6-base-dev qt6-base-private-dev build-essential cmake
```

O CMake pode baixar o ONNX Runtime 1.19.0 automaticamente, via HTTPS e com checksum SHA-256 fixado. O arquivo é extraído dentro de `build/_deps`, sem modificar a árvore de código:

Baixe os modelos (também na raiz do projeto):
```bash
wget https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2netp.onnx
# Opcional, para qualidade superior (176 MB):
# https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2net.onnx
```

```bash
cmake -S . -B build -DBG_REMOVER_FETCH_ONNX_RUNTIME=ON
cmake --build build --parallel
```

Se preferir usar uma cópia já extraída, informe o caminho explicitamente:

```bash
cmake -S . -B build \
  -DBG_REMOVER_ONNX_RUNTIME_DIR=/caminho/para/onnxruntime-linux-x64-1.19.0
```

## Como usar

**GUI:**
```bash
./build/bg_remover_gui
```

**CLI (processamento em lote sem interface gráfica):**
```bash
./build/bg_remover_cli <pasta_entrada> [pasta_saida] [modelo.onnx]
```

Se nenhum modelo for informado, GUI e CLI procuram `u2netp.onnx` ao lado do executável, no diretório de instalação e depois no diretório atual. Isso permite que o pacote Linux e o ZIP Windows funcionem sem configuração adicional.

O lote evita sobrescrever resultados: em colisões, cria sufixos como `_rembg_2.png`; quando entrada e saída são a mesma pasta, ignora saídas já geradas.

## Testes

```bash
ctest --test-dir build --output-on-failure
```

O teste de pós-processamento sempre roda. O teste de inferência é marcado como desativado quando `u2netp.onnx` não está disponível. A CI compila e testa Linux e Windows x64; no Windows ela também verifica que o ZIP contém Qt, o plugin `qwindows.dll` e as DLLs do ONNX Runtime.

**Instalar como pacote Debian:**
```bash
cmake -S . -B build-release -DINSTALLED=ON \
  -DBG_REMOVER_FETCH_ONNX_RUNTIME=ON \
  -DBG_REMOVER_BUNDLE_MODELS=ON
cmake --build build-release --parallel
cd build-release
cpack -G DEB
sudo dpkg -i bg-remover_1.0.0_amd64.deb
```

`BG_REMOVER_BUNDLE_MODELS=ON` exige `u2netp.onnx` na raiz e falha se ele estiver ausente, impedindo a criação acidental de um pacote que não funciona. O modelo maior `u2net.onnx` é incluído apenas se estiver presente.

## Windows

O release para Windows é um ZIP portátil x64: extraia-o e execute `bg_remover_gui.exe`. Ele inclui as DLLs do Qt, o plugin de plataforma, ONNX Runtime, o modelo U²-Netp e o runtime C++ necessário. Para compilar no Windows, use Visual Studio 2022, CMake e Qt 6 para MSVC:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DINSTALLED=ON -DBG_REMOVER_FETCH_ONNX_RUNTIME=ON
cmake --build build --config Release --parallel
```

Ao criar uma tag `v*`, o workflow de release gera o `.deb` e o ZIP Windows, baixa o modelo leve com checksum verificado e publica os dois no GitHub Releases.

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
- O pacote Windows x64 é validado automaticamente na CI; antes da primeira release pública, recomenda-se um teste manual em uma máquina Windows limpa
- CPU-only: sem suporte a aceleração por GPU (não era um requisito do projeto, dado o hardware alvo)

## Créditos

Modelos U²-Net / U²-Netp: [Qin et al., 2020](https://github.com/xuebinqin/U-2-Net). Arquivos ONNX pré-convertidos via [rembg](https://github.com/danielgatis/rembg).

## Licença

Ainda não há uma licença definida para o código. Escolha e adicione uma licença antes de publicar o repositório para deixar explícitas as permissões de uso e contribuição.

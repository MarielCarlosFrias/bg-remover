# BG Remover

[![CI](https://github.com/MarielCarlosFrias/bg-remover/actions/workflows/ci.yml/badge.svg)](https://github.com/MarielCarlosFrias/bg-remover/actions/workflows/ci.yml)
[![Release](https://github.com/MarielCarlosFrias/bg-remover/actions/workflows/release.yml/badge.svg)](https://github.com/MarielCarlosFrias/bg-remover/actions/workflows/release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Qt 6](https://img.shields.io/badge/Qt-6-41cd52.svg)](https://www.qt.io/)
[![Platforms](https://img.shields.io/badge/platforms-Linux%20%7C%20Windows%20%7C%20macOS-blue.svg)](#download-pronto-sem-compilar)

Remova o fundo de imagens no seu computador — sem contas, créditos ou envio de arquivos para serviços externos.

O BG Remover é um aplicativo desktop em C++ e Qt que usa U²-Net através do ONNX Runtime para criar PNGs com transparência, processar pastas inteiras e corrigir detalhes manualmente com pincel.

![Editor do BG Remover: original à esquerda, resultado com fundo removido à direita](docs/images/bg-remover-editor.png)

## Download pronto (sem compilar)

A página de [**Releases**](https://github.com/MarielCarlosFrias/bg-remover/releases) tem pacotes gerados automaticamente pelo CI. Cada release inclui um ficheiro `SHA256SUMS`; para verificar um download:

```bash
sha256sum --check SHA256SUMS
```

Pacotes disponíveis:

- **Windows x64:** ZIP portátil — extraia e execute `bg_remover_gui.exe` (Qt, ONNX Runtime e o modelo leve já incluídos).
- **macOS (Intel e Apple Silicon):** ZIP com `bg_remover_gui.app` universal2 — arraste o aplicativo para **Aplicações**. O CLI fica em `bg_remover_gui.app/Contents/MacOS/bg_remover_cli`.
- **Debian/Ubuntu:** pacote `.deb` (depende de `libqt6widgets6` e `libqt6network6`).
- **Qualquer Linux:** `.AppImage` — `chmod +x bg-remover_*_amd64.AppImage` e execute; não requer instalação.

## Por que usar?

- **Privado por definição:** a imagem e a inferência ficam na sua máquina.
- **Pronto para trabalhar:** abra uma imagem, remova o fundo, ajuste o que for preciso e exporte em PNG, JPEG ou WebP.
- **Preciso onde importa:** amplie a pré-visualização e use o pincel para restaurar cabelo, roupa ou contornos, ou apagar resíduos.
- **Feito para volume:** processe uma pasta inteira pela interface ou pela linha de comando.
- **Sem dependência de GPU:** funciona em CPU, inclusive em máquinas sem placa de vídeo dedicada.

## Funcionalidades

- Remoção de fundo com **U²-Netp**, **U²-Net**, **Silueta**, **ISNet** e outros — instale novos modelos pela aba **Modelos**, com download integrado.
- Inferência numa **thread separada**: a janela nunca bloqueia durante o processamento.
- Processamento em lote com progresso, log, **botão de cancelar** e nomes de saída que nunca sobrescrevem arquivos existentes.
- Fundo transparente, branco, preto ou uma cor personalizada.
- Refinamento de bordas e limpeza de ruído com limiar ajustável.
- Edição manual da máscara com modos **Restaurar** e **Apagar**.
- **Desfazer/Refazer multi-nível** (Ctrl+Z / Ctrl+Y), **Copiar resultado** (Ctrl+Shift+C) e atalhos de teclado (Ctrl+O, Ctrl+S, Ctrl+±, Ctrl+0).
- **Comparar antes/depois**: divisor arrastável sobre o resultado para ver o original e o recorte lado a lado.
- **Salvar em PNG, JPEG ou WebP** — PNG e WebP mantêm a transparência; JPEG é composto sobre o fundo escolhido. *(No Linux, WebP requer o pacote `qt6-image-formats-plugins`; o programa avisa se não estiver instalado.)*
- **Aviso de nova versão**: o programa consulta as releases do projeto e avisa quando há atualização. A verificação pode ser desativada com a variável de ambiente `BG_REMOVER_NO_UPDATE_CHECK=1` ou com `update/check=false` nas configurações (`~/.config/bg-remover/bg-remover.conf` no Linux).
- **Arraste e solte** imagens (abre) ou pastas (define o lote) sobre a janela.
- **Tema escuro e claro** com alternância e persistência; preferências salvas (pastas, modelo, geometria).
- Zoom de 50% a 400% **centrado no cursor** (Ctrl + roda do mouse).
- CLI para automação em scripts (`bg_remover_cli --help`), com opções `-o/--output` e `-m/--model`.
- Pacotes preparados para Debian/Ubuntu, Windows x64 e macOS universal2.

## Como funciona

1. A imagem é preparada no formato esperado pelo modelo (320 × 320, RGB normalizado).
2. O ONNX Runtime executa o modelo localmente e retorna um mapa que separa o objeto principal do fundo.
3. O mapa é redimensionado para a resolução original e aplicado como canal alpha.
4. Se necessário, a máscara recebe filtros de borda/ruído e pode ser corrigida com o pincel.
5. O resultado é exportado em PNG/WebP com transparência ou em JPEG composto sobre uma cor sólida.

O mesmo pipeline é usado pela interface, pelo lote e pelo CLI, para que os resultados sejam consistentes em todos os fluxos.

## Começar rapidamente (compilar do código)

### Linux (Debian/Ubuntu)

Instale os requisitos de desenvolvimento:

```bash
sudo apt install build-essential cmake qt6-base-dev qt6-image-formats-plugins
```

Compile. O CMake baixa o ONNX Runtime automaticamente e de forma verificada:

```bash
cmake -S . -B build -DBG_REMOVER_FETCH_ONNX_RUNTIME=ON
cmake --build build --parallel
```

Baixe o modelo leve na raiz do projeto:

```bash
wget https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2netp.onnx
```

Abra o aplicativo:

```bash
./build/bg_remover_gui
```

### Windows

Os releases incluem um ZIP portátil x64. Extraia-o e execute `bg_remover_gui.exe`; o pacote contém Qt, ONNX Runtime e o modelo leve.

Para compilar a partir do código-fonte, use Visual Studio 2022, CMake e Qt 6 para MSVC:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DINSTALLED=ON -DBG_REMOVER_FETCH_ONNX_RUNTIME=ON
cmake --build build --config Release --parallel
```

### macOS

Instale Xcode Command Line Tools, CMake e Qt 6 (por exemplo com `aqtinstall`):

```bash
python3 -m venv .venv
.venv/bin/pip install aqtinstall
.venv/bin/aqt install-qt mac desktop 6.7.3 clang_64 -O Qt
export CMAKE_PREFIX_PATH="$PWD/Qt/6.7.3/macos"
```

Compile como aplicativo universal2 (Intel + Apple Silicon):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DINSTALLED=ON \
  -DBG_REMOVER_FETCH_ONNX_RUNTIME=ON \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build --parallel
open build/bg_remover_gui.app
```

Baixe `u2netp.onnx` na raiz do projeto como no Linux; o aplicativo procura o modelo em `Contents/Resources` quando está dentro do bundle.

## Utilização

### Interface gráfica

1. Clique em **Abrir imagem** (ou arraste uma imagem para a janela).
2. Escolha o modelo e clique em **Remover fundo**.
3. Use `+`, `−`, **Ajustar** ou Ctrl + roda do mouse para aproximar e enquadrar.
4. Para pequenos erros, escolha **Restaurar** ou **Apagar** e pinte diretamente sobre o resultado; Ctrl+Z desfaz.
5. Escolha o formato e clique em **Salvar imagem** (Ctrl+S).

### Gerenciar modelos (aba Modelos)

![Aba Modelos com o catálogo para download](docs/images/bg-remover-modelos.png)

Na aba **Modelos**, no topo da janela, escolha um modelo do catálogo e clique em **Descarregar**. Os arquivos ficam no diretório de dados do aplicativo (`~/.local/share/bg-remover/models` no Linux) e passam a aparecer no seletor de modelos da aba Editor, além de serem encontrados automaticamente pelo CLI. Modelos maiores (ISNet, U²-Net completo) geram recortes melhores em cabelo e contornos finos; o U²-Netp continua sendo o padrão leve. Os downloads usam HTTPS e os modelos com hash conhecido têm verificação SHA256; qualquer modelo pode ser removido do disco na mesma aba. Os modelos instalados são verificados por tamanho e, quando há hash publicado, por SHA256; a lista mostra **Incompleto** ou **Corrompido** quando o arquivo não confere.

### Processamento em lote

Na barra inferior, escolha a pasta de entrada e a pasta de saída, depois clique em **Processar lote** (o botão vira **Cancelar lote** durante a execução). As imagens geradas recebem o sufixo `_rembg` e usam o formato escolhido no painel (PNG, JPEG ou WebP); se já existir um arquivo com esse nome, o aplicativo cria uma variação numerada em vez de sobrescrevê-lo. Em JPEG, o resultado é composto sobre o fundo escolhido (branco se for transparente), porque o formato não tem canal alfa.

### Linha de comando

![Aba Ajuda com resumo, atalhos e links](docs/images/bg-remover-ajuda.png)

```bash
# Forma posicional (compatível com versões anteriores)
./build/bg_remover_cli <pasta_entrada> [pasta_saida] [modelo.onnx]

# Forma com opções
./build/bg_remover_cli <pasta_entrada> -o <pasta_saida> -m <modelo.onnx>

# Ajuda e versão
./build/bg_remover_cli --help
./build/bg_remover_cli --version
```

O CLI grava PNGs com o sufixo `_rembg` e nunca sobrescreve arquivos existentes. Se não indicar o modelo, a GUI e o CLI procuram `u2netp.onnx` junto do executável, no diretório de instalação, nos modelos instalados pela aba Modelos e por fim no diretório atual. Nos pacotes Linux, `man bg_remover_cli` mostra a referência completa.

## Testes

```bash
ctest --test-dir build --output-on-failure
```

O projeto inclui testes de pós-processamento, do planejamento de lote (BatchPlan), do caminho de modelos, da validação de integridade dos modelos, da extensão de salvamento, do lote do CLI, dos formatos de imagem (WebP, ignorado se o plugin não estiver instalado), de inferência e dois testes de fumo da GUI em português e inglês (headless, com `QT_QPA_PLATFORM=offscreen`). A integração contínua compila e testa Linux (com ASan/UBSan), Windows x64 e macOS universal2; no Windows, também valida se o ZIP final contém as DLLs e plugins necessários, e no macOS valida o bundle e o CLI deployados.

## Criar um pacote Linux

```bash
cmake -S . -B build-release -DINSTALLED=ON \
  -DBG_REMOVER_FETCH_ONNX_RUNTIME=ON \
  -DBG_REMOVER_BUNDLE_MODELS=ON
cmake --build build-release --parallel
cd build-release
cpack -G DEB
```

`BG_REMOVER_BUNDLE_MODELS=ON` exige que `u2netp.onnx` esteja na raiz, impedindo a criação de um pacote que não funciona.

## Criar um pacote macOS

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DINSTALLED=ON \
  -DBG_REMOVER_FETCH_ONNX_RUNTIME=ON -DBG_REMOVER_BUNDLE_MODELS=ON \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_INSTALL_PREFIX="$PWD/dist"
cmake --build build-release --parallel
cmake --install build-release
macdeployqt dist/bg_remover_gui.app -always-overwrite \
  -executable=dist/bg_remover_gui.app/Contents/MacOS/bg_remover_cli
ditto -c -k --keepParent dist/bg_remover_gui.app bg-remover_macos_universal2.zip
```

O bundle inclui Qt, ONNX Runtime e o modelo leve; o CLI fica em `Contents/MacOS`. O ZIP é assinado ad-hoc pelo CI, não notarizado.

## Estrutura do projeto

```text
bg-remover/
├── .github/                  # CI, releases, templates e Dependabot
├── CONTRIBUTING.md / SECURITY.md / CHANGELOG.md
├── backgroundremover.*       # Inferência ONNX e geração da máscara
├── imagepostprocessor.*      # Filtros alpha e composição do fundo
├── batchprocessor.* + batchplan.h  # Processamento de diretórios (CLI + GUI)
├── mainwindow.*              # Interface Qt (abas, lote, zoom)
├── maskeditcanvas.*          # Pincel e edição da máscara
├── modelmanager.*            # Aba Modelos: catálogo e download
├── modelvalidator.*          # Verificação de tamanho/SHA256 dos modelos
├── savepath.h                # Extensão coerente com o formato de saída
├── helpwidget.*              # Aba Ajuda
├── apptheme.*                # Temas escuro/claro
├── main.cpp / main_cli.cpp   # Arranque da GUI e do CLI
├── bg-remover.desktop + *.metainfo.xml + *.1  # Integração Linux
└── test_*.cpp                # Testes automatizados
```

Os modelos, o runtime extraído, os diretórios de build e os resultados locais não entram no Git. Os releases automatizados obtêm o runtime e o modelo leve, verificam os respectivos checksums e criam os pacotes distribuíveis.

## Limitações atuais

- U²-Netp privilegia velocidade; cabelo e transparências muito finas podem precisar de correção manual. U²-Net oferece melhor segmentação, mas é substancialmente maior.
- O ZIP do Windows é gerado e verificado pelo CI (DLLs e plugins presentes), mas ainda **não foi testado numa instalação limpa do Windows** — se encontrar problemas, abra uma [issue](https://github.com/MarielCarlosFrias/bg-remover/issues).
- O `.AppImage` é gerado pelo CI e **validado localmente** (empacotado com `linuxdeploy`, com Qt, ONNX Runtime e modelos incluídos); ainda não foi testado em várias distros diferentes.
- O ZIP do macOS é gerado e verificado pelo CI (bundle, CLI e ONNX Runtime), mas **não foi executado numa instalação limpa do macOS** neste ambiente de desenvolvimento. A assinatura é ad-hoc e não notarizada; se o Gatekeeper bloquear, use **Abrir** no menu de contexto.
- Modelos de 1024×1024 (ISNet) são mais lentos em CPU: a máscara tem 16× mais pixels de entrada que a U²-Netp.

## Contribuir

Sugestões, relatos de bugs e pull requests são bem-vindos. Leia [CONTRIBUTING.md](CONTRIBUTING.md) para compilar, correr os testes e enviar alterações. Para vulnerabilidades, siga as instruções de [SECURITY.md](SECURITY.md) em vez de abrir uma issue pública.

## Créditos

Modelos [U²-Net](https://github.com/xuebinqin/U-2-Net) e arquivos ONNX distribuídos pelo projeto [rembg](https://github.com/danielgatis/rembg).

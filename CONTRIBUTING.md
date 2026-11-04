# Contribuir com o Aletheia

Obrigado pelo interesse! Este projeto é um aplicativo desktop em C++17/Qt 6 que
executa modelos ONNX localmente. Contribuições de código, documentação,
traduções e relatos de bugs são bem-vindas.

## Antes de começar

- Procure uma issue existente antes de abrir outra.
- Para mudanças grandes, abra uma issue ou discussão primeiro para alinhar o
  escopo.
- Para vulnerabilidades, **não** abra uma issue pública: siga
  [SECURITY.md](SECURITY.md).

## Preparar o ambiente

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake qt6-base-dev qt6-image-formats-plugins
```

### Windows

Use Visual Studio 2022, CMake e Qt 6 para MSVC. O CI usa Qt 6.7.3 x64.

### macOS

Use Xcode Command Line Tools, CMake e Qt 6.7.3. O CI instala o Qt com
`aqtinstall` e compila universal2 (`x86_64;arm64`).

## Compilar e testar

```bash
cmake -S . -B build -DBG_REMOVER_FETCH_ONNX_RUNTIME=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Os testes de inferência e os testes GUI completos precisam de `u2netp.onnx` na
raiz do projeto:

```bash
wget https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2netp.onnx
```

Sem o modelo, o CMake desativa os testes de inferência/lote e o teste GUI
valida apenas a construção da interface e a recuperação de erros.

Para os testes GUI em modo headless:

```bash
QT_QPA_PLATFORM=offscreen BG_REMOVER_LANG=pt ./build/test_gui ./u2netp.onnx
QT_QPA_PLATFORM=offscreen BG_REMOVER_LANG=en ./build/test_gui ./u2netp.onnx
```

## Estilo e convenções

- C++17, Qt 6 e CMake; evite novas dependências sem discussão prévia.
- Mantenha o estilo do ficheiro em volta (4 espaços, nomes existentes,
  comentários em português quando o ficheiro já usa português).
- Textos de interface usam `TR("português", "English")`; atualize os dois
  idiomas na mesma alteração.
- Preserve a compatibilidade do CLI e dos caminhos de configuração/modelos.
- Não inclua modelos `.onnx`, builds, binários ou imagens de teste no commit.
- Prefira alterações pequenas e focadas, com testes quando possível.

## Testes e empacotamento

O projeto usa CTest. Antes de enviar:

```bash
ctest --test-dir build --output-on-failure
```

Para validar o pacote Linux:

```bash
cmake -S . -B build-release -DINSTALLED=ON \
  -DBG_REMOVER_FETCH_ONNX_RUNTIME=ON -DBG_REMOVER_BUNDLE_MODELS=ON
cmake --build build-release --parallel
cd build-release && cpack -G DEB
```

No macOS, o CI valida o bundle com `macdeployqt` e o CLI dentro de
`Contents/MacOS`. No Windows, o CI valida o ZIP portátil.

## Pull requests

1. Crie um branch a partir de `main`.
2. Faça commits pequenos e com mensagens claras (ex.: `fix: ...`,
   `feat: ...`, `docs: ...`, `ci: ...`).
3. Atualize README/CHANGELOG/man page quando a alteração for visível ao
   utilizador.
4. Garanta que `ctest` passa e que o CI fica verde.
5. Descreva o problema, a solução e como testou no template do PR.

Ao contribuir, você concorda que a sua contribuição é licenciada sob a licença
MIT do projeto.

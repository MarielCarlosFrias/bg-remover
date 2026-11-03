# Changelog

Todas as alterações relevantes são documentadas aqui. O formato segue
[Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/) e o projeto usa
[Versionamento Semântico](https://semver.org/lang/pt-BR/).

## [Não lançado]

### Adicionado

- Suporte a macOS com bundle universal2 (Intel + Apple Silicon), Qt e ONNX
  Runtime incluídos, CLI dentro do `.app` e ZIP de release.
- Testes de fumo da GUI em português e inglês, executados em modo offscreen.
- Opções `-o/--output` e `-m/--model` no CLI, mantendo os formatos posicionais
  anteriores.
- Templates de issue/PR, Dependabot, `CONTRIBUTING.md`, `SECURITY.md` e este
  changelog.
- Notas de release geradas automaticamente no GitHub.

### Alterado

- Empacotamento Linux/Windows/macOS e checksums de release num único
  `SHA256SUMS`.
- README, man page e metadados AppStream atualizados.

### Corrigido

- Vários problemas da auditoria 1.1.0: estado ocupado da GUI, resultados de
  imagens antigas, colisões de nomes no lote, validação de modelos, timeout da
  verificação de atualizações, undo/redo limitado por memória e extensão de
  salvamento coerente com o formato.

## [1.1.0] - 2026-10-31

### Adicionado

- Comparação antes/depois com divisor arrastável.
- Copiar resultado para a área de transferência (Ctrl+Shift+C).
- Exportação em PNG, JPEG e WebP; JPEG composto sobre o fundo escolhido.
- Escolha do formato no processamento em lote.
- Desfazer/refazer multi-nível com limite de memória.
- Aba **Modelos** com catálogo, download, verificação SHA256 e remoção.
- Verificação de novas versões com opt-out.
- Arrastar e soltar imagens ou pastas.
- Tema claro/escuro e preferências persistentes.
- Zoom de 50% a 400% centrado no cursor.

## [1.0.5] - 2026-10-31

### Corrigido

- Nome do pacote DEB/ZIP alinhado com a versão do CMake.
- Verificação do ZIP Windows independente do nome da versão.

## [1.0.4] - 2026-10-31

### Alterado

- AppImage gerado no `ubuntu-latest` (Qt 6.2 do Ubuntu 22.04 não configurava).

## [1.0.3] - 2026-10-31

### Corrigido

- `Categories` do `.desktop` com valor reconhecido (`2DGraphics`).

## [1.0.2] - 2026-10-31

### Adicionado

- Deteção do tamanho de entrada do modelo (ISNet 1024).
- Publicação da release mesmo quando um job isolado falha.
- Downloads do CI com novas tentativas.

### Corrigido

- Orientação EXIF ao abrir imagens.
- Lista de modelos e bloqueio da sessão ONNX durante o processamento.
- Validação de tamanho das descargas e confirmação de remoção de modelos.
- Filtros alfa e composição de imagem mais rápidos.

## [1.0.1] - 2026-10-14

### Adicionado

- Interface em português e inglês, com seletor de idioma.
- Screenshots reais no README.
- AppImage na release e RPATH portátil (`$ORIGIN`).

## [1.0.0] - 2026-10-14

### Adicionado

- Primeira versão pública: GUI Qt, CLI, remoção de fundo com U²-Netp/U²-Net,
  processamento em lote e pacotes Linux/Windows.

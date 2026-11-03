## Resumo

<!-- O que muda e por quê? -->

## Tipo de alteração

- [ ] Correção de bug
- [ ] Nova funcionalidade
- [ ] Melhoria de desempenho
- [ ] Refactor sem mudança de comportamento
- [ ] Documentação / tradução
- [ ] CI / empacotamento

## Como testar

<!-- Comandos, imagens ou passos usados para validar. -->

```bash
ctest --test-dir build --output-on-failure
```

## Checklist

- [ ] O código compila sem novos avisos relevantes.
- [ ] Os testes existentes passam (`ctest`).
- [ ] Adicionei/atualizei testes para o comportamento novo.
- [ ] Atualizei README/CHANGELOG/man page quando necessário.
- [ ] Os textos novos têm versão em português e inglês (`TR(pt, en)`).
- [ ] Não incluí modelos `.onnx`, builds ou binários no commit.
- [ ] Mantive a compatibilidade do CLI e dos caminhos de configuração.

## Issues relacionadas

<!-- Ex.: Closes #123 -->

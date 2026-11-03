# Segurança

## Versões suportadas

Recebemos relatos para a versão estável mais recente (atualmente a série
**1.1.x**). Se possível, confirme o problema na última release antes de
reportar.

## Como reportar uma vulnerabilidade

**Não abra uma issue pública.** Use o canal privado de Security Advisories do
GitHub:

1. Abra <https://github.com/MarielCarlosFrias/bg-remover/security/advisories/new>.
2. Descreva o impacto, a versão afetada, os passos para reproduzir e, se
   possível, uma proposta de correção.
3. Indique se pretende ser creditado no aviso.

Responderemos o mais rápido possível. Este é um projeto mantido por
voluntários, por isso não há um prazo garantido; agradecemos a paciência e a
divulgação responsável.

## Âmbito

Fazem parte do âmbito:

- Execução de modelos ONNX e processamento de imagens.
- Download e validação de modelos na aba **Modelos**.
- Verificação de atualizações (consulta à API de releases do GitHub).
- Pacotes de release (DEB, AppImage, ZIP Windows e ZIP macOS).

Fora do âmbito, normalmente:

- Vulnerabilidades em dependências externas (Qt, ONNX Runtime, modelos U²-Net)
  que já tenham correção publicada — reporte também ao projeto upstream.
- Problemas que exigem acesso físico à máquina ou uma instalação já
  comprometida.
- Avisos de que o aplicativo não é notarizado no macOS ou não tem assinatura
  comercial no Windows.

## Privacidade

O BG Remover não envia imagens para servidores. A única ligação de rede
automática é a consulta à API pública de releases do GitHub para avisar sobre
novas versões. Ela pode ser desativada com
`BG_REMOVER_NO_UPDATE_CHECK=1` ou com `update/check=false` nas configurações.
Os downloads de modelos só acontecem quando o utilizador os inicia.

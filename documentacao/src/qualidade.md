---
title: Qualidade gráfica
group: Renderização
order: 3
---

# Qualidade gráfica

Ajuste a aparência e o desempenho em **Configurações → Gráficos**
(<kbd>Ctrl+,</kbd>). Tudo é aplicado **na hora**, sem reiniciar o editor.

## Presets

| Preset | Uso |
|--------|-----|
| **Ultra** | Máximo de fidelidade (para máquinas fortes) |
| **High** | Balanceado |
| **Medium** | Bom meio-termo |
| **Low** | Máquina fraca / integrada |
| **Custom** | Você ajustou manualmente |

O editor já começa com o preset adequado ao seu hardware — em geral você não
precisa mexer em nada.

## Configurações

| Opção | O que faz |
|-------|-----------|
| **Resolução interna** | Renderiza em resolução menor e amplia (ganho grande de FPS); >1 = superamostragem |
| **MSAA** | Suaviza as bordas (2x, 4x, 8x) |
| **Shadow map (CSM)** | Resolução das sombras (512–4096) |
| **Suavização de sombra** | Bordas de sombra mais suaves |
| **Bloom** | Brilho dos objetos luminosos (limiar, intensidade, largura) |
| **Tonemapping** | Acabamento de cor: ACES (cinematográfico) / Reinhard (suave) / Filmic (contraste) |
| **SSAO** | Sombras de contato entre objetos (amostras, raio) |
| **Exposição** | Brilho geral da imagem |
| **VSync** | Trava ao refresh do monitor (evita "rasgo" de tela) |
| **Névoa** | Atmosfera por distância (densidade) |
| **Vinheta / Aberração / Grão** | Efeitos de câmera de filme |

## Ajuste automático

O editor escolhe padrões adequados pela **sua** placa de vídeo e depois aplica
suas escolhas manuais por cima. As configurações ficam salvas e são mantidas
entre sessões.

## Dicas de desempenho

- **Resolução interna 0.75** é o ganho mais rápido em máquinas fracas;
- **SSAO** e **bloom** são os efeitos que mais pesam — desligue no Low;
- **Sombras em 2048** é um bom equilíbrio;
- Se o jogo estiver lento, diminua primeiro o preset — **Low** costuma
  resolver.

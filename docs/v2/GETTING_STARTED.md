# Kizuri Engine v2 Getting Started

## PT-BR

### Para usuarios

- A `v2` esta em construcao e entra no repositório como nova base oficial de evolucao.
- A `v1` continua existindo como legado, referencia e ponto de comparacao tecnica.
- O foco da `v2` e formar uma fundacao proprietaria para jogos `2D` e `3D` completos.

### O que esperar do inicio

- Documentacao antes de grandes features.
- Modulos pequenos, claros e bem nomeados.
- Build reproduzivel.
- Arquitetura preparada para crescer sem virar monolito.

### Para desenvolvedores

- Toda nova feature deve dizer a qual modulo pertence.
- Toda feature nova precisa declarar impacto em runtime, editor, build e assets.
- APIs publicas da `v2` devem priorizar estabilidade de contrato.
- Sistemas novos precisam prever observabilidade basica: logs, asserts e pontos de profiling.

### Fluxo de contribuicao inicial

1. Ler `README.md` desta pasta.
2. Ler `ARCHITECTURE.md`.
3. Ler `ROADMAP.md`.
4. Ler `SCRIPTING_CSHARP.md` se tocar em scripting.
5. Implementar de forma incremental, sempre deixando a base compilavel.

### Norte de qualidade

- Cada modulo deve ser pequeno o suficiente para manutencao por equipe reduzida.
- Cada decisao tecnica precisa justificar custo de longo prazo.
- Performance e experiencia de criacao devem andar juntas.

---

## EN

### For users

- `v2` is under construction and enters the repository as the new official evolution track.
- `v1` remains as legacy, reference, and technical comparison point.
- The focus of `v2` is to build a proprietary foundation for complete `2D` and `3D` games.

### What to expect from the start

- Documentation before major features.
- Small, clear, and well-named modules.
- Reproducible builds.
- Architecture prepared to grow without turning into a monolith.

### For developers

- Every new feature must state which module owns it.
- Every new feature must declare impact on runtime, editor, build, and assets.
- Public `v2` APIs should prioritize contract stability.
- New systems need basic observability: logs, asserts, and profiling points.

### Initial contribution flow

1. Read `README.md` in this folder.
2. Read `ARCHITECTURE.md`.
3. Read `ROADMAP.md`.
4. Read `SCRIPTING_CSHARP.md` when working on scripting.
5. Implement incrementally and always leave the base compilable.

### Quality north star

- Each module must stay small enough for maintenance by a reduced team.
- Every technical decision must justify long-term cost.
- Performance and creation experience must advance together.

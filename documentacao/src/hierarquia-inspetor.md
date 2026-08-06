---
title: Hierarquia e Inspetor
group: Editor
order: 6
---

# Hierarquia e Inspetor

Juntos, esses dois painéis controlam **o que existe** na cena e **como cada
coisa se comporta**.

## Hierarquia

- Lista todas as **entidades** da cena.
- **Clique** para selecionar (o Inspetor e o viewport acompanham).
- **Arraste** uma entidade sobre outra para torná-la **filha** (a filha herda
  a posição/rotação/escala do pai).
- **Botão direito**: Duplicar (**Ctrl+D**), Salvar como Prefab, Excluir (**Del**).
- A **busca** (campo no topo) filtra por nome e mostra uma lista plana.

## Inspetor

Mostra os **componentes** da entidade selecionada. Cada componente tem seu
próprio cabeçalho (pode ser recolhido/expandido):

- **Transform** — posição, rotação e escala (e hierarquia pai/filho)
- **MeshRenderer** — malha + material (albedo, metal, rugosidade, mapas, emissão, POM, espelho)
- **SpriteRenderer** — sprite, cor, ordem de renderização, flip
- **LightComponent** — direcional/ponto/spot, cor, intensidade, alcance, cone
- **CameraComponent** — tipo (2D/3D), FOV, clipes, câmera principal
- **Rigidbody/Colliders** — física 2D e 3D
- **AudioSource** — som a tocar, volume, espacialização
- **ScriptComponent** — associa um script C# do projeto
- **Animator** — animação esquelética
- **Tilemap**, **ParticleSystem**, **UI** e mais

### Adicionar componentes

Clique em **Adicionar Componente** na parte inferior do Inspetor.

### Undo/redo

Toda edição no Inspetor (valores, cores, componentes) é desfazível com
**Ctrl+Z** e refazível com **Ctrl+Y**.

::: dica
Mudou algo e não gostou? **Ctrl+Z** desfaz até mudanças de um arrastar no
Inspetor — o histórico captura o valor antes da edição começar.
:::

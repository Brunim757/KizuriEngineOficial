---
title: Tilemap
group: Componentes
order: 11
---

# Tilemap

O **Tilemap** cria pisos/paredes a partir de tiles de um atlas, com colisão
automática — a base para jogos de plataforma, metroidvania e RPG.

## Como usar

1. Adicione o componente **Tilemap** a uma entidade.
2. No Inspetor, defina o **Atlas** (imagem com os tiles), **colunas/linhas**
   do atlas e o **tamanho do tile**.
3. Selecione a entidade e use a **ferramenta de pintura** no viewport 2D:
   - **Botão esquerdo** pinta com o valor do pincel
   - **Botão direito** apaga (valor 0)
4. Marque tiles como **sólidos** (`SolidTileValues`) para gerar colisão.

## Colisão

- Tiles sólidos viram colisores Box2D automaticamente.
- O personagem com **Rigidbody2D** colide com o chão/paredes sem precisar de
  nada extra.
- `Scene.Raycast2D` também acerta os tiles sólidos.

## Scripting

```csharp
// ler o valor do tile em uma posição
int valor = // a API atual do tilemap: criar e pintar tiles
Entity.SetTile(tileX, tileY, valor);   // pinta um tile
// mudar um tile em runtime
Entity.SetTile(tileX, tileY, 3);
```

::: dica
Para colisões finas de tilemap (pisos de uma via), o mapa expõe a grade ao
**Raycast2D** e aos eventos de colisão — veja [Física 2D](fisica-2d.html).
:::

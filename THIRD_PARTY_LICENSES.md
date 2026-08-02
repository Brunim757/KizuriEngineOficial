# Licenças de Terceiros — Kizuri Engine

A Kizuri Engine é construída sobre as seguintes bibliotecas de código aberto,
baixadas via CMake FetchContent. Cada uma mantém sua licença original:

| Biblioteca      | Uso na engine                          | Licença     |
|------------------|------------------------------------------|-------------|
| GLFW             | Janela, contexto OpenGL, input            | zlib/libpng |
| glm              | Matemática (vetores, matrizes)            | MIT         |
| EnTT             | ECS (Entity Component System)             | MIT         |
| spdlog           | Logging                                    | MIT         |
| Bullet3          | Física 3D                                  | zlib        |
| box2d            | Física 2D                                  | MIT         |
| miniaudio        | Áudio (mixagem, streaming, 3D espacial)   | MIT / Unlicense |
| stb (stb_image)  | Carregamento de imagens                    | MIT / Public Domain |
| tinyobjloader    | Importação de modelos .obj                 | MIT         |
| Dear ImGui       | UI de debug e editor                       | MIT         |
| nlohmann/json    | Serialização de cenas (.kzscene)          | MIT         |

Nenhuma dessas bibliotecas concede direitos adicionais sobre o código
proprietário da Kizuri Engine em si — apenas sobre o próprio código delas.

## JetBrains Mono
Copyright 2020 The JetBrains Mono Project Authors.
Licenciada sob a SIL Open Font License 1.1. Texto completo em
`engine/resources/fonts/JetBrainsMono-OFL.txt`. Fonte usada pela UI do
Kizuri Editor e embutida no executável em tempo de build a partir de
`engine/resources/fonts/` (ver `cmake/EmbedResource.cmake`).

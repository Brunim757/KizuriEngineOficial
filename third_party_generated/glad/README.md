# glad (OpenGL 4.5 core)

Bindings gerados pelo [glad2](https://github.com/Dav1dde/glad) e comitados
direto neste repositório — não são baixados nem gerados durante o build.
Isso evita depender de Python/pip disponíveis no runner de CI e deixa o
build 100% determinístico.

## Por que comitado em vez de gerado no build

Antes disso, `engine/CMakeLists.txt` compilava um loader OpenGL escrito à
mão (`cmake/glad_stub/`) que só cobria o subconjunto de funções/enums usado
até então pelo renderer. Toda vez que uma feature nova precisava de uma
função GL que ninguém tinha adicionado ao stub, o build quebrava com erros
tipo `'glGenRenderbuffers' was not declared in this scope`. Trocamos pelo
glad real pra isso parar de acontecer.

## Como foi gerado

Via [gen.glad.sh](https://gen.glad.sh):

- Language: `C/C++`
- Specification: `OpenGL`
- API gl: `Version 4.5`, Profile `Core`
- Options: `Loader`

Ou por linha de comando:

```bash
pip install glad2
python -m glad --api gl:core=4.5 --out-path ./glad_out c
```

## Como regerar (se precisar de uma extensão nova)

1. Gere de novo como acima (adicionando a extensão necessária em `Extensions`
   no gen.glad.sh, ou `--extensions=...` na CLI).
2. Substitua os 3 arquivos:
   - `include/glad/gl.h`
   - `include/KHR/khrplatform.h`
   - `src/gl.c`
3. Não precisa mexer no `engine/CMakeLists.txt` nem em nenhum `.cpp` — os
   nomes de função/enum não mudam entre gerações, só o conjunto coberto.

## Uso na engine

`Window.cpp` chama `gladLoadGL((GLADloadfunc)glfwGetProcAddress)` depois de
criar o contexto GL. Todo o resto (`Renderer3D`, `Shader`, `Texture`,
`Buffer`, `Framebuffer`, `RenderCommand`) inclui `<glad/gl.h>` normalmente.

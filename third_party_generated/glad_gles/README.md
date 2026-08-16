# glad (OpenGL ES 3.x — Android)

Bindings gerados pelo [glad2](https://github.com/Dav1dde/glad) e comitados
direto neste repositório (mesmo padrão do `../glad` de desktop). É o loader
usado ALVO Android (`KZ_PLATFORM_ANDROID`): o desktop usa o `../glad` (GL
4.5 core) sem nenhuma mudança.

## Como foi gerado

```
python -m glad --profile core --api "gles2=3.2" --generator c \
    --out-path third_party_generated/glad_gles --reproducible
```

O `--api "gles2=3.2"` cobre GLES 3.0/3.1/3.2 (a spec Khronos chama a família
de "GLES2"; as funções têm os MESMOS nomes da API GL desktop — GLES 3.x
compartilha o núcleo com GL 3.3).

## Por que "glad/gl.h" também existe aqui

O código da engine inclui sempre `<glad/gl.h>`. Em Android esse header é
um shim de 4 linhas que aponta pro loader GLES (`glad/glad.h`) e traduz a
função de load (`gladLoadGL` -> `gladLoadGLES2Loader`). Assim nenhum
arquivo de renderer muda de include por plataforma.

## Diferença de macros de versão

O loader GLES reporta versões como `GLAD_GL_ES_VERSION_3_1`/`3_2` (e não
`GLAD_GL_VERSION_*` do desktop) — defeito de feature é checado com essas
macros (ex: MSAA por textura em `Renderer3D.cpp`).

## Regenerar

```
pip install glad2
python -m glad --profile core --api "gles2=3.2" --generator c \
    --out-path third_party_generated/glad_gles --reproducible
# recria o shim include/glad/gl.h (não é gerado pelo glad)
```
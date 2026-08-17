#!/usr/bin/env bash
# make-appimage.sh — empacota o jogo Linux como AppImage único (autônomo).
#
#   $1 = bin/ do build Linux (deve ter KizuriGame + libKizuriEngine.so)
#   $2 = conteúdo do jogo (Start.kzscene + assets) — entra em usr/share/kizuri/game/
#   $3 = onde gravar o .AppImage
#
# O AppImage roda a partir do diretório do jogo (AppRun faz cd), então a
# cena inicial resolve como no desktop. Tudo dentro de UM arquivo: nada de
# "grudado" com outra plataforma — cada export sai 100% da sua.
set -euo pipefail

BIN_DIR="${1:?bin dir}"; GAME_SRC="${2:?game assets}"; OUT="${3:?output .AppImage}"

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
APP="$WORK/KizuriGame.AppDir"

echo "==> (1/4) estrutura AppDir"
mkdir -p "$APP/usr/bin" "$APP/usr/share/kizuri/game" "$APP/usr/share/applications" \
         "$APP/usr/share/icons/hicolor/256x256/apps"
cp "$BIN_DIR/KizuriGame"       "$APP/usr/bin/kizuri-game"
cp "$BIN_DIR/libKizuriEngine.so" "$APP/usr/bin/"
cp -r "$GAME_SRC"/.           "$APP/usr/share/kizuri/game/"
cp "$REPO_ROOT/packaging/icons/kizuri-torii-256.png" \
   "$APP/usr/share/icons/hicolor/256x256/apps/kizuri-torii.png"

cat > "$APP/usr/share/applications/kizuri-game.desktop" <<'DESKTOP'
[Desktop Entry]
Name=Kizuri Game
Comment=Jogo feito com a Kizuri Engine
Exec=kizuri-game
Icon=kizuri-torii
Type=Application
Categories=Game;
Terminal=false
DESKTOP

# AppRun: o jogo espera a cena relativa ao CWD — roda a partir do diretório
# dos dados do AppImage.
cat > "$APP/AppRun" <<'RUN'
#!/usr/bin/env bash
HERE="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
cd "$HERE/usr/share/kizuri/game" || exit 1
exec "$HERE/usr/bin/kizuri-game"
RUN
chmod +x "$APP/AppRun" "$APP/usr/bin/kizuri-game"
ln -sf usr/share/icons/hicolor/256x256/apps/kizuri-torii.png "$APP/.DirIcon"

echo "==> (2/4) baixando o runtime do AppImageKit (sem FUSE — CI/VM ok)"
RUNTIME="$WORK/runtime-x86_64"
curl -fsSL -o "$RUNTIME" \
    https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64
chmod +x "$RUNTIME"

echo "==> (3/4) montando o AppImage (tipo 2: runtime + squashfs)"
# appimagetool exige FUSE/glibc novos no runner; o formato é simples:
# arquivo único = [runtime][squashfs do AppDir]. mksquashfs do squashfs-tools.
command -v mksquashfs >/dev/null || { echo "ERRO: instale squashfs-tools"; exit 1; }
mksquashfs "$APP" "$WORK/kizuri.squashfs" -root-owned -noappend
cat "$RUNTIME" "$WORK/kizuri.squashfs" > "$OUT"
chmod +x "$OUT"

echo "==> (4/4) verificação"
test -s "$OUT" || { echo "ERRO: AppImage vazio"; exit 1; }
chmod +x "$OUT"
echo "OK: $OUT ($(du -h "$OUT" | cut -f1))"
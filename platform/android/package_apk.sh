#!/usr/bin/env bash
# package_apk.sh — monta o APK do KizuriGame a partir dos artefatos do build:
#
#   $1 = bin/ (build android): deve conter libKizuriGame.so + libKizuriEngine.so
#   $2 = dotnet/ (publish SampleGame p/ android-arm64): runtime CoreCLR + assemblies
#   $3 = assets do jogo (Start.kzscene etc; vai pro filesDir como "game/")
#   $4 = Android SDK build-tools (aapt2/zipalign/apksigner)
#   $5 = onde gravar o APK final
#
# Sem gradle/JDK de aplicação: aapt2 link + zip + zipalign + apksigner (debug
# keystore gerado na hora). rodar via CI (.github/workflows/build.yml).
set -euo pipefail

BIN_DIR="${1:?bin dir}"; DOTNET_DIR="${2:?dotnet dir}"; GAME_ASSETS="${3:?game assets}"
SDK_BUILD_TOOLS="${4:?build-tools}"
OUT_APK="${5:-android-release.apk}"

TOOLS=$SDK_BUILD_TOOLS
AAPT2="$TOOLS/aapt2"; ZIPALIGN="$TOOLS/zipalign"; APKSIGNER="$TOOLS/apksigner"

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT

# Diagnóstico da CI: mostra todos os comandos e decisions (temporário).
set -x

echo "==> (1/6) recursos + assets"
RES="$WORK/res"
mkdir -p "$RES"
echo "default" > "$RES/compatibility_version.txt"
cp -r "$GAME_ASSETS" "$WORK/game_src"

echo "==> (2/6) aapt2 compile/link"
"$AAPT2" compile --dir "$RES" -o "$WORK/resources.zip"
# O android.jar fica em <sdk>/platforms/android-<api>/ — procura em
# candidatos; o manifest é 100% nativo (sem resources do framework), então
# o -I é opcional: se não achar, linka sem ele.
PLATFORM_JAR=""
for cand in \
    "$ANDROID_SDK_ROOT/platforms/android-34/android.jar" \
    "$(dirname "$SDK_BUILD_TOOLS")/platforms/android-34/android.jar" \
    "$HOME/Android/Sdk/platforms/android-34/android.jar" \
    "$ANDROID_HOME/platforms/android-34/android.jar"; do
    [ -f "$cand" ] && { PLATFORM_JAR="$cand"; break; }
done
LINK_ARGS=()
if [ -n "$PLATFORM_JAR" ]; then
    echo "PLATFORM_JAR=$PLATFORM_JAR"
    LINK_ARGS+=(-I "$PLATFORM_JAR")
else
    echo "AVISO: android.jar não encontrado — aapt2 link sem -I."
fi
"$AAPT2" link \
    -o "$WORK/unsigned.apk" \
    "${LINK_ARGS[@]}" \
    --manifest "$SCRIPT_DIR/AndroidManifest.xml" \
    --min-sdk-version 24 --target-sdk-version 34 \
    --version-code 1 --version-name 0.8.0 \
    "$WORK/resources.zip"

echo "==> (3/6) .so do jogo + engine (lib/<abi>/)"
mkdir -p "$WORK/apk/lib/arm64-v8a"
cp "$BIN_DIR/libKizuriGame.so"   "$WORK/apk/lib/arm64-v8a/"
cp "$BIN_DIR/libKizuriEngine.so" "$WORK/apk/lib/arm64-v8a/"

echo "==> (4/6) runtime .NET (CoreCLR) + assemblies + cena"
# O jogo C# + libcoreclr/libhostfxr da engine vão em assets/dotnet
# (extraídos pro filesDir na primeira execução — AndroidEntry.cpp).
mkdir -p "$WORK/apk/assets/dotnet" "$WORK/apk/assets/game"
cp -r "$DOTNET_DIR"/. "$WORK/apk/assets/dotnet/"
cp -r "$GAME_ASSETS"/. "$WORK/apk/assets/game/"
# O .deps.json/runtimeconfig são gerados pelo publish — confere:
test -f "$WORK/apk/assets/dotnet/SampleGame.runtimeconfig.json" \
    || { echo "ERRO: publish sem runtimeconfig (SampleGame.runtimeconfig.json)"; exit 1; }

echo "==> (5/6) libs+assets direto no APK + zipalign"
# Adiciona lib/ e assets/ DENTRO do apk gerado pelo aapt2 (evita zip
# intermediário separado — fonte de "I/O error" do apksigner).
( cd "$WORK/apk" && zip -qr "$WORK/unsigned.apk" lib assets )
"$ZIPALIGN" -f 4 "$WORK/unsigned.apk" "$WORK/aligned.apk"

echo "==> (6/6) assinar (keystore de debug gerado na hora)"
KEY="$WORK/debug.keystore"
keytool -genkeypair -v -keystore "$KEY" -storepass android -alias androiddebugkey \
    -keypass android -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Kizuri CI, OU=CI, O=Kizuri, L=CI, S=CI, C=BR" >/dev/null 2>&1
"$APKSIGNER" sign --ks "$KEY" --ks-pass pass:android --key-pass pass:android \
    --out "$OUT_APK" "$WORK/aligned.apk"

echo "OK: $OUT_APK ($(du -h "$OUT_APK" | cut -f1))"
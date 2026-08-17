#pragma once
// Updater.hpp — auto-atualização do editor (menu Ajuda > Verificar
// Atualizações e checagem no início). A engine consulta uma API pública
// do site do dev, ex:
//
//     GET https://seusite.com/api/version
//     -> { "version": "0.37.0", "download_url": "https://.../KizuriGame-windows-latest-x86_64.zip" }
//
// Se a versão remota for maior que KIZURI_VERSION, o hub pergunta
// "Nova versão disponível — deseja atualizar?" (Sim / Não, com opção de
// não perguntar de novo). O download + extração + relaunch são feitos
// aqui. Configuração: update_settings.json (api_url e skip_version) no
// diretório de trabalho (bin/).
#include <string>

namespace kizuri {

struct UpdateInfo {
    std::string Version;       // ex: "0.37.0"
    std::string DownloadUrl;   // ex: "https://.../*.zip"
    bool Valid = false;
};

class Updater {
public:
    // Configuração persistida (update_settings.json no CWD).
    static std::string GetApiUrl();
    static void SetApiUrl(const std::string& url);
    static std::string GetSkipVersion();
    static void SetSkipVersion(const std::string& version);

    static std::string GetLocalVersion(); // KIZURI_VERSION

    // Consulta a API (síncrono — rode na thread). Só retorna valid=true se
    // a versão remota for MAIOR que a local.
    static UpdateInfo CheckForUpdate(std::string& outError);

    // Baixa downloadUrl para destPath (zip). Síncrono.
    static bool Download(const std::string& url, const std::string& destPath,
                         std::string& outError,
                         void (*progress)(double fraction) = nullptr);

    // Extrai o zip (miniz) por cima do diretório de trabalho (bin/) —
    // os zips de release tem bin/ dentro. Faz backup do binário atual
    // antes de substituir. Síncrono.
    static bool Install(const std::string& zipPath, std::string& outError);

    // Relança o próprio executável no próximo frame (o processo atual
    // precisa morrer: o updater aplica ANTES de chamar, com arquivos já
    // no lugar).
    static void Relaunch(std::string& outError);
};

} // namespace kizuri
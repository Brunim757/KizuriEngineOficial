// Updater.cpp — implementação do auto-update (libcurl + miniz).
#include "Updater.hpp"
#include <kizuri/core/Version.hpp>
#include <kizuri/core/Log.hpp>
#include <kizuri/core/CommandLineArgs.hpp>

#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include "miniz.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>

namespace fs = std::filesystem;

namespace kizuri {

namespace {

constexpr const char* kSettingsFile = "update_settings.json";
// API do site oficial da engine (exemplo do README/comunidade). O dev pode
// trocar em Ajuda > Configurar Atualizações; o default já aponta pro site.
constexpr const char* kDefaultApiUrl = "https://kizuri-studio.vercel.app/api/version";

std::string s_ApiUrl;         // cache em memória (lido do disco uma vez)
std::string s_SkipVersion;
bool s_SettingsLoaded = false;

void LoadSettingsOnce() {
    if (s_SettingsLoaded) return;
    s_SettingsLoaded = true;
    std::ifstream in(kSettingsFile);
    if (!in.is_open()) return;
    try {
        nlohmann::json j;
        in >> j;
        s_ApiUrl = j.value("api_url", std::string(kDefaultApiUrl));
        s_SkipVersion = j.value("skip_version", std::string());
    } catch (...) {
        KZ_CORE_WARN("update_settings.json inválido — ignorando.");
    }
}

void SaveSettings() {
    nlohmann::json j;
    j["api_url"] = s_ApiUrl;
    j["skip_version"] = s_SkipVersion;
    std::ofstream out(kSettingsFile);
    if (out.is_open()) out << j.dump(2);
}

// callback do libcurl: acumula a resposta em string.
size_t WriteStringCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

struct DownloadCtx { FILE* File = nullptr; double Total = 0.0; };

size_t WriteFileCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<DownloadCtx*>(userdata);
    size_t written = fwrite(ptr, 1, size * nmemb, ctx->File);
    return written;
}

int ProgressCb(void* userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<DownloadCtx*>(userdata);
    if (ctx->Total <= 0.0) ctx->Total = (double)dltotal;
    return 0;
}

std::string HttpGet(const std::string& url, std::string& outError, long* outHttpCode = nullptr) {
    std::string body;
    CURL* curl = curl_easy_init();
    if (!curl) { outError = "curl_easy_init falhou."; return {}; }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "KizuriUpdater/" KIZURI_VERSION);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteStringCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if (outHttpCode) *outHttpCode = code;
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK) {
        outError = curl_easy_strerror(rc);
        return {};
    }
    if (code != 200) {
        outError = "HTTP " + std::to_string(code);
        return {};
    }
    return body;
}

std::string GetExecutablePath() {
    const auto& args = GetCommandLineArgs();
    if (!args.empty()) return args[0];
    return ".";
}

} // namespace

std::string Updater::GetLocalVersion() { return KIZURI_VERSION; }

std::string Updater::GetApiUrl() { LoadSettingsOnce(); return s_ApiUrl.empty() ? kDefaultApiUrl : s_ApiUrl; }
void Updater::SetApiUrl(const std::string& url) { LoadSettingsOnce(); s_ApiUrl = url; SaveSettings(); }
std::string Updater::GetSkipVersion() { LoadSettingsOnce(); return s_SkipVersion; }
void Updater::SetSkipVersion(const std::string& v) { LoadSettingsOnce(); s_SkipVersion = v; SaveSettings(); }

UpdateInfo Updater::CheckForUpdate(std::string& outError) {
    UpdateInfo info;
    std::string apiUrl = GetApiUrl();
    if (apiUrl.empty()) {
        outError = "API de atualizações não configurada (Ajuda > Configurar Atualizações).";
        return info;
    }
    if (apiUrl.find("/api/version") == std::string::npos) {
        if (!apiUrl.empty() && apiUrl.back() != '/') apiUrl += '/';
        apiUrl += "api/version";
    }

    long httpCode = 0;
    KZ_CORE_INFO("Updater: consultando {0}...", apiUrl);
    std::string body = HttpGet(apiUrl, outError, &httpCode);
    KZ_CORE_INFO("Updater: HTTP {0} — {1}", httpCode, body.substr(0, 160));
    if (body.empty()) {
        KZ_CORE_ERROR("Updater: falha na consulta: {0}", outError);
        return info;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(body);
        info.Version = j.value("version", std::string());
        info.DownloadUrl = j.value("download_url", std::string());
        if (info.Version.empty() || info.DownloadUrl.empty()) {
            outError = "Resposta da API sem version/download_url.";
            return info;
        }
    } catch (...) {
        outError = "Resposta da API não é JSON válido.";
        return info;
    }

    // Compara semver simples (major.minor.patch).
    auto parts = [](const std::string& v) {
        int a = 0, b = 0, c = 0;
        sscanf(v.c_str(), "%d.%d.%d", &a, &b, &c);
        struct { int a, b, c; } out{ a, b, c }; return out;
    };
    auto local = parts(GetLocalVersion());
    auto remote = parts(info.Version);
    bool newer = (remote.a > local.a) || (remote.a == local.a && remote.b > local.b)
        || (remote.a == local.a && remote.b == local.b && remote.c > local.c);
    KZ_CORE_INFO("Updater: local={0} remoto={1} -> {2}",
                  GetLocalVersion(), info.Version, newer ? "NOVA VERSÃO" : "atualizado");
    info.Valid = newer;
    return info;
}

bool Updater::Download(const std::string& url, const std::string& destPath,
                       std::string& outError, void (*progress)(double)) {
    KZ_CORE_INFO("Updater: baixando {0} -> {1}", url, destPath);
    CURL* curl = curl_easy_init();
    if (!curl) { outError = "curl_easy_init falhou."; return false; }

    DownloadCtx ctx;
    ctx.File = fopen(destPath.c_str(), "wb");
    if (!ctx.File) {
        outError = "Não foi possível criar " + destPath;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "KizuriUpdater/" KIZURI_VERSION);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);

    CURLcode rc = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    fclose(ctx.File);

    if (rc != CURLE_OK) {
        outError = curl_easy_strerror(rc);
        std::remove(destPath.c_str());
        return false;
    }
    if (code != 200) {
        outError = "HTTP " + std::to_string(code);
        std::remove(destPath.c_str());
        return false;
    }
    if (progress) progress(1.0);
    return true;
}

bool Updater::Install(const std::string& zipPath, std::string& outError) {
    // Extrai o zip no CWD (bin/): os zips de release têm bin/ dentro. Antes
    // de sobrescrever, renomeia o executável atual pra .old (Windows não
    // deixa sobrescrever um exe em execução; o relaunch usa o novo).
    std::error_code ec;
    std::string exe = GetExecutablePath();
    if (fs::exists(exe, ec)) {
        fs::path bak = fs::path(exe).string() + ".old";
        fs::remove(bak, ec);
        fs::rename(exe, bak, ec);
        if (ec) {
            outError = "Não foi possível renomear o executável em uso: " + ec.message();
            return false;
        }
    }

    // Magic bytes do ZIP ('PK\x03\x04'). Builds antigos da CI geravam TAR
    // com extensão .zip (tar -a) — dá um diagnóstico claro em vez de
    // "zip inválido".
    {
        std::ifstream zipHead(zipPath, std::ios::binary);
        char magic[2] = { 0, 0 };
        zipHead.read(magic, 2);
        if (magic[0] != 'P' || magic[1] != 'K') {
            outError = "O arquivo baixado não é um ZIP (era um build antigo que gerava TAR com "
                       "extensão .zip — republique o zip na nova CI)";
            return false;
        }
    }

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, zipPath.c_str(), 0)) {
        outError = "Zip de atualização inválido ou corrompido.";
        return false;
    }

    // Valida que o zip é da MESMA plataforma do editor (senão a "nova
    // versão" quebra o editor). Espera: bin/KizuriEditor(.exe) +
    // bin/KizuriEngine(.dll/.so).
#if defined(_WIN32)
    const char* kEditorEntry = "bin/KizuriEditor.exe";
    const char* kEngineEntry = "bin/KizuriEngine.dll";
#else
    const char* kEditorEntry = "bin/KizuriEditor";
    const char* kEngineEntry = "bin/libKizuriEngine.so";
#endif
    if (mz_zip_reader_locate_file_v2(&zip, kEditorEntry, nullptr, 0, nullptr) < 0 ||
        mz_zip_reader_locate_file_v2(&zip, kEngineEntry, nullptr, 0, nullptr) < 0) {
        outError = std::string("O zip do download não é desta plataforma (esperava ") +
                   kEditorEntry + " e " + kEngineEntry +
                   "). Verifique o download_url da API.";
        mz_zip_reader_end(&zip);
        return false;
    }

    mz_uint fileCount = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < fileCount; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) continue;
        if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;

        // Caminho de saída: remove o prefixo "bin/" do zip de release.
        // O zip do Windows usa backslash (bin\KizuriEditor.exe) — normaliza
        // pra '/' antes de remover o prefixo, senão a pasta bin/ fica duplicada.
        std::string rel = st.m_filename;
        for (char& c : rel) if (c == '\\') c = '/';
        while (rel.size() >= 4 && rel[0] == 'b' && rel[1] == 'i' && rel[2] == 'n' && rel[3] == '/')
            rel = rel.substr(4);
        if (rel.empty()) continue;

        fs::path outPath = fs::current_path() / rel;
        if (outPath.has_parent_path()) {
            std::error_code mk;
            fs::create_directories(outPath.parent_path(), mk);
        }
        size_t size = 0;
        void* data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
        if (!data) {
            outError = "Falha ao extrair " + std::string(st.m_filename);
            mz_zip_reader_end(&zip);
            return false;
        }
        {
            std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
            out.write(static_cast<const char*>(data), (std::streamsize)size);
        }
        mz_free(data);
    }
    mz_zip_reader_end(&zip);
    KZ_CORE_INFO("Atualização instalada a partir de {0}.", zipPath);
    return true;
}

void Updater::Relaunch(std::string& outError) {
    std::string exe = GetExecutablePath();
    if (exe.empty()) { outError = "Caminho do executável desconhecido."; return; }
#if defined(_WIN32)
    std::string cmd = "start \"\" \"" + exe + "\"";
    int rc = std::system(cmd.c_str());
    if (rc != 0) outError = "Falha ao relançar (start).";
#else
    // Relança com pequeno delay (deixa o processo atual morrer e o sistema
    // liberar os binários).
    std::string cmd = "sh -c '(sleep 1; exec \"" + exe + "\") >/dev/null 2>&1 &'";
    int rc = std::system(cmd.c_str());
    if (rc != 0) outError = "Falha ao relançar (sh).";
#endif
}

} // namespace kizuri
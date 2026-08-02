#pragma once
#include <string>

namespace kizuri {

// Diálogos nativos do sistema operacional pra escolher arquivo/pasta — Windows usa a API COM
// IFileDialog (a mesma janela moderna do Explorer, com painel de favoritos/navegação). Em
// outras plataformas ainda não tem backend nativo — devolve string vazia; o chamador deve
// simplesmente manter o campo de texto manual como alternativa, nunca travar nisso.
class FileDialog {
public:
    // filterName: descrição mostrada no combo de tipo (ex: "Cena Kizuri"). filterPattern:
    // padrão de arquivo estilo Win32 (ex: "*.kzscene", ou "*.dll;*.so" pra mais de uma extensão).
    // Devolve string vazia se o usuário cancelar.
    static std::string OpenFile(const std::string& filterName, const std::string& filterPattern);
    static std::string SaveFile(const std::string& filterName, const std::string& filterPattern, const std::string& defaultExtension);
    static std::string SelectFolder();
};

} // namespace kizuri

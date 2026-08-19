#include "kizuri/core/FileDialog.hpp"

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define INITGUID
    #include <Windows.h>
    #include <shobjidl.h>
#endif

namespace kizuri {

#if defined(_WIN32)

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), wide.data(), len);
    return wide;
}

static std::string WideToUtf8(const wchar_t* wide) {
    if (!wide || !wide[0]) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    std::string out(len > 0 ? len - 1 : 0, '\0');
    if (len > 0) WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), len, nullptr, nullptr);
    return out;
}

static std::string RunDialog(bool save, bool pickFolder, const std::wstring& filterName,
                              const std::wstring& filterPattern, const std::wstring& defaultExt) {
    std::string result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool weInitialized = (hr == S_OK || hr == S_FALSE);

    IFileDialog* dialog = nullptr;
    HRESULT createHr = CoCreateInstance(
        save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        save ? IID_IFileSaveDialog : IID_IFileOpenDialog, reinterpret_cast<void**>(&dialog));

    if (SUCCEEDED(createHr) && dialog) {
        DWORD flags = 0;
        dialog->GetOptions(&flags);
        flags |= FOS_FORCEFILESYSTEM;
        if (pickFolder) flags |= FOS_PICKFOLDERS;
        dialog->SetOptions(flags);

        if (!pickFolder && !filterPattern.empty()) {
            COMDLG_FILTERSPEC spec[] = { { filterName.c_str(), filterPattern.c_str() } };
            dialog->SetFileTypes(1, spec);
            if (!defaultExt.empty()) dialog->SetDefaultExtension(defaultExt.c_str());
        }

        if (SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                    result = WideToUtf8(path);
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (weInitialized) CoUninitialize();
    return result;
}

std::string FileDialog::OpenFile(const std::string& filterName, const std::string& filterPattern) {
    return RunDialog(false, false, Utf8ToWide(filterName), Utf8ToWide(filterPattern), L"");
}

std::string FileDialog::SaveFile(const std::string& filterName, const std::string& filterPattern, const std::string& defaultExtension) {
    return RunDialog(true, false, Utf8ToWide(filterName), Utf8ToWide(filterPattern), Utf8ToWide(defaultExtension));
}

std::string FileDialog::SelectFolder() {
    return RunDialog(false, true, L"", L"", L"");
}

#else

std::string FileDialog::OpenFile(const std::string&, const std::string&) { return {}; }
std::string FileDialog::SaveFile(const std::string&, const std::string&, const std::string&) { return {}; }
std::string FileDialog::SelectFolder() { return {}; }

#endif

}

#pragma once












#include <string>

namespace kizuri {

struct UpdateInfo {
    std::string Version;       
    std::string DownloadUrl;   
    bool Valid = false;
};

class Updater {
public:
    
    static std::string GetApiUrl();
    static void SetApiUrl(const std::string& url);
    static std::string GetSkipVersion();
    static void SetSkipVersion(const std::string& version);

    static std::string GetLocalVersion(); 

    
    
    static UpdateInfo CheckForUpdate(std::string& outError);

    
    static bool Download(const std::string& url, const std::string& destPath,
                         std::string& outError,
                         void (*progress)(double fraction) = nullptr);

    
    
    
    static bool Install(const std::string& zipPath, std::string& outError);

    
    
    
    static void Relaunch(std::string& outError);
};

} 
#pragma once





#include <functional>
#include <string>

namespace kizuri {

class AndroidExporter {
public:
    struct Tools {
        bool Ok = false;
        std::string Missing;              
        std::string NdKToolchain;         
        std::string Aapt2, Zipalign;      
        std::string Apksigner;            
        bool Java = false;
        std::string Cmake, Ninja, Dotnet; 
        bool HaveNinja = true;
    };

    
    static Tools DetectTools();

    
    
    
    static bool Export(const Tools& tools,
                       const std::string& engineRoot,   
                       const std::string& gameCsproj,    
                       const std::string& gameContentDir,
                       const std::string& gameName,
                       const std::string& outputDir,
                       std::string& outApkPath,
                       std::string& outError,
                       const std::function<void(const std::string&)>& log = nullptr);
};

} 
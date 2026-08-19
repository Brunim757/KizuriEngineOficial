#pragma once
#include <string>

namespace kizuri {





class FileDialog {
public:
    
    
    
    static std::string OpenFile(const std::string& filterName, const std::string& filterPattern);
    static std::string SaveFile(const std::string& filterName, const std::string& filterPattern, const std::string& defaultExtension);
    static std::string SelectFolder();
};

} 

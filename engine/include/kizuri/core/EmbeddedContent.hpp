#pragma once
#include <cstddef>
#include <string>

namespace kizuri {


struct EmbeddedBuffer {
    const void* Data = nullptr;
    std::size_t Size = 0;
};





bool GetEmbeddedResource(const std::string& name, EmbeddedBuffer& out);
bool HasEmbeddedResource(const std::string& name);


inline bool IsEmbeddedPath(const std::string& path) {
    return path.rfind("kzres://", 0) == 0;
}

inline std::string EmbeddedNameFromPath(const std::string& path) {
    return path.substr(8);
}

} 

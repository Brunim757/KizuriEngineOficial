#include "kizuri/core/EmbeddedContent.hpp"
#include "EmbeddedContent.gen.hpp"

namespace kizuri {

bool GetEmbeddedResource(const std::string& name, EmbeddedBuffer& out) {
    for (std::size_t i = 0; i < detail::kEmbeddedFileCount; ++i) {
        const auto& entry = detail::kEmbeddedFiles[i];
        if (name == entry.Name) {
            out.Data = entry.Data;
            out.Size = entry.Size;
            return true;
        }
    }
    return false;
}

bool HasEmbeddedResource(const std::string& name) {
    for (std::size_t i = 0; i < detail::kEmbeddedFileCount; ++i) {
        if (name == detail::kEmbeddedFiles[i].Name) return true;
    }
    return false;
}

}

#pragma once
#include "kizuri/scripting/NativeScript.hpp"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace kizuri {

class ScriptRegistry {
public:
    using FactoryFn = std::function<NativeScript*()>;

    void Register(const std::string& className, FactoryFn factory) {
        m_Factories[className] = std::move(factory);
    }

    template<typename T>
    void Register(const std::string& className) {
        Register(className, []() -> NativeScript* { return new T(); });
    }

    bool IsRegistered(const std::string& className) const {
        return m_Factories.find(className) != m_Factories.end();
    }

    NativeScript* Create(const std::string& className) const {
        auto it = m_Factories.find(className);
        return it != m_Factories.end() ? it->second() : nullptr;
    }

    std::vector<std::string> GetClassNames() const {
        std::vector<std::string> names;
        names.reserve(m_Factories.size());
        for (auto& [name, factory] : m_Factories) names.push_back(name);
        return names;
    }

    void Clear() { m_Factories.clear(); }

private:
    std::unordered_map<std::string, FactoryFn> m_Factories;
};

}

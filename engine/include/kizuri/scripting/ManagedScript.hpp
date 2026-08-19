#pragma once



#include "kizuri/scripting/NativeScript.hpp"
#include <string>

namespace kizuri {

class ManagedScript final : public NativeScript {
public:
    explicit ManagedScript(std::string className) : m_ClassName(std::move(className)) {}
    ~ManagedScript() override;

protected:
    void OnCreate() override;
    void OnDestroy() override;
    void OnUpdate(Timestep ts) override;
    void OnCollisionBegin(Entity other) override;
    void OnCollisionEnd(Entity other) override;

private:
    std::string m_ClassName;
    void* m_ManagedHandle = nullptr;
};

} 

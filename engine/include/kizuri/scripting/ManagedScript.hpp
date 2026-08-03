#pragma once
// ManagedScript — NativeScript cujo corpo é um script C# gerenciado pelo
// CoreCLR (ver CoreCLRHost). A Scene enxerga só NativeScript*, então nada
// no runtime precisa saber que o jogo agora é C#.
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

} // namespace kizuri

#pragma once
#include "kizuri/core/Event.hpp"
#include "kizuri/core/Timestep.hpp"
#include <string>
#include <vector>

namespace kizuri {



class Layer {
public:
    explicit Layer(std::string name = "Layer") : m_DebugName(std::move(name)) {}
    virtual ~Layer() = default;

    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void OnUpdate([[maybe_unused]] Timestep ts) {}
    virtual void OnImGuiRender() {}
    virtual void OnEvent([[maybe_unused]] Event& event) {}

    const std::string& GetName() const { return m_DebugName; }

protected:
    std::string m_DebugName;
};

class LayerStack {
public:
    ~LayerStack();

    void PushLayer(Layer* layer);
    void PushOverlay(Layer* overlay);
    void PopLayer(Layer* layer);
    void PopOverlay(Layer* overlay);

    std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
    std::vector<Layer*>::iterator end()   { return m_Layers.end(); }

private:
    std::vector<Layer*> m_Layers;
    unsigned int m_LayerInsertIndex = 0;
};

} 

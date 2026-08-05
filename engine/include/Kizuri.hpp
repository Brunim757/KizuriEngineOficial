#pragma once

// =============================================================================
//  KIZURI ENGINE — header único de conveniência
//  Inclua apenas este arquivo em qualquer jogo/ferramenta feita com a Kizuri.
// =============================================================================

// ---- Core ----
#include "kizuri/Core.hpp"
#include "kizuri/core/Log.hpp"
#include "kizuri/core/LogHistory.hpp"
#include "kizuri/core/Application.hpp"
#include "kizuri/core/FileDialog.hpp"
#include "kizuri/core/Layer.hpp"
#include "kizuri/core/Window.hpp"
#include "kizuri/core/Input.hpp"
#include "kizuri/core/Event.hpp"
#include "kizuri/core/Timestep.hpp"
#include "kizuri/core/ImGuiLayer.hpp"
#include "kizuri/core/UUID.hpp"
#include "kizuri/core/EmbeddedContent.hpp"

// ---- Renderer ----
#include "kizuri/renderer/Renderer.hpp"
#include "kizuri/renderer/Renderer2D.hpp"
#include "kizuri/renderer/Renderer3D.hpp"
#include "kizuri/renderer/GraphicsSettings.hpp"
#include "kizuri/renderer/TextRenderer.hpp"
#include "kizuri/renderer/RenderCommand.hpp"
#include "kizuri/renderer/Camera.hpp"
#include "kizuri/renderer/Shader.hpp"
#include "kizuri/renderer/Texture.hpp"
#include "kizuri/renderer/Buffer.hpp"
#include "kizuri/renderer/Framebuffer.hpp"

// ---- ECS / Scene ----
#include "kizuri/ecs/Scene.hpp"
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/scene/SceneSerializer.hpp"
#include "kizuri/scene/Prefab.hpp"
#include "kizuri/scene/EditorHistory.hpp"
#include "kizuri/project/Project.hpp"

// ---- Scripting (base da futura KZScript) ----
#include "kizuri/scripting/NativeScript.hpp"
#include "kizuri/scripting/ScriptRegistry.hpp"
#include "kizuri/scripting/ScriptEngine.hpp"

// ---- Física ----
// (Rigidbody2DComponent / BoxCollider2DComponent / Rigidbody3DComponent já
//  disponíveis via kizuri/ecs/Components.hpp — a simulação roda dentro de Scene)

// ---- Áudio ----
#include "kizuri/audio/AudioEngine.hpp"

// ---- Assets ----
#include "kizuri/assets/AssetManager.hpp"
#include "kizuri/project/GameExporter.hpp"

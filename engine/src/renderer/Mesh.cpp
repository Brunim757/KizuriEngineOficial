#include "kizuri/renderer/Renderer3D.hpp"
#include "kizuri/core/Log.hpp"
#include "kizuri/core/EmbeddedContent.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

namespace {

// Carrega a textura de uma textura_view do glTF: prioriza a imagem embutida
// no buffer (glb) via CreateFromMemory (sem flip — UV glTF), senão o arquivo
// externo (uri). Devolve null se não houver textura.
kizuri::Ref<kizuri::Texture2D> LoadGLTFTexture(const cgltf_texture_view& view) {
    if (!view.texture || !view.texture->image) return nullptr;
    const cgltf_image* img = view.texture->image;
    if (img->buffer_view && img->buffer_view->buffer && img->buffer_view->buffer->data) {
        const cgltf_buffer_view* bv = img->buffer_view;
        return kizuri::Texture2D::CreateFromMemory((const uint8_t*)bv->buffer->data + bv->offset, bv->size,
                                                   img->name ? img->name : "gltf");
    }
    if (img->uri) return kizuri::Texture2D::Create(img->uri);
    return nullptr;
}

} // namespace

#include <glm/gtc/constants.hpp>
#include <cmath>
#include <string>

namespace kizuri {

Mesh::Mesh(const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices) {
    m_VertexArray = CreateRef<VertexArray>();

    auto vb = CreateRef<VertexBuffer>((float*)vertices.data(), (uint32_t)(vertices.size() * sizeof(Vertex3D)));
    vb->SetLayout({
        { ShaderDataType::Float3, "a_Position" },
        { ShaderDataType::Float3, "a_Normal" },
        { ShaderDataType::Float2, "a_TexCoord" },
        { ShaderDataType::Float4, "a_JointIndices" },
        { ShaderDataType::Float4, "a_JointWeights" },
    });
    m_VertexArray->AddVertexBuffer(vb);

    auto ib = CreateRef<IndexBuffer>((uint32_t*)indices.data(), (uint32_t)indices.size());
    m_VertexArray->SetIndexBuffer(ib);
    m_IndexCount = (uint32_t)indices.size();

    if (!vertices.empty()) {
        m_BoundsMin = m_BoundsMax = vertices[0].Position;
        for (const auto& v : vertices) {
            m_BoundsMin = glm::min(m_BoundsMin, v.Position);
            m_BoundsMax = glm::max(m_BoundsMax, v.Position);
        }
    }
}

Ref<Mesh> Mesh::CreateCube() {
    std::vector<Vertex3D> v = {
        // frente
        {{-0.5f,-0.5f, 0.5f},{0,0,1},{0,0}}, {{0.5f,-0.5f,0.5f},{0,0,1},{1,0}}, {{0.5f,0.5f,0.5f},{0,0,1},{1,1}}, {{-0.5f,0.5f,0.5f},{0,0,1},{0,1}},
        // trás
        {{0.5f,-0.5f,-0.5f},{0,0,-1},{0,0}}, {{-0.5f,-0.5f,-0.5f},{0,0,-1},{1,0}}, {{-0.5f,0.5f,-0.5f},{0,0,-1},{1,1}}, {{0.5f,0.5f,-0.5f},{0,0,-1},{0,1}},
        // esquerda
        {{-0.5f,-0.5f,-0.5f},{-1,0,0},{0,0}}, {{-0.5f,-0.5f,0.5f},{-1,0,0},{1,0}}, {{-0.5f,0.5f,0.5f},{-1,0,0},{1,1}}, {{-0.5f,0.5f,-0.5f},{-1,0,0},{0,1}},
        // direita
        {{0.5f,-0.5f,0.5f},{1,0,0},{0,0}}, {{0.5f,-0.5f,-0.5f},{1,0,0},{1,0}}, {{0.5f,0.5f,-0.5f},{1,0,0},{1,1}}, {{0.5f,0.5f,0.5f},{1,0,0},{0,1}},
        // topo
        {{-0.5f,0.5f,0.5f},{0,1,0},{0,0}}, {{0.5f,0.5f,0.5f},{0,1,0},{1,0}}, {{0.5f,0.5f,-0.5f},{0,1,0},{1,1}}, {{-0.5f,0.5f,-0.5f},{0,1,0},{0,1}},
        // base
        {{-0.5f,-0.5f,-0.5f},{0,-1,0},{0,0}}, {{0.5f,-0.5f,-0.5f},{0,-1,0},{1,0}}, {{0.5f,-0.5f,0.5f},{0,-1,0},{1,1}}, {{-0.5f,-0.5f,0.5f},{0,-1,0},{0,1}},
    };
    std::vector<uint32_t> idx;
    for (uint32_t face = 0; face < 6; ++face) {
        uint32_t o = face * 4;
        idx.insert(idx.end(), { o+0, o+1, o+2, o+2, o+3, o+0 });
    }
    return CreateRef<Mesh>(v, idx);
}

Ref<Mesh> Mesh::CreatePlane() {
    std::vector<Vertex3D> v = {
        {{-0.5f, 0.0f, -0.5f}, {0,1,0}, {0,0}},
        {{ 0.5f, 0.0f, -0.5f}, {0,1,0}, {1,0}},
        {{ 0.5f, 0.0f,  0.5f}, {0,1,0}, {1,1}},
        {{-0.5f, 0.0f,  0.5f}, {0,1,0}, {0,1}},
    };
    std::vector<uint32_t> idx = { 0,1,2,2,3,0 };
    return CreateRef<Mesh>(v, idx);
}

Ref<Mesh> Mesh::CreateSphere(uint32_t sectors, uint32_t stacks) {
    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;
    const float PI = glm::pi<float>();

    for (uint32_t i = 0; i <= stacks; ++i) {
        float stackAngle = PI / 2 - (float)i * (PI / stacks);
        float xy = cosf(stackAngle);
        float z = sinf(stackAngle);
        for (uint32_t j = 0; j <= sectors; ++j) {
            float sectorAngle = (float)j * (2 * PI / sectors);
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);
            glm::vec3 pos(x, z, y);
            glm::vec2 uv((float)j / sectors, (float)i / stacks);
            vertices.push_back({ pos, glm::normalize(pos), uv });
        }
    }
    for (uint32_t i = 0; i < stacks; ++i) {
        uint32_t k1 = i * (sectors + 1);
        uint32_t k2 = k1 + sectors + 1;
        for (uint32_t j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) indices.insert(indices.end(), { k1, k2, k1 + 1 });
            if (i != (stacks - 1)) indices.insert(indices.end(), { k1 + 1, k2, k2 + 1 });
        }
    }
    return CreateRef<Mesh>(vertices, indices);
}

Ref<Mesh> Mesh::LoadFromOBJ(const std::string& path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
        KZ_CORE_ERROR("Falha ao carregar a malha OBJ '{0}': {1}", path, err);
        return CreateCube();
    }
    if (!warn.empty()) KZ_CORE_WARN("Aviso do tinyobjloader ({0}): {1}", path, warn);

    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex3D vertex{};
            vertex.Position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            if (index.normal_index >= 0) {
                vertex.Normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }
            if (index.texcoord_index >= 0) {
                vertex.TexCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            indices.push_back((uint32_t)vertices.size());
            vertices.push_back(vertex);
        }
    }
    KZ_CORE_INFO("Malha OBJ carregada: {0} ({1} vértices).", path, vertices.size());
    return CreateRef<Mesh>(vertices, indices);
}

Ref<Mesh> Mesh::CreateCylinder(uint32_t sectors) {
    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;
    const float PI = glm::pi<float>();
    const float radius = 0.5f, halfH = 0.5f;

    for (uint32_t i = 0; i <= sectors; ++i) {
        float a = (float)i * 2.0f * PI / (float)sectors;
        glm::vec2 ring(glm::cos(a), glm::sin(a));
        // corpo (2 anéis: topo e base)
        vertices.push_back({ { ring.x * radius, halfH, ring.y * radius }, { ring.x, 0.0f, ring.y }, { a / (2.0f * PI), 1.0f } });
        vertices.push_back({ { ring.x * radius, -halfH, ring.y * radius }, { ring.x, 0.0f, ring.y }, { a / (2.0f * PI), 0.0f } });
        // tampas (centro + borda) — topo com normal +Y, base -Y
        vertices.push_back({ { 0.0f, halfH, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.5f, 0.5f } });
        vertices.push_back({ { ring.x * radius, halfH, ring.y * radius }, { 0.0f, 1.0f, 0.0f }, { (ring.x * 0.5f + 0.5f), (ring.y * 0.5f + 0.5f) } });
        vertices.push_back({ { 0.0f, -halfH, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.5f, 0.5f } });
        vertices.push_back({ { ring.x * radius, -halfH, ring.y * radius }, { 0.0f, -1.0f, 0.0f }, { (ring.x * 0.5f + 0.5f), (ring.y * 0.5f + 0.5f) } });
    }

    for (uint32_t i = 0; i < sectors; ++i) {
        uint32_t top0 = i * 6, top1 = (i + 1) * 6;
        uint32_t b0 = top0 + 1, b1 = top1 + 1;
        // parede
        indices.insert(indices.end(), { top0, b0, top1, top1, b0, b1 });
        // tampas
        uint32_t tc0 = top0 + 2, te0 = top0 + 3, tc1 = top1 + 2, te1 = top1 + 3;
        uint32_t bc0 = top0 + 4, be0 = top0 + 5, bc1 = top1 + 4, be1 = top1 + 5;
        indices.insert(indices.end(), { tc0, te0, tc1, tc1, te0, te1 });
        indices.insert(indices.end(), { bc0, bc1, be0, be0, bc1, be1 });
    }
    return CreateRef<Mesh>(vertices, indices);
}

Ref<Mesh> Mesh::CreateCone(uint32_t sectors) {
    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;
    const float PI = glm::pi<float>();
    const float radius = 0.5f, halfH = 0.5f;

    for (uint32_t i = 0; i <= sectors; ++i) {
        float a = (float)i * 2.0f * PI / (float)sectors;
        glm::vec2 ring(glm::cos(a), glm::sin(a));
        glm::vec3 edge(ring.x * radius, -halfH, ring.y * radius);
        glm::vec3 normal = glm::normalize(glm::vec3(ring.x, radius / glm::max(halfH * 2.0f, 0.001f), ring.y));
        vertices.push_back({ { 0.0f, halfH, 0.0f }, normal, { 0.5f, 0.5f } });            // ápice
        vertices.push_back({ edge, normal, { a / (2.0f * PI), 0.0f } });                    // borda
        vertices.push_back({ { 0.0f, -halfH, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.5f, 0.5f } }); // centro da base
        vertices.push_back({ edge, { 0.0f, -1.0f, 0.0f }, { (ring.x * 0.5f + 0.5f), (ring.y * 0.5f + 0.5f) } }); // borda da base
    }
    for (uint32_t i = 0; i < sectors; ++i) {
        uint32_t o0 = i * 4, o1 = (i + 1) * 4;
        indices.insert(indices.end(), { o0, o1 + 1, o0 + 1 });       // lateral
        indices.insert(indices.end(), { o0 + 2, o0 + 3, o1 + 3, o1 + 3, o1 + 2, o0 + 2 }); // base
    }
    return CreateRef<Mesh>(vertices, indices);
}

Ref<Mesh> Mesh::CreateCapsule(uint32_t sectors, uint32_t stacks) {
    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;
    const float PI = glm::pi<float>();
    const float radius = 0.25f, halfBody = 0.35f;

    // hemisférios: amostra ângulo de elevação de 0..90 no topo e 90..180 embaixo.
    auto PushRing = [&](float phi, float y) {
        for (uint32_t i = 0; i <= sectors; ++i) {
            float a = (float)i * 2.0f * PI / (float)sectors;
            glm::vec3 dir(glm::cos(a) * glm::sin(phi), glm::cos(phi), glm::sin(a) * glm::sin(phi));
            vertices.push_back({ glm::vec3(dir.x * radius, y + dir.y * radius, dir.z * radius), dir,
                                 { (float)i / (float)sectors, (y + radius + halfBody) / (2.0f * radius + 2.0f * halfBody) } });
        }
    };

    for (uint32_t s = 0; s <= stacks; ++s) {
        float phi = PI * (0.5f - 0.5f * (float)s / (float)stacks); // topo: phi de 90°→0°
        PushRing(phi, halfBody);
    }
    for (uint32_t s = 0; s <= stacks; ++s) {
        float phi = PI * (0.5f + 0.5f * (float)s / (float)stacks); // base: phi de 0°→-90°
        PushRing(phi, -halfBody);
    }
    uint32_t rings = (uint32_t)vertices.size() / (sectors + 1);
    for (uint32_t r = 0; r + 1 < rings; ++r) {
        for (uint32_t i = 0; i < sectors; ++i) {
            uint32_t a0 = r * (sectors + 1) + i, a1 = a0 + 1;
            uint32_t b0 = a0 + (sectors + 1), b1 = b0 + 1;
            indices.insert(indices.end(), { a0, b0, a1, a1, b0, b1 });
        }
    }
    return CreateRef<Mesh>(vertices, indices);
}

Ref<Mesh> Mesh::CreateTorus(uint32_t majorSeg, uint32_t minorSeg) {
    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;
    const float PI = glm::pi<float>();
    const float major = 0.4f, minor = 0.12f;

    for (uint32_t i = 0; i <= majorSeg; ++i) {
        float ma = (float)i * 2.0f * PI / (float)majorSeg;
        glm::vec2 mc(glm::cos(ma), glm::sin(ma));
        for (uint32_t j = 0; j <= minorSeg; ++j) {
            float mi = (float)j * 2.0f * PI / (float)minorSeg;
            glm::vec3 dir(glm::cos(mi) * mc.x, glm::sin(mi), glm::cos(mi) * mc.y);
            vertices.push_back({ glm::vec3((major + minor * glm::cos(mi)) * mc.x, minor * glm::sin(mi), (major + minor * glm::cos(mi)) * mc.y),
                                 dir, { (float)i / (float)majorSeg, (float)j / (float)minorSeg } });
        }
    }
    for (uint32_t i = 0; i < majorSeg; ++i) {
        for (uint32_t j = 0; j < minorSeg; ++j) {
            uint32_t a0 = i * (minorSeg + 1) + j, a1 = a0 + 1;
            uint32_t b0 = a0 + (minorSeg + 1), b1 = b0 + 1;
            indices.insert(indices.end(), { a0, b0, a1, a1, b0, b1 });
        }
    }
    return CreateRef<Mesh>(vertices, indices);
}

Ref<Mesh> Mesh::FromSource(const std::string& source) {
    if (source == "builtin:cube")     return CreateCube();
    if (source == "builtin:plane")    return CreatePlane();
    if (source == "builtin:sphere")   return CreateSphere();
    if (source == "builtin:cylinder") return CreateCylinder();
    if (source == "builtin:cone")     return CreateCone();
    if (source == "builtin:capsule")  return CreateCapsule();
    if (source == "builtin:torus")    return CreateTorus();
    if (!source.empty()) {
        if (IsEmbeddedPath(source)) {
            EmbeddedBuffer buf;
            if (GetEmbeddedResource(EmbeddedNameFromPath(source), buf))
                return LoadFromGLTFMemory(buf.Data, buf.Size);
            KZ_CORE_ERROR("Recurso embutido não encontrado: {0}", source);
            return CreateCube();
        }
        size_t dot = source.find_last_of('.');
        if (dot != std::string::npos) {
            std::string ext = source.substr(dot + 1);
            if (ext == "glb" || ext == "gltf") return LoadFromGLTF(source);
        }
        return LoadFromOBJ(source);
    }
    return CreateCube();
}

// Flatten compartilhado: converte um cgltf_data já parseado (meshes +
// primitivas triangulares) num Mesh único. Usado tanto pelo caminho de
// arquivo quanto pelo caminho em memória.
static Ref<Mesh> BuildMeshFromGLTF(cgltf_data* data, const std::string& label) {
    // Acha os accessors certos por atributo dentro de cada primitiva.
    auto FindAttr = [](const cgltf_primitive& prim, cgltf_attribute_type type, int idx) -> const cgltf_accessor* {
        for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
            const cgltf_attribute& attr = prim.attributes[a];
            if (attr.type == type && (int)attr.index == idx) return attr.data;
        }
        return nullptr;
    };

    std::vector<Vertex3D> vertices;
    std::vector<uint32_t> indices;
    uint32_t base = 0;

    for (cgltf_size m = 0; m < data->meshes_count; ++m) {
        const cgltf_mesh& mesh = data->meshes[m];
        for (cgltf_size p = 0; p < mesh.primitives_count; ++p) {
            const cgltf_primitive& prim = mesh.primitives[p];
            if (prim.type != cgltf_primitive_type_triangles) continue; // só triângulos (v1)

            const cgltf_accessor* pos = FindAttr(prim, cgltf_attribute_type_position, 0);
            if (!pos) continue;
            const cgltf_accessor* nrm = FindAttr(prim, cgltf_attribute_type_normal, 0);
            const cgltf_accessor* uv  = FindAttr(prim, cgltf_attribute_type_texcoord, 0);
            const cgltf_accessor* jnt = FindAttr(prim, cgltf_attribute_type_joints, 0);
            const cgltf_accessor* wgt = FindAttr(prim, cgltf_attribute_type_weights, 0);

            cgltf_size count = pos->count;
            for (cgltf_size i = 0; i < count; ++i) {
                Vertex3D v{};
                cgltf_accessor_read_float(pos, i, &v.Position.x, 3);
                if (nrm) cgltf_accessor_read_float(nrm, i, &v.Normal.x, 3);
                else v.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
                if (uv) cgltf_accessor_read_float(uv, i, &v.TexCoord.x, 2);
                if (jnt) {
                    glm::vec4 ji(0.0f);
                    cgltf_accessor_read_float(jnt, i, &ji.x, 4); // convertido de ubyte/uint pra float
                    v.Joints = ji;
                }
                if (wgt) cgltf_accessor_read_float(wgt, i, &v.Weights.x, 4);
                vertices.push_back(v);
            }

            if (prim.indices) {
                for (cgltf_size i = 0; i < prim.indices->count; ++i)
                    indices.push_back(base + (uint32_t)cgltf_accessor_read_index(prim.indices, i));
            } else {
                for (cgltf_size i = 0; i < count; ++i) indices.push_back(base + (uint32_t)i);
            }
            base += (uint32_t)count;
        }
    }
    cgltf_free(data);

    if (vertices.empty()) {
        KZ_CORE_ERROR("O glTF '{0}' não contém malhas triangulares.", label);
        return CreateCube();
    }
    KZ_CORE_INFO("Malha glTF carregada: {0} ({1} vértices).", label, vertices.size());
    return CreateRef<Mesh>(vertices, indices);
}

Ref<Mesh> Mesh::LoadFromGLTF(const std::string& path) {
    cgltf_options options = {};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        KZ_CORE_ERROR("Falha ao carregar a malha glTF '{0}' (cgltf erro {1}).", path, (int)result);
        return CreateCube();
    }
    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success) {
        KZ_CORE_ERROR("Falha ao carregar os buffers do glTF '{0}' (cgltf erro {1}).", path, (int)result);
        cgltf_free(data);
        return CreateCube();
    }
    return BuildMeshFromGLTF(data, path);
}

Ref<Mesh> Mesh::LoadFromGLTFMemory(const void* data, std::size_t size) {
    cgltf_options options = {};
    cgltf_data* gltf = nullptr;
    cgltf_result result = cgltf_parse(&options, data, size, &gltf);
    if (result != cgltf_result_success) {
        KZ_CORE_ERROR("Falha ao carregar a malha glTF em memória (cgltf erro {0}).", (int)result);
        return CreateCube();
    }
    result = cgltf_load_buffers(&options, gltf, nullptr); // .glb: buffers já vêm do chunk BIN
    if (result != cgltf_result_success) {
        KZ_CORE_ERROR("Falha ao carregar os buffers do glTF em memória (cgltf erro {0}).", (int)result);
        cgltf_free(gltf);
        return CreateCube();
    }
    return BuildMeshFromGLTF(gltf, "memória");
}

Material Mesh::ExtractMaterialFromGLTF(const std::string& path) {
    Material mat;
    cgltf_options options = {};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) return mat;
    if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success) {
        cgltf_free(data);
        return mat;
    }

    if (data->materials_count > 0) {
        const cgltf_material& gm = data->materials[0];
        const cgltf_pbr_metallic_roughness& pbr = gm.pbr_metallic_roughness;
        mat.Albedo = { pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2] };
        mat.Metallic = pbr.metallic_factor;
        mat.Roughness = pbr.roughness_factor;
        mat.Emissive = { gm.emissive_factor[0], gm.emissive_factor[1], gm.emissive_factor[2] };
        mat.AlbedoMap = LoadGLTFTexture(pbr.base_color_texture);
        mat.MetallicRoughnessMap = LoadGLTFTexture(pbr.metallic_roughness_texture);
        mat.NormalMap = LoadGLTFTexture(gm.normal_texture);
        mat.EmissiveMap = LoadGLTFTexture(gm.emissive_texture);
    }
    cgltf_free(data);
    return mat;
}

} // namespace kizuri

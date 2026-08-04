#include "kizuri/renderer/Renderer3D.hpp"
#include "kizuri/core/Log.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

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

Ref<Mesh> Mesh::FromSource(const std::string& source) {
    if (source == "builtin:cube")    return CreateCube();
    if (source == "builtin:plane")   return CreatePlane();
    if (source == "builtin:sphere")  return CreateSphere();
    if (!source.empty()) {
        size_t dot = source.find_last_of('.');
        if (dot != std::string::npos) {
            std::string ext = source.substr(dot + 1);
            if (ext == "glb" || ext == "gltf") return LoadFromGLTF(source);
        }
        return LoadFromOBJ(source);
    }
    return CreateCube();
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
        KZ_CORE_ERROR("O glTF '{0}' não contém malhas triangulares.", path);
        return CreateCube();
    }
    KZ_CORE_INFO("Malha glTF carregada: {0} ({1} vértices).", path, vertices.size());
    return CreateRef<Mesh>(vertices, indices);
}

} // namespace kizuri

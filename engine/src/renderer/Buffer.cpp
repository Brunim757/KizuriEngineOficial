#include "kizuri/renderer/Buffer.hpp"
#include <glad/gl.h>

namespace kizuri {

static GLenum ToGLBaseType(ShaderDataType type) {
    switch (type) {
        case ShaderDataType::Float: case ShaderDataType::Float2:
        case ShaderDataType::Float3: case ShaderDataType::Float4:
        case ShaderDataType::Mat3: case ShaderDataType::Mat4:
            return GL_FLOAT;
        case ShaderDataType::Int: case ShaderDataType::Int2:
        case ShaderDataType::Int3: case ShaderDataType::Int4:
            return GL_UNSIGNED_INT;
        case ShaderDataType::Bool: return GL_UNSIGNED_BYTE;
        default: return 0;
    }
}


VertexBuffer::VertexBuffer(uint32_t size) {
    glGenBuffers(1, &m_RendererID);
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
}

VertexBuffer::VertexBuffer(float* vertices, uint32_t size) {
    glGenBuffers(1, &m_RendererID);
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
}

VertexBuffer::~VertexBuffer() { glDeleteBuffers(1, &m_RendererID); }
void VertexBuffer::Bind() const   { glBindBuffer(GL_ARRAY_BUFFER, m_RendererID); }
void VertexBuffer::Unbind() const { glBindBuffer(GL_ARRAY_BUFFER, 0); }
void VertexBuffer::SetData(const void* data, uint32_t size) {
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
}


IndexBuffer::IndexBuffer(uint32_t* indices, uint32_t count) : m_Count(count) {
    glGenBuffers(1, &m_RendererID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
}
IndexBuffer::~IndexBuffer() { glDeleteBuffers(1, &m_RendererID); }
void IndexBuffer::Bind() const   { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID); }
void IndexBuffer::Unbind() const { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); }


VertexArray::VertexArray() { glGenVertexArrays(1, &m_RendererID); }
VertexArray::~VertexArray() { glDeleteVertexArrays(1, &m_RendererID); }
void VertexArray::Bind() const   { glBindVertexArray(m_RendererID); }
void VertexArray::Unbind() const { glBindVertexArray(0); }

void VertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vb) {
    glBindVertexArray(m_RendererID);
    vb->Bind();

    const auto& layout = vb->GetLayout();
    for (const auto& element : layout) {
        glEnableVertexAttribArray(m_VertexBufferIndex);
        glVertexAttribPointer(
            m_VertexBufferIndex,
            (GLint)element.GetComponentCount(),
            ToGLBaseType(element.Type),
            element.Normalized ? GL_TRUE : GL_FALSE,
            (GLsizei)layout.GetStride(),
            (const void*)element.Offset
        );
        ++m_VertexBufferIndex;
    }
    m_VertexBuffers.push_back(vb);
}

void VertexArray::SetIndexBuffer(const Ref<IndexBuffer>& ib) {
    glBindVertexArray(m_RendererID);
    ib->Bind();
    m_IndexBuffer = ib;
}

} 

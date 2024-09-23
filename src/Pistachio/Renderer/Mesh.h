#pragma once
#include "Buffer.h"
#include "Shader.h"
#include "../Core/Error.h"
#include "BufferHandles.h"
namespace Pistachio {
	//probably have different Vertex structs
	struct PISTACHIO_API Vertex
	{
		
		struct {
			float x, y, z;
		} position = {0,0,0};
		struct {
			float x, y, z;
		} normal = {0,0,0};
		struct
		{
			float u, v;
		} TexCoord = {0,0};
		Vertex(float px, float py, float pz, float nx, float ny, float nz, float u, float v)
			: position{ px, py, pz },normal{nx, ny, nz}, TexCoord{u, v}
		{
		}
		Vertex() = default;
	};
	struct PISTACHIO_API Face
	{
		uint32_t i0, i1, i2;
	};
	class PISTACHIO_API Mesh {
	public:
		Mesh() = default;
		static Result<Mesh*> Create(const char* filepath, std::uint32_t index = 0);
		[[nodiscard]] Error CreateStack(const char* filepath, std::uint32_t index = 0);
		Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
		Mesh(std::vector<Vertex>&& vertices, std::vector<unsigned int>&& indices);
		~Mesh();
		static BufferLayout* GetLayout();
		static int GetLayoutSize() { return 3; }
		[[nodiscard]] RendererVBHandle GetVBHandle() const { return m_VertexBuffer; }
		[[nodiscard]] RendererIBHandle GetIBHandle() const { return m_IndexBuffer; }
		[[nodiscard]] const std::vector<Vertex>& GetVertices() const {return m_vertices;}
		[[nodiscard]] const std::vector<uint32_t>& GetIndices() const {return m_indices;}
	private:
		static BufferLayout layout[];
		std::vector<Vertex> m_vertices;
		std::vector<uint32_t> m_indices;
		RendererVBHandle m_VertexBuffer{};
		RendererIBHandle m_IndexBuffer{};
	};
}
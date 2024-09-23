#include "FormatsAndTypes.h"
#include "Pistachio/Core/Error.h"
#include "ptpch.h"
#include "Mesh.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/Logger.hpp"
#include "assimp/DefaultLogger.hpp"
#include "../Core/Log.h"
#include "Renderer.h"
#include <thread>
Pistachio::BufferLayout Pistachio::Mesh::layout[] = {
			{"POSITION", RHI::Format::FLOAT3, 0},
			{"NORMAL",   RHI::Format::FLOAT3, 12},
			{"UV",       RHI::Format::FLOAT2, 24}
};
void ProcessIndices(const aiMesh* pMesh, std::vector<unsigned int>& indices)
{
	for (unsigned int i = 0; i < pMesh->mNumFaces; i++)
	{
		const auto& face = pMesh->mFaces[i];
		for(unsigned int j = 0; j < face.mNumIndices; j++)
		indices.push_back(face.mIndices[j]);
	}
}
namespace Pistachio {
	Result<Mesh*> Mesh::Create(const char* filepath, const std::uint32_t index)
	{
		PT_PROFILE_FUNCTION();
		auto result = std::make_unique<Mesh>();
		if(auto err = result->CreateStack(filepath, index); !err.Successful())
			return ezr::err(std::move(err));
		return ezr::ok(result.release());
	}
	Error Mesh::CreateStack(const char* filepath, const std::uint32_t index)
	{
		PT_PROFILE_FUNCTION();
		m_vertices.clear();
		m_indices.clear();
		PT_CORE_INFO("Loading Mesh {0}", filepath);
		if(!Pistachio::Error::CheckFileExistence(filepath))
			return {ErrorType::NonExistentFile, std::string(__FUNCTION__) + ", filename: " + filepath};
		Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE, aiDefaultLogStream_STDOUT);
		Assimp::Importer imp;
		const aiScene* pScene = imp.ReadFile(filepath, aiProcess_GenNormals | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ConvertToLeftHanded);
		
		Assimp::DefaultLogger::kill();
		const aiMesh* p_meshes = pScene->mMeshes[index];
		m_vertices.reserve(p_meshes->mNumVertices);
		m_indices.reserve(p_meshes->mNumFaces * 3);
		std::thread worker(ProcessIndices, (p_meshes), std::ref(m_indices));
		for (unsigned int j = 0; j < p_meshes->mNumVertices; j++)
		{
			auto& vert = m_vertices.emplace_back();
			vert.position =	{p_meshes->mVertices[j].x, p_meshes->mVertices[j].y, p_meshes->mVertices[j].z};
			if(p_meshes->HasNormals())vert.normal={p_meshes->mNormals[j].x, p_meshes->mNormals[j].y, p_meshes->mNormals[j].z};
			if(p_meshes->HasTextureCoords(0))vert.TexCoord={p_meshes->mTextureCoords[0][j].x, p_meshes->mTextureCoords[0][j].y};
		}
		worker.join();
		m_VertexBuffer = Renderer::AllocateVertexBuffer(sizeof(Vertex) * p_meshes->mNumVertices, m_vertices.data());
		m_IndexBuffer = Renderer::AllocateIndexBuffer(sizeof(unsigned int) * p_meshes->mNumFaces * 3, m_indices.data());
		imp.FreeScene();
		return {ErrorType::Success, "NO ERROR"};
	}
	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
	{
		PT_PROFILE_FUNCTION();
		m_vertices = vertices;
		m_indices = indices;
		m_VertexBuffer = Renderer::AllocateVertexBuffer(static_cast<uint32_t>(sizeof(Vertex) * m_vertices.size()), m_vertices.data());
		m_IndexBuffer = Renderer::AllocateIndexBuffer(static_cast<uint32_t>(sizeof(unsigned int) * m_indices.size()), m_indices.data());
	}
	Mesh::Mesh(std::vector<Vertex>&& vertices, std::vector<unsigned int>&& indices)
	{
		PT_PROFILE_FUNCTION();
		m_vertices = vertices;
		m_indices = indices;
		m_VertexBuffer = Renderer::AllocateVertexBuffer(static_cast<uint32_t>(sizeof(Vertex) * m_vertices.size()), m_vertices.data());
		m_IndexBuffer = Renderer::AllocateIndexBuffer(static_cast<uint32_t>(sizeof(unsigned int) * m_indices.size()), m_indices.data());
	}
	Mesh::~Mesh()
	{
		Renderer::FreeVertexBuffer(m_VertexBuffer);
		Renderer::FreeIndexBuffer(m_IndexBuffer);
	}
	BufferLayout* Mesh::GetLayout()
	{
		return layout;
	}
}
#pragma once
#include "Pistachio/Core/Math.h"
#include "entt.hpp"
#include "Pistachio/Renderer/RenderTexture.h"
#include "Pistachio/Renderer/EditorCamera.h"
#include "Pistachio/Core/UUID.h"
#include "Pistachio/Renderer/Mesh.h"
#include "Pistachio/Allocators/AtlasAllocator.h"
#include "Pistachio/Renderer/Renderer.h"
#include "Pistachio/Event/SceneGraphEvent.h"
#include "Pistachio/Renderer/RenderGraph.h"
namespace physx {
	class PxScene;
}
namespace Pistachio {
	struct PISTACHIO_API alignas(16) PassConstants
	{
		DirectX::XMFLOAT4X4 View;
		DirectX::XMFLOAT4X4 InvView;
		DirectX::XMFLOAT4X4 Proj;
		DirectX::XMFLOAT4X4 InvProj;
		DirectX::XMFLOAT4X4 ViewProj;
		DirectX::XMFLOAT4X4 InvViewProj;
		DirectX::XMFLOAT2 RenderTargetSize;
		DirectX::XMFLOAT2 InvRenderTargetSize;
		float NearZ;
		float FarZ;
		float TotalTime;
		float DeltaTime;
		DirectX::XMFLOAT3 EyePosW;
		float scale;
		DirectX::XMFLOAT3 numClusters;
		float bias;
		uint32_t numRegularLights;
		uint32_t numShadowLights;
		uint32_t numDirectionalRegularLights;
		uint32_t numDirectionalShadowLights;
	};
	class Entity;
	struct PISTACHIO_API SceneDesc
	{
		Vector2 Resolution;
		uint32_t clusterX;
		uint32_t clusterY;
		uint32_t clusterZ;
		SceneDesc() : Resolution(1920,1080), clusterX(16), clusterY(9), clusterZ(24) {}
	};
	class PISTACHIO_API Scene {
	public:
		explicit Scene(SceneDesc desc  = SceneDesc());
		~Scene();
		Entity CreateEntity(const std::string& name = "");
		Entity DuplicateEntity(Entity e);
		Entity CreateEntityWithUUID(UUID ID, const std::string& name = "");
		void OnRuntimeStart();
		void OnRuntimeStop();
		void OnUpdateEditor(float delta, EditorCamera& camera);
		void OnUpdateRuntime(float delta);
		void OnViewportResize(unsigned int width, unsigned int height);
		void DestroyEntity(Entity entity);
		void DefferedDelete(Entity entity);
		void ReparentEntity(Entity entity, Entity new_parent);
		void SyncSkybox();
		template<typename _ComponentTy> auto GetAllComponents() { return m_Registry.view<_ComponentTy>(); }
		Entity GetRootEntity();
		Entity GetPrimaryCameraEntity();
		Entity GetEntityByUUID(UUID id);
		Entity GetEntityByName(const std::string& name);
		std::vector<Entity> GetAllEntitesWithName(const std::string& name);
		void UpdatePassConstants(const Matrix4& view, const SceneCamera& cam, const Vector3& camPos, float delta);
		void UpdatePassConstants(const EditorCamera& cam, float delta);
		const RenderTexture& GetFinalRender();
		//const RenderTexture& GetGBuffer() { return m_gBuffer; };
		//const RenderTexture& GetRenderedScene() { return m_finalRender; };

	private:
		Entity CreateRootEntity(UUID ID);
		void UpdateObjectCBs();
		void SortMeshComponents();
		void UpdateLightsBuffer();
		void FrustumCull(const Matrix4& view, const Matrix4& proj, float fovRad, float nearClip,float farClip,float aspect);
		DirectX::XMMATRIX GetTransfrom(Entity e);
		void UpdateTransforms(entt::entity e, const Matrix4& mat);
	private:
		friend class FrameComposer;
		friend class Entity;
		friend class SceneHierarchyPanel;
		friend class SceneSerializer;
		std::vector<entt::entity> meshesToDraw;
		std::vector<ShadowCastingLight> shadowLights;
		std::vector<RegularLight> regularLights;
		std::vector<entt::entity> deletionQueue;
		uint32_t numShadowDirLights = 0;
		uint32_t numRegularDirLights = 0;
		AtlasAllocator sm_allocator;
		entt::registry m_Registry;
		entt::entity root;
		physx::PxScene* m_PhysicsScene = nullptr;
		RGTextureHandle finalRenderTex{};
		uint32_t lightListSize = 0;
		uint32_t clustersDim[3]{};
		uint32_t sceneResolution[2]{};
		PassConstants passConstants{};
		StructuredBuffer shadowMarker;//replace with a push constant
		//consider fusing these two
		StructuredBuffer clusterAABB;
		StructuredBuffer activeClustersBuffer;
		StructuredBuffer sparseActiveClustersBuffer_lightIndices;
		StructuredBuffer lightList;
		StructuredBuffer lightGrid;
		ResourceSet passCBinfoGFX[RendererBase::numFramesInFlight];
		ResourceSet passCBinfoCMP[RendererBase::numFramesInFlight];
		ResourceSet passCBinfoVS_PS[RendererBase::numFramesInFlight];
		ResourceSet buildClusterInfo;
		ResourceSet activeClusterInfo;
		ResourceSet tightenListInfo;
		ResourceSet cullLightsInfo;
		ResourceSet sceneInfo;
		ResourceSet shadowSetInfo;
		ResourceSet backgroundInfo;
		DepthTexture shadowMapAtlas;
		DepthTexture zPrepass;
		RenderTexture finalRender;
		StructuredBuffer computeShaderMiscBuffer;
		ConstantBuffer passCB[RendererBase::numFramesInFlight];
		RenderGraph graph;
	};

}
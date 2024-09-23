#include "ptpch.h"
#include "MeshFactory.h"
namespace Pistachio
{
	Mesh* MeshFactory::CreatePlane()
	{
		std::vector<Vertex> positions = {
			{ /*positions*/-1, -1, 0, /*normals*/0, 0, -1, /*UV*/0, 1  },
			{ /*positions*/ 1, -1, 0, /*normals*/0, 0, -1, /*UV*/1, 1  },
			{ /*positions*/ 1,  1, 0, /*normals*/0, 0, -1, /*UV*/1, 0  },
			{ /*positions*/-1,  1, 0, /*normals*/0, 0, -1, /*UV*/0, 0  }
		};
		std::vector<unsigned int> indices = {
			2,1,0,0,3,2
		};
		return new Mesh(positions, indices);
	}
	Mesh* MeshFactory::CreateCube()
	{
		std::vector<Vertex> positions = {
			{ 1, -1, -1, 0, -1, -0, 1, 0.666667 },
			{ -1, -1, -1, 0, -1, -0, 1, 0.333333 },
			{ -1, -1, 1, 0, -1, -0, 0.666667, 0.333333 },
			{ -1, 1, 1, 0, 1, -0, 1, 0.666667 },
			{ -1, 1, -1, 0, 1, -0, 0.666667, 0.666667 },
			{ 1, 1, -1, 0, 1, -0, 0.666667, 1 },
			{ 1, 1, 1, 1, 0, -0, 0, 0.666667 },
			{ 1, 1, -1, 1, 0, -0, 0, 1 },
			{ 1, -1, -1, 1, 0, -0, 0.333333, 1 },
			{ 1, 1, -1, -0, 0, -1, 0.333333, 1 },
			{ -1, 1, -1, -0, 0, -1, 0.666667, 1 },
			{ -1, -1, -1, -0, 0, -1, 0.666667, 0.666667 },
			{ -1, -1, -1, -1, -0, 0, 0.333333, 0 },
			{ -1, 1, -1, -1, -0, 0, 0, 0 },
			{ -1, 1, 1, -1, -0, 0, 0, 0.333333 },
			{ 1, -1, 1, 0, 0, 1, 0.333333, 0.666667 },
			{ -1, -1, 1, 0, 0, 1, 0.333333, 0.333333 },
			{ -1, 1, 1, 0, 0, 1, 0, 0.333333 },
			{ 1, -1, 1, 0, -1, -0, 0.666667, 0.666667 },
			{ 1, 1, 1, 0, 1, -0, 1, 1 },
			{ 1, -1, 1, 1, 0, -0, 0.333333, 0.666667 },
			{ 1, -1, -1, -0, 0, -1, 0.333333, 0.666667 },
			{ -1, -1, 1, -1, -0, 0, 0.333333, 0.333333 },
			{ 1, 1, 1, 0, 0, 1, 0, 0.666667 },
		};
		std::vector<unsigned int> indices = {
			2, 1, 0, 5, 4, 3, 8, 7, 6, 11, 10, 9, 14, 13, 12, 17, 16, 15, 2, 0, 18, 5, 3, 19, 8, 6, 20, 11, 9, 21, 14, 12, 22, 17, 15, 23
		};
		return new Mesh(std::move(positions), std::move(indices));
	}
	Mesh* MeshFactory::CreateUVSphere(int slices, int stacks)
	{
		return nullptr;
	}
	Mesh* MeshFactory::CreateIcoSphere(int resolutions)
	{
		return nullptr;
	}
	Mesh* MeshFactory::CreateQuadSphere(int resolution)
	{
		return nullptr;
	}
	Mesh* MeshFactory::CreatePlatonic()
	{
		return nullptr;
	}
}
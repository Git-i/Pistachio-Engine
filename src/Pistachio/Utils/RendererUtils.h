#pragma once
#include "Pistachio/Core.h"
#include "Pistachio/Core/Log.h"
#include <cstdint>
#include <utility>
namespace Pistachio {
	
	namespace RendererUtils {
		
		uint32_t ConstantBufferElementSize(uint32_t byteSize);
		uint32_t SwapImageCount(std::pair<uint32_t, uint32_t> minMax, uint32_t preferred);
		
	}
}
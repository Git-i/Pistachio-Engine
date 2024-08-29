#include "RendererUtils.h"
#include <cstdint>
namespace Pistachio
{
    namespace RendererUtils
    {
        uint32_t ConstantBufferElementSize(uint32_t byteSize)
		{
			return (byteSize + 255) & ~255;
		}
        uint32_t SwapImageCount(std::pair<uint32_t, uint32_t> minMax, uint32_t preferred)
        {
			PT_CORE_ASSERT(preferred >= 1);
			uint32_t chosen;
			if (minMax.second == 0) minMax.second = UINT32_MAX;
			chosen = minMax.first;
			//we opt for at least double buffering if available
			uint32_t max_available = std::min(preferred, minMax.second);
			if(chosen < max_available) chosen = max_available;
			return chosen;
		}
    }
}
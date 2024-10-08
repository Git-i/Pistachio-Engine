#pragma once
#include "Pistachio/Core.h"
namespace Pistachio {
	class PISTACHIO_API RefCountedObject
	{
	public:
		virtual ~RefCountedObject() = default;
		int hold() const;
		int release() const;
		int count();
	private:
		mutable int m_count_ = 1;
	};
}

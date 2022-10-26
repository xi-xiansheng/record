#pragma once
#include "Common.h"
#include "ThreadCache.h"


static void* ConcurrentAlloc(size_t size)
{
	if (pTLSTreadCache == nullptr)
	{
		pTLSTreadCache = new TreadCache;
	}
	return pTLSTreadCache->Allocate(size);
}

static void ConcurrentFree(void* obj, size_t size)
{
	assert(pTLSTreadCache);
	pTLSTreadCache->Delaloocate(obj, size);
}


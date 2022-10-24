#pragma once
#include "Common.h"




class TreadCache
{
public:
	void* Allocate(size_t n);

	void Delaloocate(void* obj, size_t size);

	void* FetchFromCentralCache(size_t index, size_t alignSize);

private:
	FreeList _freeLists[FREELISTN];
};

static _declspec(thread) TreadCache* pTLSTreadCache = nullptr;
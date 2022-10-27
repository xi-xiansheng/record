#pragma once
#include "Common.h"




class ThreadCache
{
public:
	void* Allocate(size_t n);

	void Delaloocate(void* obj, size_t size);

	void* FetchFromCentralCache(size_t index, size_t alignSize);

	void ListTooLonng(FreeList& list, size_t size);

private:
	FreeList _freeLists[FREELISTN];
};

static _declspec(thread) ThreadCache* pTLSTreadCache = nullptr;
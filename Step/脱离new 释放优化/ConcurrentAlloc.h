#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"

static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size);

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(alignSize));
		span->_objSize = size;
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageId >> PAGE_SHIFT);
		return ptr;
	}
	if (pTLSTreadCache == nullptr)
	{
		static ObjectPool<ThreadCache> tcPool;
		//pTLSTreadCache = new ThreadCache;
		pTLSTreadCache = tcPool.New();
	}
	return pTLSTreadCache->Allocate(size);
}

static void ConcurrentFree(void* obj)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(obj);
	size_t size = span->_objSize;
	if (size > MAX_BYTES)
	{
		//Span* span = PageCache::GetInstance()->MapObjectToSpan(obj);
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
		return;
	}
	assert(pTLSTreadCache);
	pTLSTreadCache->Delaloocate(obj, size);
}


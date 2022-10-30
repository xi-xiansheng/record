#pragma once
#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"

class PageCache
{
	//µ¥ÀýÄ£Ê½
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	Span* NewSpan(size_t k);

	Span* MapObjectToSpan(void* obj);

	void ReleaseSpanToPageCache(Span* span);

	std::mutex _pageMtx;

private:
	SpanList _spanLists[NPAGES];
	ObjectPool<Span> _spanPool;
	//std::unordered_map<PageID, Span*> _idSpanMap;
	TCMalloc_PageMap2<32 - PAGE_SHIFT>  _idSpanMap;
	PageCache() {}
	PageCache(const PageCache&) = delete;

	static PageCache _sInst;
};
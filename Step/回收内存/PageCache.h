#pragma once
#include "Common.h"


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

	std::unordered_map<PageID, Span*> _idSpanMap;

	PageCache() {}
	PageCache(const PageCache&) = delete;

	static PageCache _sInst;
};
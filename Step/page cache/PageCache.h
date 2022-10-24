#pragma once
#include "Common.h"


class PageCache
{
	//����ģʽ
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	Span* NewSpan(size_t k);

	std::mutex _pageMtx;

private:
	SpanList _spanLists[NPAGES];
	
	PageCache() {}
	PageCache(const PageCache&) = delete;

	static PageCache _sInst;
};
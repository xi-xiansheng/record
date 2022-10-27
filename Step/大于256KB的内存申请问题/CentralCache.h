#pragma once
#include "Common.h"



//单例模式
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	size_t FenthRangObj(void*& start, void*& end, size_t n, size_t byte_size);

	Span* GetOneSpan(SpanList& list, size_t size);

	void ReleaseListToSpans(void* start, size_t size);

private:
	//构造函数私有
	CentralCache() {}

	CentralCache(const CentralCache&) = delete;		//防拷贝

private:
	SpanList _SpanLists[FREELISTN];
	static CentralCache _sInst;
};

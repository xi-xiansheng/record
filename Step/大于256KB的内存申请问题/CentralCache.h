#pragma once
#include "Common.h"



//����ģʽ
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
	//���캯��˽��
	CentralCache() {}

	CentralCache(const CentralCache&) = delete;		//������

private:
	SpanList _SpanLists[FREELISTN];
	static CentralCache _sInst;
};

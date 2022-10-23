#include "CentralCache.h"

CentralCache CentralCache::_sInst;

Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//
	return nullptr;
}

size_t CentralCache::FenthRangObj(void*& start, void*& end, size_t num, size_t size)
{
	//从中心缓存获取一定数量的对象给thread cache
	size_t index = SizeClass::Index(size);
	_SpanLists[index]._mtx.lock();
	//从Span中获取
	Span* span = GetOneSpan(_SpanLists[index], size);
	assert(span);
	assert(span->_freeList);
	start = end = span->_freeList;
	size_t actualNum = 1;
	size_t i = 0;
	while (i < num - 1 && Next(end))
	{
		end = Next(end);
		i++;
		actualNum++;
	}
	span->_freeList = Next(end);
	Next(end) = nullptr;
	_SpanLists[index]._mtx.unlock();
	//返回真实获取数量
	return actualNum;
}


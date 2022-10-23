#include "CentralCache.h"

CentralCache CentralCache::_sInst;

Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//
	return nullptr;
}

size_t CentralCache::FenthRangObj(void*& start, void*& end, size_t num, size_t size)
{
	//�����Ļ����ȡһ�������Ķ����thread cache
	size_t index = SizeClass::Index(size);
	_SpanLists[index]._mtx.lock();
	//��Span�л�ȡ
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
	//������ʵ��ȡ����
	return actualNum;
}


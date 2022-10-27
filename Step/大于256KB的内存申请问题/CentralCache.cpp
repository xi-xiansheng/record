#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_sInst;

Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList)
		{
			return it;
		}
		it = it->_next;
	}
	//�ߵ���һ����ζ��û�������Span

	//�Ȱ�central cache��Ͱ�������������������߳��ͷ��ڴ�����������������
	list._mtx.unlock();
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	PageCache::GetInstance()->_pageMtx.unlock();
	//ͨ��ҳ�ţ�������ʼ��ַ
	char* start = (char*)((span->_pageId) << PAGE_SHIFT);
	//�������ֽ���
	size_t bytes = size * span->_n;
	//�и�Span��β��
	//�� Span�и����������û���̻߳��õ����Span
	char* end = start + bytes;
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	while (start < end)
	{
		Next(tail) = start;
		tail = Next(tail);
		start += size;
	}
	Next(tail) = nullptr;
	/*span->_freeList = start;
	while (start < end)
	{
		Next(start) = start + size;
		start += size;
	}
	Next(end) = nullptr;*/
	//��Ͱ��ʱ����Ҫ����
	list._mtx.lock();
	list.PushFront(span);
	return span;
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

void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	//
	size_t index = SizeClass::Index(size);
	//ͨ����ַ����ʼҳ�ż���ӳ����һ��Span
	_SpanLists[index]._mtx.lock();
	while (start)
	{
		void* next = Next(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		span->_usecount--;
		if (span->_usecount == 0)
		{
			//ǰ��ҳ�ϲ�
			_SpanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;
			_SpanLists[index]._mtx.unlock();
			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();
			_SpanLists[index]._mtx.lock();
		}
		Next(start) = span->_freeList;
		span->_freeList = start;
		start = next;
	}
	_SpanLists[index]._mtx.unlock();
}
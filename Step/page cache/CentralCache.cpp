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
	//走到这一步意味着没有满足的Span

	//桶解锁
	list._mtx.unlock();

	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	PageCache::GetInstance()->_pageMtx.unlock();
	//通过页号，计算起始地址
	char* start = (char*)((span->_pageId) << PAGE_SHIFT);
	//计算总字节数
	size_t bytes = size * span->_n;
	//切割Span—尾插
	//对 Span切割无需加锁，没有线程会拿到这个Span
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
	//挂桶的时候需要加锁
	list._mtx.lock();
	list.PushFront(span);
	return span;
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


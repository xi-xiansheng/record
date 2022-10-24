#include "PageCache.h"

Span* PageCache::NewSpan(size_t k)
{
	//获取K页Span
	assert(k > 0 && k < NPAGES);
	if (!_spanLists[k].Empty())
	{
		//不为空
		return _spanLists[k].PopFront();
	}
	for (size_t i = k + 1; i < NPAGES; i++)
	{
		if (!_spanLists[i].Empty())
		{
			//不为空，则切割
			Span* kspan = new Span;
			Span* nspan = _spanLists[i].PopFront();
			

			kspan->_n = k;
			kspan->_pageId = nspan->_pageId + k;

			nspan->_n -= k;
			nspan->_pageId += k;

			_spanLists[nspan->_n].PushFront(nspan);
			return kspan;
		}
	}
	//走到这一步，说明无满足的Span，需要向系统申请
	Span* bigSpan = new Span;
	void* address = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PageID)address >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;
	_spanLists[NPAGES - 1].PushFront(bigSpan);
	return NewSpan(k);
}


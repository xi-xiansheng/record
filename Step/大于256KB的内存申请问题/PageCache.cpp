#include "PageCache.h"

PageCache PageCache::_sInst;


Span* PageCache::NewSpan(size_t k)
{
	//��ȡKҳSpan
	assert(k > 0);

	if (k > NPAGES - 1)
	{
		//����NPAGES,ֱ����ϵͳ����
		void* ptr = SystemAlloc(k);
		Span* span = new Span;
		span->_pageId = (PageID)(ptr) >> PAGE_SHIFT;
		span->_n = k;
		//����ӳ���ϵ
		_idSpanMap[span->_pageId] = span;
		return span;
	}

	if (!_spanLists[k].Empty())
	{
		//��Ϊ��
		return _spanLists[k].PopFront();
	}
	for (size_t i = k + 1; i < NPAGES; i++)
	{
		if (!_spanLists[i].Empty())
		{
			//��Ϊ�գ����и�
			Span* kspan = new Span;
			Span* nspan = _spanLists[i].PopFront();
			
			kspan->_n = k;
			kspan->_pageId = nspan->_pageId + k;

			nspan->_n -= k;
			nspan->_pageId += k;

			//�洢��βҳ����span��ӳ�䣬�������
			_idSpanMap[nspan->_pageId] = nspan;
			_idSpanMap[nspan->_pageId + nspan->_n - 1] = nspan;

			//����id��span��ӳ��
			for (PageID i = 0; i < kspan->_n; i++)
			{
				_idSpanMap[i] = kspan;
			}

			_spanLists[nspan->_n].PushFront(nspan);
			return kspan;
		}
	}
	//�ߵ���һ����˵���������Span����Ҫ��ϵͳ����
	Span* bigSpan = new Span;
	void* address = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PageID)address >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;
	_spanLists[NPAGES - 1].PushFront(bigSpan);
	return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID id = (PageID)obj >> PAGE_SHIFT;
	auto ret = _idSpanMap.find(id);
	if (ret == _idSpanMap.end())
	{
		assert(false);
		return nullptr;
	}
	return ret->second;
}

void PageCache::ReleaseSpanToPageCache(Span* span)
{
	if (span->_n > NPAGES - 1)
	{
		//����128ҳ��ֱ��ϵͳ�ͷ�
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		delete span;
		return;
	}
	//��ǰ��ҳ�ϲ��������ڴ���Ƭ����
	while (true)
	{
		PageID prev = span->_pageId - 1;
		auto it = _idSpanMap.find(prev);
		if (it != _idSpanMap.end())
		{

			Span* prevSpan = it->second;
			if (prevSpan->_usecount)
			{
				break;
			}
			if (prevSpan->_isUse)
			{
				break;
			}
			if (prevSpan->_n + span->_n > NPAGES - 1)
			{
				break;
			}
			//�ϲ�
			span->_pageId = prevSpan->_pageId;
			span->_n += prevSpan->_n;

			_spanLists[prevSpan->_n].Erase(prevSpan);
			delete prevSpan;
		}
		else
		{
			break;
		}
	}

	while (true)
	{
		PageID next = span->_pageId + span->_n;
		auto it = _idSpanMap.find(next);
		if (it != _idSpanMap.end())
		{
			Span* nextSpan = it->second;
			if (nextSpan->_usecount)
			{
				break;
			}
			if (nextSpan->_isUse)
			{
				break;
			}
			if (nextSpan->_n + span->_n > NPAGES - 1)
			{
				break;
			}
			//�ϲ�
			span->_n += nextSpan->_n;

			_spanLists[nextSpan->_n].Erase(nextSpan);
			delete nextSpan;
		}
		else
		{
			break;
		}
	}
	_spanLists->PushFront(span);
	span->_isUse = false;
	_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId + span->_n - 1] = span;
}


#pragma once
#include <iostream>
#include <vector>
#include <time.h>
#include <assert.h>
#include <thread>
#include <mutex>


using std::cout;
using std::endl;


static void*& Next(void* obj)
{
	return *(void**)obj;
}

#ifdef _WIN64
typedef unsigned long long PageID;
#elif _WIN32
typedef size_t PageID;
#endif

static const size_t FREELISTN = 208;
static const size_t MAX_BYTES = 256 * 1024;

struct Span
{
	//管理一个跨度的大块内存
	PageID _pageId = 0;			//大块内存起始页页号
	size_t _n = 0;				//页数


	Span* _prev = nullptr;		//
	Span* _next = nullptr;		//

	void* _freeList = nullptr;	//切好大小的链表
	size_t _usecount = 0;		//使用数量
};

class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_prev = _head;
		_head->_next = _head;
	}

	void Insert(Span* pos, Span* newSpan)
	{
		assert(pos);
		assert(newSpan);
		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}

	void Erase(Span* pos)
	{
		assert(pos);
		assert(pos != _head);
		Span* prev = pos->_prev;
		prev->_next = pos->_next;
		pos->_next->_prev = prev;
	}

private:
	Span* _head;
public:
	std::mutex _mtx;
};

class FreeList
{
public:
	void Push(void* obj)
	{
		//头插
		assert(obj);
		*(void**)obj = _freeList;
		_freeList = obj;
	}

	void* Pop()
	{
		//头删
		assert(_freeList);
		void* ret = _freeList;
		_freeList = Next(_freeList);
		return ret;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}

	size_t& MaxSize()
	{
		return _maxSize;
	}

	void PushRange(void* start, void* end)
	{
		Next(end) = _freeList;
		_freeList = start;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
};

class SizeClass
{
	//计算哈希桶映射对齐规则
public:
	// 整体控制在最多11%左右的内碎片浪费
	// [1,128] 8byte对齐 freelist[0,16)
	// [128+1,1024] 16byte对齐 freelist[16,72)
	// [1024+1,8*1024] 128byte对齐 freelist[72,128)
	// [8*1024+1,64*1024] 1024byte对齐 freelist[128,184)
	// [64*1024+1,256*1024] 8*1024byte对齐 freelist[184,208)
	static inline size_t RoundUp(size_t size)
	{
		//计算某一字节数对齐后的字节数
		if (size <= 128)
		{
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{
			return _RoundUp(size, 16);
		}
		else if (size <= 1024 * 8)
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 1024 * 64)
		{
			return _RoundUp(size, 1024);
		}
		else if (size <= 1024 * 256)
		{
			return _RoundUp(size, 8 * 1024);
		}
		else
		{
			assert(false);
			return -1;
		}
	}
	
	static inline size_t Index(size_t bytes)
	{
		//计算映射自由链表的哪一个桶
		static size_t group[4] = { 16,56,56,56 };
		assert(bytes <= MAX_BYTES);
		if (bytes <= 128)
		{
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024)
		{
			return _Index(bytes, 4) + group[0];
		}
		else if (bytes <= 1024 * 8)
		{
			return _Index(bytes, 7) + group[0] + group[1];
		}
		else if (bytes <= 1024 * 64)
		{
			return _Index(bytes, 10) + group[0] + group[1] + group[2];
		}
		else if (bytes <= 1024 * 256)
		{
			return _Index(bytes, 13) + group[0] + group[1] + group[2] + group[3];
		}
		else
		{
			assert(false);
			return -1;
		}
	}

	static inline size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		size_t num = MAX_BYTES / size;
		//设置上限
		if (num < 2)	num = 2;
		if (num > 512)	num = 512;

		return num;
	}

private:
	static inline size_t _RoundUp(size_t size, size_t alignNum)
	{
		//return ((size + alignNum - 1) & ~(alignNum - 1));
		return (size / alignNum + (size % alignNum != 0)) * alignNum;
	}

	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		//bytes—字节数 align_shift—对齐数的左移数
		//int alignNum = std::pow(2, align_shift);
		//return bytes / alignNum + (bytes % alignNum != 0) - 1;
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
};


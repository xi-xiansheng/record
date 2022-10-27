#pragma once
#include <iostream>
#include <vector>
#include <time.h>
#include <assert.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#include "ObjectPool.h"

using std::cout;
using std::endl;

#ifdef _WIN32
#include <windows.h>
#else
// linux下brk mmap等
#endif


// 直接去堆上按页申请空间
static inline void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage * (1 << 13), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif
	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

// 直接去堆上释放空间
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}

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
static const size_t NPAGES = 129;
static const size_t PAGE_SHIFT = 13;



struct Span
{
	//管理一个跨度的大块内存
	PageID _pageId = 0;			//大块内存起始页页号
	size_t _n = 0;				//页数


	Span* _prev = nullptr;		//
	Span* _next = nullptr;		//

	void* _freeList = nullptr;	//切好大小的链表
	size_t _objSize = 0;		//记录对象地址大小

	size_t _usecount = 0;		//使用数量
	bool _isUse = false;
};

class SpanList
{
public:
	SpanList()
	{
		//_head = new Span;
		_head = _spanPool.New();
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

	Span* Begin()
	{
		return _head->_next;
	}

	Span* End()
	{
		return _head;
	}

	void PushFront(Span* span)
	{
		/*span->_next = _head->_next;
		_head->_next->_prev = span;
		span->_prev = _head;
		_head->_next = span;*/
		Insert(Begin(), span);
	}

	Span* PopFront()
	{
		assert(_head);
		Span* front = _head->_next;
		Erase(front);
		return front;
	}

	bool Empty()
	{
		return _head == nullptr;
	}



private:
	Span* _head;
	static ObjectPool<Span> _spanPool;
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

	void PushRange(void* start, void* end, size_t size)
	{
		Next(end) = _freeList;
		_freeList = start;
		_size += size;
	}

	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n >=_size);
		start = end = _freeList;
		for (size_t i = 0; i < n - 1; i++)
		{
			end = Next(end);
		}
		_freeList = Next(end);
		Next(end) = nullptr;
		_size -= n;
	}

	size_t Size()
	{
		return _size;
	}



private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size = 0;
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
			return _RoundUp(size, 1 << PAGE_SHIFT);
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
		//一次从中心缓存获取多少个
		assert(size > 0);
		size_t num = MAX_BYTES / size;
		//设置上限
		if (num < 2)	num = 2;
		if (num > 512)	num = 512;

		return num;
	}

	static inline size_t NumMovePage(size_t size)
	{
		//计算一次需要多少页
		size_t num = NumMoveSize(size);
		size_t bytes = num * size;			//总字节数

		size_t  pages = bytes >> PAGE_SHIFT;			//所需页数
		if (pages == 0)	pages = 1;
		return pages;
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


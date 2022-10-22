#pragma once
#include <iostream>
#include <vector>
#include <time.h>
#include <assert.h>
#include <thread>

using std::cout;
using std::endl;


static void*& Next(void* obj)
{
	return *(void**)obj;
}

class FreeList
{
public:
	void Push(void* obj)
	{
		//ͷ��
		assert(obj);
		*(void**)obj = _freeList;
		_freeList = obj;
	}

	void* Pop()
	{
		//ͷɾ
		assert(_freeList);
		void* ret = _freeList;
		_freeList = Next(_freeList);
		return ret;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}

private:
	void* _freeList = nullptr;
};

class SizeClass
{
	//�����ϣͰӳ��������
public:
	// ������������11%���ҵ�����Ƭ�˷�
	// [1,128] 8byte���� freelist[0,16)
	// [128+1,1024] 16byte���� freelist[16,72)
	// [1024+1,8*1024] 128byte���� freelist[72,128)
	// [8*1024+1,64*1024] 1024byte���� freelist[128,184)
	// [64*1024+1,256*1024] 8*1024byte���� freelist[184,208)
	static inline size_t RoundUp(size_t size)
	{
		//����ĳһ�ֽ����������ֽ���
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
		//����ӳ�������������һ��Ͱ
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

private:
	static inline size_t _RoundUp(size_t size, size_t alignNum)
	{
		//return ((size + alignNum - 1) & ~(alignNum - 1));
		return (size / alignNum + (size % alignNum != 0)) * alignNum;
	}

	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		//bytes���ֽ��� align_shift����������������
		int alignNum = std::pow(2, align_shift);
		return bytes / alignNum + (bytes % alignNum != 0) - 1;
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
};


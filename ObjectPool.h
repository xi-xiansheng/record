#pragma once
#include <iostream>
#include <vector>
#include <time.h>

#ifdef _WIN32
	#include <windows.h>
#else
	// linux下brk mmap等
#endif

using std::cout;
using std::endl;

//定长内存池
 
inline static void* SystemAlloc(size_t kpage)
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

//template<size_t N>
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		if (_freeList)
		{
			//优先利用删除的地址
			obj = (T*)_freeList;
			_freeList = *(void**)_freeList;
			return obj;
		}
		else
		{
			if (_remianBytes < sizeof(T))
			{
				_remianBytes = 128 * 1024;
				//剩余内存大小不足以开辟一个对象则重新申请内存
				//_memory = (char*)malloc(128 * 1024);
				_memory = (char*)SystemAlloc(_remianBytes >> 13);
				if (_memory == nullptr)
				{
					//申请失败
					throw std::bad_alloc();
				}
			}
			obj = (T*)_memory;
			//考虑指针类型
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(T) : sizeof(void*);
			_remianBytes -= objSize;
			_memory += objSize;
		}
		//定位new，显式调用T的构造函数
		new(obj)T;
		return obj;
	}


	void Delete(T* obj)
	{
		//显式调用T的析构函数
		obj->~T();
		//头插—需要考虑32/64位的情况
		*(void**)obj = _freeList;
		_freeList = (void*)obj;
	}

private:
	char* _memory = nullptr;	//指向大块内存的指针
	void* _freeList = nullptr;	//还回来的自由链表的头指针
	size_t _remianBytes = 0;	//大块内存剩余字节数
};


struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;
	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{}
};

void TestObjectPool()
{
	// 申请释放的轮次
	const size_t Rounds = 3;
	// 每轮申请释放多少次
	const size_t N = 100000;
	size_t begin1 = clock();
	std::vector<TreeNode*> v1;
	v1.reserve(N);
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v1.push_back(new TreeNode);
		}
		for (int i = 0; i < N; ++i)
		{
			delete v1[i];
		}
		v1.clear();
	}
	size_t end1 = clock();
	ObjectPool<TreeNode> TNPool;
	size_t begin2 = clock();
	std::vector<TreeNode*> v2;
	v2.reserve(N);
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v2.push_back(TNPool.New());
		}
		for (int i = 0; i < 100000; ++i)
		{
			TNPool.Delete(v2[i]);
		}
		v2.clear();
	}
	size_t end2 = clock();
	cout << "new cost time:" << end1 - begin1 << endl;
	cout << "object pool cost time:" << end2 - begin2 << endl;
}
#pragma once
#include <iostream>
#include <vector>
#include <time.h>

#ifdef _WIN32
	#include <windows.h>
#else
	// linux��brk mmap��
#endif

using std::cout;
using std::endl;

//�����ڴ��
 
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage * (1 << 13), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux��brk mmap��
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
			//��������ɾ���ĵ�ַ
			obj = (T*)_freeList;
			_freeList = *(void**)_freeList;
			return obj;
		}
		else
		{
			if (_remianBytes < sizeof(T))
			{
				_remianBytes = 128 * 1024;
				//ʣ���ڴ��С�����Կ���һ�����������������ڴ�
				//_memory = (char*)malloc(128 * 1024);
				_memory = (char*)SystemAlloc(_remianBytes >> 13);
				if (_memory == nullptr)
				{
					//����ʧ��
					throw std::bad_alloc();
				}
			}
			obj = (T*)_memory;
			//����ָ������
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(T) : sizeof(void*);
			_remianBytes -= objSize;
			_memory += objSize;
		}
		//��λnew����ʽ����T�Ĺ��캯��
		new(obj)T;
		return obj;
	}


	void Delete(T* obj)
	{
		//��ʽ����T����������
		obj->~T();
		//ͷ�塪��Ҫ����32/64λ�����
		*(void**)obj = _freeList;
		_freeList = (void*)obj;
	}

private:
	char* _memory = nullptr;	//ָ�����ڴ��ָ��
	void* _freeList = nullptr;	//�����������������ͷָ��
	size_t _remianBytes = 0;	//����ڴ�ʣ���ֽ���
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
	// �����ͷŵ��ִ�
	const size_t Rounds = 3;
	// ÿ�������ͷŶ��ٴ�
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
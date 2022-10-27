#include "ThreadCache.h"
#include "CentralCache.h"
#include "PageCache.h"


void* TreadCache::Allocate(size_t size)
{
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);
	if (_freeLists[index].Empty())
	{
		//�����Ļ����ȡ����
		return FetchFromCentralCache(index, alignSize);
	}
	return _freeLists[index].Pop();
}

void TreadCache::Delaloocate(void* obj, size_t size)
{
	assert(size <= MAX_BYTES);
	assert(obj);
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(obj);

	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
	{
		ListTooLonng(_freeLists[index], size);
	}
}

void* TreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	//�����Ļ����ȡ����
	//����ʼ���������㷨
	//1.����һ��Ҫ̫��
	//2.���������size��С�����󣬻�����������
	//3.sizeԽ��batchNumԽС
	//4.sizeԽС��batchNumԽ��
	size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (batchNum == _freeLists[index].MaxSize())	_freeLists[index].MaxSize() += 1;

	void* start = nullptr;
	void* end = nullptr;
	size_t actualNum = CentralCache::GetInstance()->FenthRangObj(start, end, batchNum, size);
	assert(actualNum >= 1);
	if (actualNum == 1)
	{
		assert(start == end);
		return start;
	}
	else
		_freeLists[index].PushRange(Next(start), end, actualNum - 1);
	return start;
}

void TreadCache::ListTooLonng(FreeList& list, size_t size)
{
	//�ͷŶ���ʱ����������������ڴ浽���Ļ���
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.MaxSize());
	CentralCache::GetInstance()->ReleaseListToSpans(start,size);
}
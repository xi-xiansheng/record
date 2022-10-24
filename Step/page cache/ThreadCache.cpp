#include "ThreadCache.h"
#include "CentralCache.h"



void* TreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
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
}

void* TreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	//�����Ļ����ȡ����
	//����ʼ���������㷨
	//1.����һ��Ҫ̫��
	//2.���������size��С�����󣬻�����������
	//3.sizeԽ��batchNumԽС
	//4.sizeԽС��batchNumԽ��
	size_t batchNum = std::min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
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
	_freeLists[index].PushRange(Next(start), end);
	return start;
}

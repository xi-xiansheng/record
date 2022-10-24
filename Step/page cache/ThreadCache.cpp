#include "ThreadCache.h"
#include "CentralCache.h"



void* TreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);
	if (_freeLists[index].Empty())
	{
		//从中心缓存获取对象
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
	//从中心缓存获取对象
	//慢开始反馈调节算法
	//1.不会一次要太多
	//2.如果不断有size大小的需求，会逐渐增至上限
	//3.size越大，batchNum越小
	//4.size越小，batchNum越大
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

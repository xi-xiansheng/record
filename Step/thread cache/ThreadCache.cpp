#include "ThreadCache.h"



static const size_t MAX_BYTES = 256 * 1024;

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

void* TreadCache::FetchFromCentralCache(size_t index, size_t alignSize)
{
	//从中心缓存获取对象
	return nullptr;
}
#include "ObjectPool.h"
#include "ConcurrentAlloc.h"

void AllocTest1()
{
	for (int i = 0; i < 5; i++)
	{
		void* ptr = ConcurrentAlloc(5);
	}
}
void AllocTest2()
{
	for (int i = 0; i < 5; i++)
	{
		void* ptr = ConcurrentAlloc(10);
	}
}

void TLSTest()
{
	std::thread t1(AllocTest1);
	std::thread t2(AllocTest2);
	t1.join();
	t2.join();
}

//int main()
//{
//	//TestObjectPool();
//	//TLSTest();
//	return 0;
//}
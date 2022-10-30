[TOC]

------

# 项目介绍

&emsp;&emsp;本项目是实现一个**高并发内存池**，其原型是**google**的一个开源项目**tcmalloc(Thread-Caching Malloc)**，即**线程缓存的malloc**，实现了高效的多线程内存管理，用于替代系统的内存分配相关的函数（`malloc()`、`free()`）。鉴于tcmalloc的代码量和复杂度，本项目将tcmalloc最核心的框架简化后拿出来，模拟实现出一个自己的高并发内存池，目的就是学习tcamlloc的精华。
&emsp;&emsp;**TCMalloc** 作为 **Google** 开源的内存分配器，在不少项目中都有使用，例如在 Golang 中就使用了类似的算法进行内存分配。它具有现代化内存分配器的基本特征：对抗内存碎片、在多核处理器能够 scale。据称，它的内存分配速度是 glibc2.3 中实现的 malloc 的数倍。
&emsp;&emsp;涉及知识：C/C++、数据结构（链表、哈希桶）、操作系统内存管理、单例模式、多线程、互斥锁等等方面的知识。

&emsp;&emsp;👉[tcmalloc源代码](https://gitee.com/mirrors/tcmalloc)

# 内存池

**1. 池化技术**

&emsp;&emsp;池化技术 — 就是程序先向系统申请过量的资源，然后自己管理，以备不时之需。简单点来说，就是提前保存大量的资源，以备不时之需。之所以要申请过量的资源，是因为每次申请该资源都涉及到很多系统调用，都有较大的开销，不如提前申请好了，这样使用时就会变得非常快捷，大大提高程序运行效率。倘若你的程序需要很多类似的工作线程或者需要频繁的申请释放小块内存，如果没有在这方面进行优化，那很有可能这部分代码将会成为影响你整个程序性能的瓶颈。
&emsp;&emsp;在计算机中，有很多使用“池”这种技术的地方，除了内存池，还有连接池、线程池、对象池等。以服务器上的线程池为例，它的主要思想是：先启动若干数量的线程，让它们处于睡眠状态，当接收到客户端的请求时，唤醒池中某个睡眠的线程，让它来处理客户端的请求，当处理完这个请求，线程又进入睡眠状态。对象池就是提前创建很多对象，将用过的对象保存起来，等下一次需要这种对象的时候，再拿出来重复使用。

<hr size=1> 

**2. 内存池**
&emsp;&emsp;内存池是指程序预先从操作系统申请一块足够大内存，此后，当程序中需要申请内存的时候，不是直接向操作系统申请，而是直接从内存池中获取并将其标志置为已使用；同理，释放内存时，不是真正地调用free或是delete的过程，而是把内存放回内存池的过程，同时要把标志位置为空闲。当程序退出(或者特定时间)时，内存池才将之前申请的内存真正释放。

-   使用内存池的好处：1.减少了内存碎片的产生。2. 提高了内存的使用效率。
-   缺点：就是很有可能会造成内存的浪费，原因也很明显，开始分配了一大块内存，不是全部都用得到的。

<hr size=1> 

**3. 内存池主要解决的问题**
&emsp;&emsp;内存池主要解决的当然是效率的问题，其次如果作为系统的内存分配器的角度，还需要解决一下内存碎片的问题。

**tips：**内存碎片分为内部碎片和外部碎片

-   **内部碎片就是已经被分配出去（能明确指出属于哪个进程）却不能被利用的内存空间；**
    比如：结构体内存对齐的需求，导致分配出去的空间中一些内存无法被利用。
-   **外部碎片指的是还没有被分配出去（不属于任何进程），但无法分配给申请内存空间的新进程的内存空闲区域。**
    外部碎片是出于任何已分配区域或页面外部的空闲存储块。这些存储块的总和可以满足当前申请的长度要求，但是由于它们的地址不连续或其他原因，使得系统无法满足当前申请。如下图所示：

![image-20221018200108022](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210190027819.png)

<hr size=1> 

**4. malloc**

&emsp;&emsp;C/C++中我们要动态申请内存都是通过malloc去申请内存，但是我们要知道，实际我们不是直接去堆获取内存的，而malloc就是一个内存池。`malloc()` 相当于向操作系统“批发”了一块较大的内存空间，然后“零售”给程序用。当全部“售完”或程序有大量的内存需求时，再根据实际需求向操作系统“进货”。
&emsp;&emsp;malloc的实现方式有很多种，一般不同编译器平台用的都是不同的。比如windows的vs系列用的微软自己写的一套，Linux gcc用的glibc中的ptmalloc。  至于其底层实现，大家可以参考这篇文章具体了解👉[malloc的底层实现（ptmalloc）](https://blog.csdn.net/z_ryan/article/details/79950737)

![image-20221018203320093](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210190027820.png)

# 开胃菜 — 定长内存池的实现

&emsp;&emsp;我们知道申请内存使用的是`malloc()`，`malloc()`作为标准库提供的函数，什么场景下都可以用，同时意味着什么场景下都不会有很高的性能。下面我们就先来设计一个定长内存池做个开胃菜，先熟悉一下简单内存池是如何控制的，其次它会作为我们后面内存池的一个基础组件。
&emsp;&emsp;定长内存池就是针对固定大小内存块的申请和释放的内存池，因此有如下特点：1. 性能达到机制2. 不考虑内存碎片等问题

<hr size=1> 

**设计原理**

-   申请：从操作系统申请一块大块内存，在用户层进行管理。需要申请内存的时候先从自由链表当中找，如果自由链表当中没有就切割大块内存；
-   释放：不是真正地调用`free`或`delete`的过程，而是把内存放回内存池的过程。由于其释放顺序的不确定性，该部分内存采用**自由链表**挂起，不可以直接还给操作系统，因为释放一块内存需要从申请的虚拟地址还起，并且一次需要还完。

<hr size=1> 

**定长实现**

&emsp;&emsp;定长内存池就要做到“定长”，我们可以使用非类型模板参数，使得在该内存池中申请到的对象的大小都是N。如下：

```c++
template<size_t N>
class ObjectPool {};
```

&emsp;&emsp;不过上述方式太麻烦，我们根据传入的对象类型的大小来实现“定长”：

```c++
template<class T>
class ObjectPool {};
```

<hr size=1> 

**成员变量**

-   对于我们申请到的内存来说，其必然是连续的，因此我们只需要一个指针来保存即可，并且`char *`类型的指针更加方便加减操作。但还需要用一个变量来记录这块内存的长度。

![image-20221018212326990](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210190027822.png)

-   对于还回来的内存，我们采用**自由链表**，每个节点保存下一个地址即可，其最后必然是空指针，当走到空指针即可判断是否有闲置内存。

    ![image-20221018214625469](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210190027823.png)因此，定长内存池当中包含三个成员变量：

    ```c++
    private:
    	char* _memory = nullptr;
    	void* _freeList = nullptr;
    	size_t _remianBytes = 0;
    ```

<hr size=1> 

**管理释放**
&emsp;&emsp;对于还回来的定长内存，我们可以用自由链表将其链接起来。我们无需为其专门定义链式结构，我们可以让内存块的前4个字节（32位平台）或8个字节（64位平台）作为指针，存储后面内存块的起始地址即可。
&emsp;&emsp;因此在向自由链表插入被释放的内存块时，先让该内存块的前4个字节或8个字节存储自由链表中第一个内存块的地址，然后再让`_freeList`指向该内存块即可，即链表的**头插**操作。
&emsp;&emsp;不过大家可能会有疑问，如果指针是`char *`这种内存块不足4/8个字节该怎么办？这里无需考虑，我们将在**管理申请**时考虑该问题。我们只需考虑当前是32/64位平台的问题。通常我们采取`if`判断，不过我们还有更加简单的方式：将`obj`看成二级指针，存储`_freeList`后再修改头结点，同时在释放对象时，我们应该显示调用该对象的析构函数清理该对象。

```c++
void Delete(T* obj)
{
    //显式调用T的析构函数
    obj->~T();
    //头插—需要考虑32/64位的情况
    *(void**)obj = _freeList;
    _freeList = (void*)obj;
}
```

<hr size=1> 

**管理申请**

&emsp;&emsp;当我们申请内存时，内存池应该优先重复利用还回来的内存块。

![image-20221018234549882](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210190027824.png)

&emsp;&emsp;如果自由链表为空，就当考虑在大块内存中切出定长的内存块进行返回，并更新`_memory`指针的指向，以及`_remainBytes`的值。

![image-20221018235211049](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210190027825.png)

&emsp;&emsp;这时，我们就应当考虑前文的问题：如果指针是`char *`这种内存块不足4/8个字节该怎么办？因此我们必须保证切出来的内存至少能够存储下一个地址。所以当对象所占内存小于当前所在平台指针的大小时，需要按指针的大小进行内存块的切分，并且显式调用其构造函数。
<hr size=1> 

&emsp;&emsp;综上，我们有如下代码：

```c++
T* New()
{
    T* obj = nullptr;
    if (_freeList)
    {
        //优先利用删除的地址
        obj = _freeList;
        _freeList = *(void**)_freeList;
    }
    else
    {
        if (_remianBytes < sizeof(T))
        {
            //剩余内存大小不足以开辟一个对象则重新申请内存
            _memory = (char*)malloc(128 * 1024);
            if (_memory == nullptr)
            {
                //申请失败
                throw std::bad_alloc();
            }
            _remianBytes = 128 * 1024;
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
```

<hr size=1> 

**补充：**

&emsp;&emsp;事实上，我们可以跳过`malloc`直接向堆申请页为单位的大块内存，可以参考以下文章：1. 👉[VirtualAlloc](https://baike.baidu.com/item/VirtualAlloc/1606859?fr=aladdin) 2. 👉[brk() 和mmap()](https://www.cnblogs.com/vinozly/p/5489138.html)

```c++
#ifdef _WIN32
	#include <windows.h>
#else
	// linux下brk mmap等
#endif

inline static void* SystemAlloc(size_t kpage)
{
    #ifdef _WIN32
    void* ptr = VirtualAlloc(0, kpage*(1 << 13),MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    #else
    // linux下brk mmap等
    #endif
    if (ptr == nullptr)
    throw std::bad_alloc();
    return ptr;
}
```

**完整代码：**

```c++
#pragma once
#include <iostream>

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
			obj = _freeList;
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
```

<hr size=1> 

**测试**

&emsp;&emsp;使用下列代码进行测试：

```c++
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
```

测试结果如下：

![image-20221019002250543](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210190027826.png)

# 整体框架设计
&emsp;&emsp;在C/C++申请内存的时，其底层调用的均为`malloc`，在多线程环境下，必然存在激烈的锁竞争问题。`malloc`本身其实已经很优秀了，但是在并发场景下可能会因为频繁的加锁和解锁导致效率有所降低，而该项目的原型**tcmalloc**实现的就是一种在多线程高并发场景下更胜一筹的内存池。
&emsp;&emsp;一般优秀的内存池会需要考虑到**效率问题**和**内存碎片**的问题，但对于**高并发内存池**来说，我们还需要考虑在多线程环境下的**锁竞争问题**。

<hr>

**concurrent memory pool**主要由以下3个部分构成：


-   **thread cache**：线程缓存是每个线程独有的，用于小于256KB的内存的分配，线程从这里申请内存不需要加锁，每个线程独享一个cache。
-   **central cache**：中心缓存是所有线程所共享，thread cache是**按需从central cache中获取**的对象。central cache合适的时机回收thread cache中的对象，避免一个线程占用了太多的内存，达到内存分配在多个线程中**更均衡的按需调度**的目的。central cache是存在竞争的，所以从这里取内存对象是需要加锁，首先这里用的是桶锁，其次只有thread cache的没有内存对象时才会找central cache，所以这里竞争不会很激烈。
-   **page cache**：页缓存是在central cache缓存上面的一层缓存，存储的内存是**以页为单位存储及分配**的，central cache没有内存对象时，从page cache分配出一定数量的page，并切割成定长大小的小块内存，分配给central cache。**当一个span的几个跨度页的对象都回收以后，page cache会回收central cache满足条件的span对象，并且合并相邻的页，组成更大的页，缓解内存碎片的问题。** 

![image-20221021110230352](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210211103126.png)

# Thread Cache

## Thread Cache 整体设计

&emsp;&emsp;前文我们说到，自由链表可以管理好切好的小块内存，那么我们可以使用多个自由链表管理256KB的需求。因此**thread cache**实际上一个**哈希桶**结构，每个桶中存放的都是一个自由链表。
&emsp;&emsp;**thread cache**被设计支持小于等于256KB内存的申请，倘若每种字节都使用一个自由链表来管理，即需要256 * 1024 = 262144个自由链表，这显然是消耗大量内存且得不偿失的，因此我们需要平衡牺牲。我们在这选择一些值进行**向上对齐**，比如申请1 ~ 8个字节，我们直接给它8个字节，申请9 ~ 16个字节，统一给16个字节，并以此类推。结构如下图所示：

![image-20221021131408542](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210211314178.png)

&emsp;&emsp;因此，当某一线程申请内存时，我们先计算得出其**对齐字节数**，并计算出**哈希映射**的下标，再去对应下标的**自由链表**下申请内存。若内存不够，那就需要向下一层的**central cache**获取。同时我们注意到，这样做不可避免的产生了一些内部碎片，不过并无较大影响。

<hr size=1> 

&emsp;&emsp;基于上述定长内存池，我们将自由链表进行封装：

```c++
void*& Next(void* obj)
{
	return *(void**)obj;
}

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
	void* pop()
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
private:
	void* _freeList = nullptr;
};
```

## thread cache 哈希桶映射对齐规则

&emsp;&emsp;有了上述理解，接下来我们就要考虑内存对齐数了。从定长内存池我们知道，考虑到32/64位平台，我们的对齐数最少是8位。但是，如果**256KB**的内存均以**8Byte**来对齐，那么共有**256 * 1024  / 8 = 32768**个桶，这显然是不合理的。由此我们考虑**分段对齐**，即不同范围的字节数选择不同的对齐数进行对齐。于是我们有以下规则：

|              字节数              |    对齐数    |
| :------------------------------: | :----------: |
|        **[0 + 1， 128]**         |    **8**     |
|       **[128 + 1， 1024]**       |    **16**    |
|    **[1024 + 1， 8 * 1024]**     |   **128**    |
|  **[8 * 1024 + 1， 64 * 1024]**  |   **1024**   |
| **[64 * 1024 + 1， 256 * 1024]** | **8 * 1024** |

这样看似更加浪费了。不过我们来计算一下浪费率：$$\huge{浪费率 = \frac{浪费的字节数}{对齐后总字节数}} $$
按照此公式，我们计算如下：
区间**[128 + 1, 1024]**的最大浪费率：$$\large{15\div144 \thickapprox 10.42\%} $$
区间**[1024 + 1, 8 * 1024]**的最大浪费率：$$\large{127\div1152 \thickapprox11.02\%} $$
区间**[8 * 1024 + 1, 64 * 1024]**的最大浪费率：$$\large{1023\div9216 \thickapprox11.10\%} $$
可以看到，基本上稳定在 **11%** 左右。而关于区间**[0 + 1, 128]**，即便对齐数为2，也会有50%的浪费，故不考虑此区间的浪费率。

<hr > 

**SizeClass类**

&emsp;&emsp;有了上述哈希桶映射对齐规则，为方便**管理对齐和映射等关系**，我们可以考虑设计类将其封装。

```c++
class SizeClass
{
public:
	//获取向上对齐后的总字节数
	static inline size_t RoundUp(size_t bytes);
	//获取所映射哈希桶的下标
	static inline size_t Index(size_t bytes);
};
```

**对齐数计算**


&emsp;&emsp;接下来我们要做的就是**计算某一字节数对齐后的总字节数**。比如 9 Byte对齐后总字节数为 16 Byte。我们可以先判断该字节数属于哪一个区间，然后再通过调用一个子函数进行进一步处理。

```c++
static inline size_t RoundUp(size_t bytes)
{
    //计算某一字节数对齐后的字节数
    if (bytes <= 128)
    {
        return _RoundUp(bytes, 8);
    }
    else if (bytes <= 1024)
    {
        return _RoundUp(bytes, 16);
    }
    else if (bytes <= 1024 * 8)
    {
        return _RoundUp(size, 128);
    }
    else if (bytes <= 1024 * 64)
    {
        return _RoundUp(bytes, 1024);
    }
    else if (bytes <= 1024 * 256)
    {
        return _RoundUp(bytes, 8 * 1024);
    }
    else
    {
        assert(false);
        return -1;
    }
}
```

接下来就是子函数的编写了，我们最先想到的莫过于下面的写法：

```c++
static inline size_t _RoundUp(size_t size, size_t alignNum)
{
    //size——对齐前字节数 alignNum——对齐数
    return (size / alignNum + (size % alignNum != 0)) * alignNum;
}
```

来看看高手的写法：通过**位运算**的方式来进行计算，下列代码可能不太容易理解，但要知道计算机执行位运算的速度是快于乘除法的。

```c++
static inline size_t _RoundUp(size_t size, size_t alignNum)
{
    return ((size + alignNum - 1) & ~(alignNum - 1));
}
```

我们以13为例，对齐数为8。其它同理，可自行验证。

则13 + 8 - 1 = 20

![image-20221021163621809](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210211639842.png)



 ~（8 - 1）= ~ 7

![image-20221021163702015](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210211639843.png)

20 & ~7 = 16。其二进制位表示如下：

![image-20221021163239836](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210211639844.png)

<hr> 

**桶映射计算**


&emsp;&emsp;接下来我们要做的就是**计算映射自由链表的哪一个桶**。在获取某一字节数对应的哈希桶下标时，也先判断该字节数属于哪一个区间，然后再调用一个子函数进行进一步处理。同时，由于每个区间的对齐数和长度不同，我们可以定义`group`静态数组，让子函数计算其在对应区间的下标即可。

```c++
static inline size_t Index(size_t bytes)
{
    //计算映射自由链表的哪一个桶
    static size_t group[4] = { 16,56,56,56 };
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
```

我们通常想到的子函数大致如下：

```c++
static inline size_t _Index(size_t bytes, size_t align_shift)
{
    //bytes—字节数 align_shift—对齐数的左移数
    int alignNum = std::pow(2, align_shift);
    return bytes / alignNum + (bytes % alignNum != 0) - 1;
}
```

来看看高手的写法：

```c
static inline size_t _Index(size_t bytes, size_t align_shift)
{
    return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
}
```

同样，我们取`bytes = 13, align_shift = 3(即对齐数为2的align_shift次方)`来理解：

`1 << align_shift`即`1 << 3`等于8，`bytes + 8 - 1 = 20`

![image-20221021163621809](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210302158631.png)

`20 >> 3 = 3`

![image-20221021184422134](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210211844346.png)

由于桶下标自0开始，故减去1，`3 - 1 = 2`，则`13bytes`对应的即对齐数为8的区间的第2号桶。其他区间同理。

<hr size=1> 

**Thread Cache 类**

&emsp;&emsp;基于上述理解，我们对**Thread Cache 类**有了初步设计模型。

```c++
static const size_t FREELISTN = 208;

class TreadCache
{
public:
	void* Allocate(size_t n);
	void Delaloocate(void* obj, size_t size);
	void* FetchFromCentralCache(size_t index, size_t alignSize);
private:
	FreeList _freeLists[FREELISTN];
};
```

&emsp;&emsp;此时我们能写出申请内存对象的函数：

```c++
static const size_t MAX_BYTES = 256 * 1024;
void* TreadCache::Allocate(size_t size)
{
    //申请内存对象
	assert(size <= MAX_BYTES);
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);
	if (_freeLists[index].Empty())
	{
		//从中心缓存获取对象
		return FetchFromCentralCache(index, alignSize);
	}
	return _freeLists[index].pop();
}
```

<hr size=1> 

## thread cache LTS无锁访问
&emsp;&emsp;如果一个变量是全局的，那么所有线程访问的是同一份，某一个线程对其修改会影响其他所有线程。如果我们需要一个变量在每个线程中都能访问，并且值在每个线程中互不影响，这就是TLS。详情请戳👉[线程本地存储（Thread Local Storage）](https://www.cnblogs.com/liu6666/p/12729014.html)
&emsp;&emsp;由于我们所设计的**thread cache**是每个线程独享的，那我们就不能将这个thread cache创建为全局的，因为全局变量是所有线程共享的，这样就不可避免的需要锁来控制。
&emsp;&emsp;要实现每个线程无锁的访问属于自己的thread cache，我们需要用到线程局部存储TLS(Thread Local Storage)，这样就保持了数据的线程独立性。

```c++
static _declspec(thread) TreadCache* pTLSTreadCache = nullptr;
```

&emsp;&emsp;当线程调用相关申请内存的接口时才会创建自己的thread cache，故申请内存函数如下：

```c++
static void* ConcurrentAlloc(size_t size)
{
	if (pTLSTreadCache == nullptr)
	{
		pTLSTreadCache = new TreadCache;
	}
	return pTLSTreadCache->Allocate(size);
}
```

# Central Cache

## Central Cache整体设计

&emsp;&emsp;thread cache**按需从central cache中获取**内存，由于thread cache是每个线程独享的，故其无需加锁。但central cache是线程共享的，存在竞争的，所以从这里取内存对象是需要加锁，首先这里用的是桶锁。
&emsp;&emsp;central cache也是一个哈希桶结构，他的哈希桶的映射关系跟thread cache是一样的。不同的是他的每个哈希桶位置挂是SpanList链表结构，不过每个映射桶下面的span中的大内存块被按映射关系切成了一个个小内存块对象挂在span的自由链表中。  如下图所示：

![image-20221022152119705](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210221534815.png)

## Central Cache结构设计

**页号**&emsp;

&emsp;&emsp;span是以页为单位的内存。页的大小一般是4K或者8K，以8K（即$2^{13}$）为例。在32位下，进程地址空间就可以被分成 $2^{32}\div2^{13}=2^{13}$个页；在64位下，可以被分成 $2^{64}\div2^{13}=2^{54}$个页。页号本质与地址是一样的，它们都是一个编号。由于其在不同平台下取值范围的不同，我们需要条件编译来选择值类型。其中：
1.   **WIN32/_WIN32** 可以用来判断是否 Windows 系统
2.   **_WIN64** 用来判断编译环境是 x86（32位） 还是 x64（64位）
值得注意的是：
-   在 Win32 配置下，_WIN32 有定义，_WIN64 没有定义。
-   在 x64 配置下，_WIN32 和_WIN64 都有定义。

因此，我们的条件编译需要先判断是否为64位平台
```c++
#ifdef _WIN64
typedef unsigned long long PageID;
#elif _WIN32
typedef size_t PageID;
#endif
```

<hr> </hr>

**Span设计**

&emsp;&emsp;为方便之后page cache进行前后页的合并，且由于span是以页为单位的内存，我们需要知道这块内存具体在哪一个位置，以及所管理的页数。并且thread cache的申请还回是不确定的，我们需要知道Span里的释放的内存是否全被还回来了，故添加`_usecount`变量。此外，每个span管理的大块内存，都会被按映射关系切成了一个个小内存块挂到当前span的自由链表中。

```c++
struct Span
{
	//管理一个跨度的大块内存
	PageID _pageId = 0;			//大块内存起始页页号
	size_t _n = 0;				//页数


	Span* _prev = nullptr;		//双链表结构
	Span* _next = nullptr;		//双链表结构

	void* _freeList = nullptr;	//切好大小的链表
	size_t _usecount = 0;		//使用数量
};
```

<hr> 

**带头双向循环链表**


&emsp;&emsp;由于central cache的桶里面存储的都是一个双链表结构，故我们可以将其进行封装。同时由于我们的锁是针对每个桶单独加锁，故成员添加锁。

```c++
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
	std::mutex _mtx;
};
```

## Central Cache核心实现

**单例模式**

&emsp;&emsp;由于多个线程共享一个centralcache，所以central cache可以直接设置成单例。这里设置成饿汉，懒汉都可以。因此我们需要将central cache的构造函数设置为私有，并防拷贝。

```c++
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

private:
    //构造函数私有
	CentralCache() {}

	CentralCache(const CentralCache&) = delete;		//防拷贝

private:
	SpanList _SpanLists[FREELISTN];
	static CentralCache _sInst;
};
```

<hr> 

**慢开始反馈调节算法**

&emsp;&emsp;我们在实现`vector`等容器时，我们对其扩容通常是按照2倍进行扩容，这样是很高效的。同理，我们使用thread cache的`FetchFromCentralCache`时也不该只获取指定数目的内存块。但是每次获取多少合适呢？鉴于此问题，我们使用**慢开始反馈调节算法**。

```c++
class SizeClass
{
	static inline size_t NumMoveSize(size_t size)
	{
		assert(size > 0);
		size_t num = MAX_BYTES / size;
		//设置上限
		if (num < 2)	num = 2;
		if (num > 512)	num = 512;

		return num;
	}
};
```

&emsp;&emsp;该函数其实是设置了上限值。*对象越小，计算出的上限越高* ；对象越大，计算出的上限越低。但是，即便申请对象很小，一次给512个也是很多的。因此我们考虑根据其申请次数不断变化每次给的数目，但不会超出上限值。故我们考虑在`FreeList`中增加变量。

```c++
class FreeList
{
public:
	size_t& MaxSize()
	{
		return _maxSize;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
};
```

&emsp;&emsp;于是有以下方法计算每次申请数目。

```c++
//慢开始反馈调节算法
//1.不会一次要太多
//2.如果不断有size大小的需求，会逐渐增至上限
//3.size越大，batchNum越小
//4.size越小，batchNum越大
size_t batchNum = std::min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
if (batchNum == _freeLists[index].MaxSize())	_freeLists[index].MaxSize() += 1;
```

<hr> 

**`FetchFromCentralCache`**


&emsp;&emsp;有了上述理解，我们可以完善`FetchFromCentralCache(size_t index, size_t size)`了。我们来看看`Span`的结构，其内保存着一个切好的小块内存的自由链表，也就是说我们未必能获取到指定数目的内存块，因此我们需要返回实际获取的数目。同时为方便后续插入等操作，我们采用前后两个指针来获取这一段内存。

```c++
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
    //actualNum：真实获取的数目
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
```

&emsp;&emsp;接下来我们要补充的就是`PushRange(void* start, void* end)`函数了。

```c++
void PushRange(void* start, void* end)
{
    Next(end) = _freeList;
    _freeList = start;
}
```

&emsp;&emsp;由于获取的大小指定，所以我们应当去指定下标的桶下找寻非空Span并获取链表的头和尾。

```c++
size_t CentralCache::FenthRangObj(void*& start, void*& end, size_t num, size_t size)
{
	//从中心缓存获取一定数量的对象给thread cache
	size_t index = SizeClass::Index(size);
	_SpanLists[index]._mtx.lock();
	//从Span中获取
	Span* span = GetOneSpan(_SpanLists[index], size);
	assert(span);
	assert(span->_freeList);
	start = end = span->_freeList;
	size_t actualNum = 1;
	size_t i = 0;
	while (i < num - 1 && Next(end))
	{
		end = Next(end);
		i++;
		actualNum++;
	}
	span->_freeList = Next(end);
	Next(end) = nullptr;
	_SpanLists[index]._mtx.unlock();
	//返回真实获取数量
	return actualNum;
}
```

# Page Cache
## Page Cache整体设计

**Page Cache结构**

&emsp;&emsp;Page Cache与Central Cache结构类似，也是一个哈希桶结构，并且每个桶下面挂的也是一个Span链表。
&emsp;&emsp;**不同**之处在于：central cache哈希桶，与hread cache按照相同的对齐关系映射，且其SpanList中挂的span中的内存都被按映射关系切好链接成小块内存的自由链表。而Page Cache 中的SpanList则是按下标桶号映射的，即第i号桶中挂的Span都保存着 i 页内存。  如下图所示：

![image-20221024171523667](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210241715580.png)
<hr>  </hr> 

**申请**

1.   当**central cache**向**page cache**申请内存时，page cache先检查对应位置有没有Span，如果没有则向更大页寻找一个Span，如果找到则切分成两个Span。例如：申请的是4页的Span，而4页page后面没有挂Span，则向后寻找更大的非空Span。假设在10页page位置找到一个span，则将10页page Span分裂为一个4页page Span和一个6页page Span。
2.   如果找到`_spanList[NPAGES]`都没有合适的Span，则使用系统接口申请128页page Span挂在自由链表中，再重复1中的过程。  

<hr>  </hr> 

**Page Cache实现**
&emsp;&emsp;由于page cache在整个进程中只能存在一个，故我们需要将其设置为单例模式。除此之外，基于central cache 是桶锁的原因，换言之，多个同可能同时申请同一大小的Span，则page cache就不能是桶锁。

```c++
static const size_t NPAGES = 129;
class PageCache
{
	//单例模式
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}
	std::mutex _pageMtx;
private:
	SpanList _sapnLists[NPAGES];

	PageCache() {}
	PageCache(const PageCache&) = delete;

	static PageCache _sInst;
};
```

`NPAGES = 129` ?这是为了方便下标的对应关系，设置成128也是可以的。此外，由于线程申请单个对象最大是256KB，而128页可以被切成4个256KB的对象。并且也可以根据具体的需求在page cache中挂更大的span。

## Page Cache获取Span

&emsp;&emsp;首先给`SpanList`添加几个成员函数方便后续操作。

```c++
class SpanList
{
public:
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
		Insert(Begin(), span);
	}
};
```

在我们已有的`Span* CentralCache::GetOneSpan(SpanList& list, size_t size)`中，我们的思路无非如下：
1.   从给定的`SpanList`中找询非空Span并返回

2.   若无非空`Span`，则从`Page Cache`中申请一块非空`Span`后，将其所管理的内存块切割成指定大小的自由链表，并将该Span挂在`SpanList`中，然后返回该`Span`。


&emsp;&emsp;如何找到一个span所管理的内存块呢？首先需要计算出该span的起始地址——通过span保存的起始页号乘以一页的大小即可得到这个span的起始地址，然后用保存的页数乘一页的大小就可以得到这个span所管理的内存块的大小；
&emsp;&emsp;明确了这块内存的起始位置和大小后，我们就可以根据所需对象的大小，每次从大块内存切出一块固定大小的内存块尾插到span的自由链表中。

```c++
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList)
		{
			return it;
		}
		it = it->_next;
	}
	//走到这一步意味着没有满足的Span，则从Page Cache中申请一块非空Span
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	//通过页号，计算起始地址
	char* start = (char*)((span->_pageId) << PAGE_SHIFT);
	//计算总字节数
	size_t bytes = size * span->_n;
	//切割Span—尾插
	char* end = start + bytes;
    span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	while (start < end)
	{
		Next(tail) = start;
		tail = Next(tail);
		start += size;
	}
	Next(tail) = nullptr;
    //头插span
	list.PushFront(span);
	return span;
}
```

<hr>  </hr> 

**从Page Cache获取Span**

&emsp;&emsp;从Page Cache中申请一块非空Span，由于两个哈希桶其下标映射的方式不一样，所以我们应当根据对象的大小先计算出其所需的页数再去获取Span：

```c++
class SizeClass
{
public:
	static inline size_t NumMovePage(size_t size)
	{
		//计算一次需要多少页
		size_t num = NumMoveSize(size);
		size_t bytes = num * size;			//总字节数

		size_t  pages = bytes >> PAGE_SHIFT;			//所需页数
		if (pages == 0)	pages = 1;
		return pages;
	}
}
```

&emsp;&emsp;有了上述理解，我们很容易梳理以下申请思路：假设我们需要获取K页的Span

1.   先去第K号桶申请，若有Span，则弹出头结点
2.   若page cache的第K号桶中没有span，我们就不断向后找，直至任意一个桶中有一个n页span，我们就将其切割成一个k页的span和一个n-k页的span，然后将切出来k页的span返回给central cache，再将n-k页的span挂到page cache的第n-k号桶即可。
3.   若后续的桶都没有Span，只需向系统申请`NPAGES`或`NPAGES - 1`页的内存，并设置其所存页数和起始页号，将其挂在最后一个桶上，再执行第二步即可。

```c++
Span* PageCache::NewSpan(size_t k)
{
	//获取K页Span
	assert(k > 0 && k < NPAGES);
	if (!_spanLists[k].Empty())
	{
		//不为空
		return _spanLists[k].PopFront();
	}
	for (size_t i = k + 1; i < NPAGES; i++)
	{
		if (!_spanLists[i].Empty())
		{
			//不为空，则切割
			Span* kspan = new Span;
			Span* nspan = _spanLists[i].PopFront();
			
			kspan->_n = k;
			kspan->_pageId = nspan->_pageId;

			nspan->_n -= k;
			nspan->_pageId += k;

			_spanLists[nspan->_n].PushFront(nspan);
			return kspan;
		}
	}
	//走到这一步，说明无满足的Span，需要向系统申请
	Span* bigSpan = new Span;
	void* address = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PageID)address >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;
	_spanLists[NPAGES - 1].PushFront(bigSpan);
	return NewSpan(k);
}
```

<hr>  </hr> 

**页号**

&emsp;&emsp;由于向堆申请内存后得到的是这块内存的起始地址（`void*`），此时我们需要将该地址转换为页号。可能大家看到这里有几个疑问：

1.   为什么需要转化为页号，不直接存地址？一来这是为了方便后续的回收操作，二来与保存的页数联系起来。

2.   为什么按照上述方式转化，不会存在问题吗？由于我们向堆申请内存时都是**按页**进行申请的，因此我们直接将该地址除以一页的大小即可得到对应的页号。大家或许认为，假若起始地址后13为不为0，那么不就出现问题了吗？事实上并不会。

```c++
bigSpan->_pageId = (PageID)address >> PAGE_SHIFT;
```

<hr>  </hr> 

**锁问题**

&emsp;&emsp;但是我们说过，`Page Cache`是存在锁的，那么我们如果加锁的话，存在几个问题：

1.   由于该函数是递归的，自己也会被锁阻塞住。一来我们可以使用递归锁，二来可以分离子函数来解决上述问题。

2.   我们从central cache向page cache申请的时候，central cache是桶锁的，那我们该不该将其解锁呢？假若不解锁，其他线程无法访问该桶；若释放锁，其他线程即便访问该桶，该桶也不会被申请内存出去（因为本身就是因为该桶内存不足而走到这一步），但如果是释放内存，则也会把该桶堵住。因此最好释放锁。

为了逻辑统一，我们在Central Cache中进行加锁。（Page Cache的锁可以通过添加成员函数或改为public加锁）

```c++
 Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
 {
 	//
 	Span* it = list.Begin();
 	while (it != list.End())
 	{
 		if (it->_freeList)
 		{
 			return it;
 		}
 		it = it->_next;
 	}
 	//走到这一步意味着没有满足的Span
 
 	//桶解锁
 	list._mtx.unlock();
 
 	PageCache::GetInstance()->_pageMtx.lock();
 	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
 	PageCache::GetInstance()->_pageMtx.unlock();
 	//通过页号，计算起始地址
 	char* start = (char*)((span->_pageId) << PAGE_SHIFT);
 	//计算总字节数
 	size_t bytes = size * span->_n;
 	//切割Span—尾插
 	//对 Span切割无需加锁，没有线程会拿到这个Span
 	char* end = start + bytes;
 	span->_freeList = start;
 	start += size;
 	void* tail = span->_freeList;
 	while (start < end)
 	{
 		Next(tail) = start;
 		tail = Next(tail);
 		start += size;
 	}
 	Next(tail) = nullptr;
 	//挂桶的时候需要加锁
 	list._mtx.lock();
 	list.PushFront(span);
 	return span;
 }
```

# Thread Cache回收内存

&emsp;&emsp;当某线程释放其申请的内存时，通过调用`Delaloocate()`函数将其释放给thread cache，thread cache根据其还回的大小插入到对应的哈希桶的自由链表当中。倘若某一线程还回的内存太多，其某一自由链表过长却不还给central cache，其本质也是一种浪费，导致其他线程内存吃紧。
&emsp;&emsp;基于此，我们考虑如果thread cache某个桶当中自由链表的长度超过它一次批量向central cache申请的对象个数，我们就把该自由链表当中的一些对象还给central cache。还多少合适呢？如果一次全还完，倘若下一次还申请相同大小的内存，那么其效率必然会有所降低的。因此我们考虑，一次还回当前最大批量申请数。
&emsp;&emsp;同时，为方便查看当前自由链表的长度，我们在自由链表中添加成员变量`_size`

```c++
void TreadCache::Delaloocate(void* obj, size_t size)
{
	assert(size <= MAX_BYTES);
	assert(obj);
	size_t index = SizeClass::Index(size);
    //插入到对应哈希桶当中
	_freeLists[index].Push(obj);
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
	{
        //当自由链表长度大于一次批量申请的对象个数时就开始还一段list给central cache
		ListTooLonng(_freeLists[index], size);
	}
}
```

&emsp;&emsp;还给central cache中对应的span，即取出自由链表中某一数目的内存块，还给central cache中的span即可。

```c++
void TreadCache::ListTooLonng(FreeList& list, size_t size)
{
	//释放对象时，链表过长，回收内存到中心缓存
	void* start = nullptr;
	void* end = nullptr;
    //取出链表的某一长度
	list.PopRange(start, end, list.MaxSize());
	CentralCache::GetInstance()->ReleaseListToSpans(start,size);
}
```

&emsp;&emsp;同时，我们完善一下自由链表

```c++
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
```

# Central Cache回收内存

&emsp;&emsp;到这里我们有个问题，我们给定了待回收的内存地址，该如何知道回收到哪一个Span呢？事实上，根据前文我们可以知道每个地址都属于某一页。因为每一页的内存，前32 - 13位都是相同的。而一个span管理者多页，也就是说，多个页可能映射同一个span，为解决此问题，我们可以建立页号和span之间的映射：

```c++
class PageCache
{
public:
    //获取从对象到span的映射
    Span* MapObjectToSpan(void* obj);
private:
	std::unordered_map<PageID, Span*> _idSpanMap;
};
```

```c++
Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID id = (PageID)obj >> PAGE_SHIFT;
	auto ret = _idSpanMap.find(id);
	if (ret == _idSpanMap.end())
	{
		assert(false);
		return nullptr;
	}
	return ret->second;
}
```

&emsp;&emsp;至此，我们的`NewSpan`需更新一下，保存映射关系：

```c++
Span* PageCache::NewSpan(size_t k)
{
	//获取K页Span
	assert(k > 0 && k < NPAGES);
	if (!_spanLists[k].Empty())
	{
		//不为空
		return _spanLists[k].PopFront();
	}
	for (size_t i = k + 1; i < NPAGES; i++)
	{
		if (!_spanLists[i].Empty())
		{
			//不为空，则切割
			Span* kspan = new Span;
			Span* nspan = _spanLists[i].PopFront();
			

			kspan->_n = k;
			kspan->_pageId = nspan->_pageId + k;

			nspan->_n -= k;
			nspan->_pageId += k;

			//存储首尾页号与span的映射，方便回收
			_idSpanMap[nspan->_pageId] = nspan;
			_idSpanMap[nspan->_pageId + nspan->_n - 1] = nspan;

			//建立id和span的映射
			for (PageID i = 0; i < kspan->_n; i++)
			{
				_idSpanMap[i] = kspan;
			}

			_spanLists[nspan->_n].PushFront(nspan);
			return kspan;
		}
	}
	//走到这一步，说明无满足的Span，需要向系统申请
	Span* bigSpan = new Span;
	void* address = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PageID)address >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;
	_spanLists[NPAGES - 1].PushFront(bigSpan);
	return NewSpan(k);
}
```

&emsp;&emsp;到此，我们的`ReleaseListToSpans`就能了：当central cache中某个span的_useCount减到0时，即该span分配出去的对象全部都还回来了，那么此时就可以将这个span再进一步还给page cache。

```c++
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	//
	size_t index = SizeClass::Index(size);
    _SpanLists[index]._mtx.lock();
	//通过地址和起始页号计算映射哪一个Span
	while (start)
	{
		void* next = Next(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
        //更新被分配给thread cache的计数
		span->_usecount--;
		if (span->_usecount == 0)
		{
			//前后页合并
			_SpanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;
			_SpanLists[index]._mtx.unlock();
			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
            PageCache::GetInstance()->_pageMtx.unlock();
			_SpanLists[index]._mtx.lock();
		}
		Next(start) = span->_freeList;
		span->_freeList = start;
		start = next;
	}
    _SpanLists[index]._mtx.unlock();
}
```

# Page Cache回收内存

&emsp;&emsp;当有Span从central cache还回给page cache时，我们需考虑内存碎片的问题，因此我们应当将前后页进行合并。
&emsp;&emsp;不过当我们通过页号找到其对应的span时，这个span此时可能挂在page cache，也可能挂在central cache。而我们只能合并挂在page cache的span，因此我们需要通过某一方式判断span到底是在central cache还是在page cache。
&emsp;&emsp;鉴于此，我们可以在span结构中再增加一个`_isUse`成员，用于标记这个span是否正在被使用。

```c++
struct Span
{
	//管理一个跨度的大块内存
	PageID _pageId = 0;			//大块内存起始页页号
	size_t _n = 0;				//页数

	Span* _prev = nullptr;		//
	Span* _next = nullptr;		//

	void* _freeList = nullptr;	//切好大小的链表

	size_t _usecount = 0;		//使用数量
	bool _isUse = false;
};
```

&emsp;&emsp;于是，合并代码如下：

```c++
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//对前后页合并，缓解内存碎片问题
	while (true)
	{
		PageID prev = span->_pageId - 1;
		auto it = _idSpanMap.find(prev);
		if (it != _idSpanMap.end())
		{

			Span* prevSpan = it->second;
			if (prevSpan->_usecount)
			{
				break;
			}
			if (prevSpan->_isUse)
			{
				break;
			}
			if (prevSpan->_n + span->_n > NPAGES - 1)
			{
				break;
			}
			//合并
			span->_pageId = prevSpan->_pageId;
			span->_n += prevSpan->_n;

			_spanLists[prevSpan->_n].Erase(prevSpan);
			delete prevSpan;
		}
		else
		{
			break;
		}
	}
	while (true)
	{
		PageID next = span->_pageId + 1;
		auto it = _idSpanMap.find(next);
		if (it != _idSpanMap.end())
		{
			Span* nextSpan = it->second;
			if (nextSpan->_usecount)
			{
				break;
			}
			if (nextSpan->_isUse)
			{
				break;
			}
			if (nextSpan->_n + span->_n > NPAGES - 1)
			{
				break;
			}
			//合并
			span->_n += nextSpan->_n;

			_spanLists[nextSpan->_n].Erase(nextSpan);
			delete nextSpan;
		}
		else
		{
			break;
		}
	}
	_spanLists->PushFront(span);
	span->_isUse = false;
	_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId + span->_n - 1] = span;
}
```

# 大于256KB的大块内存申请

**申请**

&emsp;&emsp;前面的每个线程的thread cache是用于申请小于等于256KB的内存的，即最大32页；而central cache中span最多也就保存了256KB，因此对于大于256KB的内存，我们可以考虑直接向page cache申请。不过由于page cache中最大的页也就只有128页，倘若是大于128页的内存申请，就只能直接向系统申请了。

```c++
static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size);

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(alignSize));
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageId >> PAGE_SHIFT);
		return ptr;
	}
	if (pTLSTreadCache == nullptr)
	{
		pTLSTreadCache = new TreadCache;
	}
	return pTLSTreadCache->Allocate(size);
}
```

&emsp;&emsp;对`RoundUp`改造一下之前的代码逻辑：

```c++
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
        //大于256KB,按页对齐
        return _RoundUp(size, 1 << PAGE_SHIFT);
    }
}
```

&emsp;&emsp;同时再对`NewSpan`函数进行改造：申请大于256KB的内存时，会直接调用page cache当中的NewSpan函数进行申请，不过我们需要考虑两种情况

-   当需要申请的内存页数大于128页时，`Page Cache`中无法申请到对应大小的span，应该向堆申请对应页数的内存块。
-   而如果申请的内存页数是小于128页时，那就在page cache中进行申请。

```c++
Span* PageCache::NewSpan(size_t k)
{
	//获取K页Span
	assert(k > 0);

	if (k > NPAGES - 1)
	{
		//大于NPAGES,直接向系统申请
		void* ptr = SystemAlloc(k);
		Span* span = new Span;
		span->_pageId = (PageID)(ptr) >> PAGE_SHIFT;
		span->_n = k;
        //建立页号与span之间的映射
		_idSpanMap[span->_pageId] = span;
		return span;
	}

	if (!_spanLists[k].Empty())
	{
		//不为空
		return _spanLists[k].PopFront();
	}
	for (size_t i = k + 1; i < NPAGES; i++)
	{
		if (!_spanLists[i].Empty())
		{
			//不为空，则切割
			Span* kspan = new Span;
			Span* nspan = _spanLists[i].PopFront();
			
			kspan->_n = k;
			kspan->_pageId = nspan->_pageId + k;

			nspan->_n -= k;
			nspan->_pageId += k;

			//存储首尾页号与span的映射，方便回收
			_idSpanMap[nspan->_pageId] = nspan;
			_idSpanMap[nspan->_pageId + nspan->_n - 1] = nspan;

			//建立id和span的映射
			for (PageID i = 0; i < kspan->_n; i++)
			{
				_idSpanMap[i] = kspan;
			}

			_spanLists[nspan->_n].PushFront(nspan);
			return kspan;
		}
	}
	//走到这一步，说明无满足的Span，需要向系统申请
	Span* bigSpan = new Span;
	void* address = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PageID)address >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;
	_spanLists[NPAGES - 1].PushFront(bigSpan);
	return NewSpan(k);
}
```

**释放**

&emsp;&emsp;由于我们在申请大于256KB的内存时，会给申请到的内存建立span结构，并建立起始页号与该span之间的映射关系的原因。但是之前的`page cache`释放的内存并不超过12页，故大于128页直接释放给堆。

```c++
static void ConcurrentFree(void* obj, size_t size)
{
	if (size > MAX_BYTES)
	{
		Span* span = PageCache::GetInstance()->MapObjectToSpan(obj);
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
		return;
	}
	assert(pTLSTreadCache);
	pTLSTreadCache->Delaloocate(obj, size);
}
```

&emsp;&emsp;这里我们使用系统接口：

```c++
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}
```

&emsp;&emsp;代码如下：

```c++
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	if (span->_n > NPAGES - 1)
	{
		//大于128页，直接系统释放
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		delete span;
		return;
	}
	//...
}
```

# 使用定长内存池配合脱离使用new

**Page Cache**

&emsp;&emsp;tcmalloc是在高并发场景下替代malloc的，但new在底层实际上就是封装了malloc的，因此tcmalloc为了完全脱离掉malloc函数，其内部是不能调用malloc函数的，故我们用定长内存池配合配合脱离new。

```c++
class PageCache
{
private:
	ObjectPool<Span> _spanPool;
};
```

&emsp;&emsp;因此，凡是使用`new`的地方，我们可以用`Span* span = _spanPool.New()`替代，用`_spanPool.Delete(span)`替代`delete`。

<hr> </hr>

**Thread Cache**

&emsp;&emsp;由于每个线程第的thread cache也是new出来的，我们也需要对其进行替换。其中，`ObjectPool<ThreadCache> tcPool`设为静态。

```c++
static void* ConcurrentAlloc(size_t size)
{
    //...
	if (pTLSTreadCache == nullptr)
	{
		static ObjectPool<ThreadCache> tcPool;
		//pTLSTreadCache = new ThreadCache;
		pTLSTreadCache = tcPool.New();
	}
	return pTLSTreadCache->Allocate(size);
}
```

<hr> </hr>

**SpanList**

&emsp;&emsp;在SpanList的构造函数中也用到了new，也稍加修改。

```c++
class SpanList
{
public:
	SpanList()
	{
		_head = _spanPool.New();
		_head->_prev = _head;
		_head->_next = _head;
	}
private:
	static ObjectPool<Span> _spanPool;
}
```

# 释放对象时优化不传对象大小
&emsp;&emsp;当我们使用`free`或`delete`的时候，只需要传入参数即可。那么，我们是否可以在调用`ConcurrentFree`时也可以不用传入大小呢？换言之，倘若我们仅传入地址参数，那么是否可以自动计算其大小呢？例如上文，我们根据地址计算出其属于哪一页，属于哪一个Span。
&emsp;&emsp;因此我们就需要建立对象地址与对象大小之间的映射。常规想法是建立哈希表等，保存地址与大小的映射，不过这样太浪费。由于可以通过对象的地址找到其对应的span，而同一span下的自由链表中的对象大小相同。因此我们可以在Span结构中再增加一个_objSize成员，用以记录这个span管理的内存块被切成的一个个对象的大小。

```c++
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
```

&emsp;&emsp;由此，释放函数也可以变化：

```c++
static void ConcurrentFree(void* obj)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(obj);
	size_t size = span->_objSize;
	if (size > MAX_BYTES)
	{
		//Span* span = PageCache::GetInstance()->MapObjectToSpan(obj);
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
		return;
	}
	assert(pTLSTreadCache);
	pTLSTreadCache->Delaloocate(obj, size);
}
```

&emsp;&emsp;同理，当获取新的span时，我们应当记录其大小：

```c++
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//...
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;
	span->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();
	//...
}
```

&emsp;&emsp;同时也有一个问题：我们对map进行读操作时，是未加锁的。比如当某一线程正在修改，而此另一线程正在读，那么可能会出现问题。因此当我们在page cache外部访问这个映射关系时是需要加锁的。

```c++
Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID id = (PageID)obj >> PAGE_SHIFT;
	std::unique_lock<std::mutex> lock(_pageMtx);
	auto ret = _idSpanMap.find(id);
	if (ret == _idSpanMap.end())
	{
		assert(false);
		return nullptr;
	}
	return ret->second;
}
```

# 多线程环境下对比malloc测试

&emsp;&emsp;我们在多线程场景下对比malloc进行测试。测试代码如下：

```c++
#include"ConcurrentAlloc.h"

// ntimes 一轮申请和释放内存的次数
// rounds 轮次
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&, k]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					//v.push_back(malloc(16));
					v.push_back(malloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	cout << nworks << "个线程并发执行" << nworks << "轮次，每轮次malloc "
		<< ntimes << "次: 花费：" << malloc_costtime << "ms" << endl;
	cout << nworks << "个线程并发执行" << nworks << "轮次，每轮次free   "
		<< ntimes << "次: 花费：" << free_costtime << "ms" << endl;
	cout << nworks << "个线程并发malloc&free " << nworks * rounds * ntimes
		<< "次: 总计花费：" << malloc_costtime + free_costtime << "ms" << endl;
}


// 单轮次申请释放次数 线程数 轮次
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					//v.push_back(ConcurrentAlloc(16));
					v.push_back(ConcurrentAlloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	cout << nworks << "个线程并发执行" << nworks << "轮次，每轮次concurrent alloc   "
		<< ntimes << "次: 花费：" << malloc_costtime << "ms" << endl;
	cout << nworks << "个线程并发执行" << nworks << "轮次，每轮次concurrent dealloc "
		<< ntimes << "次: 花费：" << free_costtime << "ms" << endl;
	cout << nworks << "个线程并发concurrent alloc&dealloc " << nworks * rounds * ntimes
		<< "次: 总计花费：" << malloc_costtime + free_costtime << "ms" << endl;
}

int main()
{
	size_t n = 10000;
	cout << "============================================================================" << endl;
	BenchmarkConcurrentMalloc(n, 10, 10);
	cout << "============================================================================" << endl;
	BenchmarkMalloc(n, 10, 10);
	cout << "============================================================================" << endl;

	return 0;
}
```

&emsp;&emsp;其中，各参数含义如下：

```c++
ntimes	:单轮次申请和释放内存的次数。
nworks	:线程数。
rounds	:轮次。
```

&emsp;&emsp;我们分别对固定大小和随机大小内存的申请和释放进行多次测试，部分结果如下图所示：

```c++
v.push_back(malloc(17));
v.push_back(ConcurrentAlloc(17));
v.push_back(malloc((16 + i) % 8192 + 1));
v.push_back(ConcurrentAlloc((16 + i) % 8192 + 1));
```

![image-20221030203545494](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210302158632.png)

![image-20221030202557177](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210302158633.png)

多轮测试后发现，随着每轮次的申请释放次数上升，ConcurrentAlloc的效率也有了较大增长，但相比malloc来说还是差一点点。

# 性能瓶颈分析

&emsp;&emsp;在这里，我们采用的平台是**Visual Studio 2022**，我们在Debug模式下点击 **调试** :point_right: **性能探查器** 后按照下图选择 **检测**:point_right: **确定**，同时屏蔽`malloc`的测试即可。
![image-20221030204615521](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210302158634.png)
&emsp;&emsp;稍加等待，结果如下所示：可以看到，`void Delaloocate()`和`Span* MapObjectToSpan()`占用时间最长，

![image-20221030204852841](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210302158635.png)

&emsp;&emsp;&emsp;我们点击`void Delaloocate()`后发现，`ListTooLong()`是其中占用时间最长的，

![image-20221030205124689](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210302158636.png)

&emsp;&emsp;再次点击，`ReleaseListToSpan()`是其中占用时间最长的，&emsp;&emsp;

![image-20221030205150949](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210302158637.png)

&emsp;&emsp;重复上述逻辑

![image-20221030205044791](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210302158638.png)

&emsp;&emsp;重复上述逻辑后发现，由于锁的竞争问题，其效率大大降低了。

![image-20221030210627345](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210302158639.png)
# 针对性能瓶颈分析使用基数树优化

&emsp;&emsp;对于**长整型数据**的映射，如何解决Hash冲突和Hash表的设计是一个令人头疼的问题。**radix Tree(基数树) 差不多是传统的二叉树**，只是在寻找方式上，利用比如一个unsigned int的类型的每一个比特位作为树节点的判断。换言之，基数树就是一个分层的哈希表，可分为单层基数树、二层基数树、三层基数树等。

参考文章:point_right: [自适应基数树：主内存数据库的 ARTful 索引|IEEE会议出版物|IEEE Xplore](https://ieeexplore.ieee.org/document/6544812?arnumber=6544812)

&emsp;&emsp;在tcmalloc源码中，就是使用基数树解决上述问题的。在这里我们采用`WIN32`下单层基数树测试，在64位下，则需要三层基数树了。

```c++
template <int BITS>
class TCMalloc_PageMap1 {
private:
	static const int LENGTH = 1 << BITS;
	void** array_;
public:
	typedef uintptr_t Number;
	explicit TCMalloc_PageMap1() {
		//explicit TCMalloc_PageMap1(void* (*allocator)(size_t)) {
			//array_ = reinterpret_cast<void**>((*allocator)(sizeof(void*) << BITS));
		size_t size = sizeof(void*) << BITS;
		size_t alignSize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		array_ = (void**)SystemAlloc(alignSize >> PAGE_SHIFT);
		memset(array_, 0, sizeof(void*) << BITS);
	}
	// Return the current value for KEY. Returns NULL if not yet set,
	// or if k is out of range.
	void* get(Number k) const {
		if ((k >> BITS) > 0) {
			return NULL;
		}
		return array_[k];
	}
	// REQUIRES "k" is in range "[0,2^BITS-1]".
	// REQUIRES "k" has been ensured before.
	//
	// Sets the value 'v' for key 'k'.
	void set(Number k, void* v) {
		array_[k] = v;
	}
};
```

&emsp;&emsp;与此同时，修改前文等部分代码，如：

```c++
_idSpanMap.set(span->_pageId, span);
```

&emsp;&emsp;除此之外，我们还需要解除`MapObjectToSpan()`内部的锁。**原因**如下：

1.   在C++中map的底层为红黑树，unordered_map的底层为哈希表，当我们在插入数据时其底层结构都有可能会发生变化。例如红黑树的旋转，哈希表增容
2.   而基数树来说就不一样了，只要你对其写入，其空间必定已经开辟好了。换言之，无论什么时候去读取某个页的映射，其位置不变。也就是说，其写入和读取是不冲突的，不需要加锁的。

我们将底层换成基数树，在`WIN32`下进行测试，可以看到其效率大幅度上升。如下图所示：

![image-20221030204144591](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210302158641.png)

![image-20221030204013870](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210302158642.png)

<hr> </hr>

在实际中，tcmalloc是可以替换malloc的，在Linux下，`void* malloc(size_t size) THROW attribute__ ((alias (tc_malloc)))  `
可以用来替换，在Windows下则使用hook技术，可参考:point_right:  [Hook技术](https://www.cnblogs.com/feng9exe/p/6015910.html)

除此之外，我们也可以将自己写好的代码打包成库：

![image-20221030215715296](https://xcs-md-images.oss-cn-nanjing.aliyuncs.com/Linux/ConcurrencyMemoryPool/202210302158643.png)

完整项目代码:point_right: [高并发内存池: C++项目：高并发内存池 (gitee.com)](https://gitee.com/xi_xiansheng/ConcurrencyMemoryPool)  

------

&emsp;&emsp;以上就是本次带来的项目【高并发内存池】，欢迎各位批评指正！
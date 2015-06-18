#include "stdafx.h"
#include "pool.h"

#pragma warning(disable:4311;disable:4291)



#pragma region 对象初始化


//! 构造函数
//!	初始对象不预分配内存
MemPool::MemPool(VOID)
{

}
//! 析构函数
MemPool::~MemPool(VOID)
{
	Release();
}


//! 默认初始化对象
//! 按默认的方式预分配内存
VOID MemPool::DefaultInit(VOID)
{
	for(LONG i = 0; i < MAX_TILE_BUF_SZIE; ++i)
	{
		AddBlock(i);
	}
}

//! 指定一个尺寸的内存预先分配多少块
BOOL MemPool::SetInitialTileNum(LONG lSize, LONG lNum)
{
	return AddBlock(GetSizeIndex(lSize), lNum);
}

//! 释放所有内存
VOID MemPool::Release(VOID)
{
#ifdef MEM_POOL_DEBUG
	MyAssert(0 == m_setBigMem.size());
#endif
	for(LONG i = 0; i < MAX_TILE_BUF_SZIE; ++i)
	{
		FreeAllBlockByIndex(i);
	}
}


#pragma  endregion


#pragma region 功能应用

//! 得到一块指定大小的内存
//! 该内存被0填充
VOID* MemPool::Alloc(LONG lSize)
{
	if(MAX_TILE_BUF_SZIE * MIN_BUF_SIZE_DIFFERENCE < lSize)
	{
		LPVOID pBigMem = malloc(lSize);
#ifdef MEM_POOL_DEBUG
		MyAssert(m_setBigMem.end() == m_setBigMem.find(pBigMem));
		m_setBigMem.insert(pBigMem);
#endif
		return pBigMem;
	}
	else
	{
		LONG lIndex = GetSizeIndex(lSize);
		MyAssert(-1 != lIndex);
		if( 0 == m_arrFreeList[lIndex]._TileFreeList.Size())
		{
			//! 如果空闲内存用完，则再分配新的大块
			if(!AddBlock(lIndex))
			{
				//! 内存耗尽
				return NULL;
			}
		}

		//! 从空闲链表中删除一个节点，用0填充节点包含的内存
		VOID *pReBuf = m_arrFreeList[lIndex]._TileFreeList.PopNode()->GetTile(); 
#ifdef FILL_MEM
		memset(pReBuf, FILL_VALUE, (lIndex * MIN_BUF_SIZE_DIFFERENCE));
#endif

		return pReBuf;
	}
}


//! 释放一块指明大小的内存
BOOL MemPool::Free(VOID *pBuf, LONG lSize)
{
	if(MAX_TILE_BUF_SZIE * MIN_BUF_SIZE_DIFFERENCE < lSize)
	{
		try
		{
#ifdef MEM_POOL_DEBUG
			MyAssert(m_setBigMem.end() != m_setBigMem.find(pBuf));
			m_setBigMem.erase(pBuf);
#endif
			free(pBuf);
		}
		catch (...)
		{
			MyAssert(false);
			return FALSE;
		}
		return TRUE;
	}

	LONG lIndex = GetSizeIndex(lSize);
	MyAssert(-1 != lIndex);
	tagHeap &MemHeap = m_arrFreeList[lIndex];

	//! 检测释放的内存是否是对象分配出去的
#ifdef MEM_POOL_DEBUG	
	{
		ListNode<tagBlockNode> *pCurrNode = MemHeap._BlockList.GetHead();
		BOOL bFind = FALSE;
		while(NULL != pCurrNode)
		{
			LPBYTE pBegin = (LPBYTE)pCurrNode->GetTile();
			LPBYTE pEnd = pBegin + pCurrNode->GetNodeData()->lBlockSize - ListNode<tagBlockNode>::GetAddedSize();
			if((LONG)pBegin < (LONG)pBuf && (LONG)pEnd > (LONG)pBuf)
			{
				bFind = TRUE;
				break;
			}
			pCurrNode = pCurrNode->GetNext();
		}

		if(!bFind)MyAssert(false);
	}
#endif

	//! 将内存片节点归还空闲链表
	LPBYTE pByteBuf = (LPBYTE)pBuf;
	pByteBuf = pByteBuf - ListNode<VOID>::GetAddedSize();
	MemHeap._TileFreeList.PushNode((ListNode<VOID>*)pByteBuf);

	return TRUE;
}


#pragma  endregion


#pragma region 私有成员


//! 为某一尺寸内存池添加一块内存
BOOL MemPool::AddBlock(LONG lIndex, LONG lTileNum)
{
	if(MAX_TILE_BUF_SZIE <= lIndex || 0 > lIndex) return FALSE;
	tagHeap &MemHeap = m_arrFreeList[lIndex];

	LONG lTileSize = MIN_BUF_SIZE_DIFFERENCE * (lIndex + 1);

	//! 分配的具体块数取lMinTileNum的整数倍
	LONG lMinTileNum = USABLE_BLOCK_SIZE / lTileSize;
	lTileNum = (lMinTileNum > lTileNum) ? lMinTileNum : lTileNum;
	lTileNum = ((((lTileNum % lMinTileNum) == 0) ? 0 : 1) + lTileNum / lMinTileNum) * lMinTileNum;


	//! 实际分配的大小 = 块数量 * （可用块大小 + 块链表节点大小） + 大块内存的链表节点大小
	LONG lAllocSize = lTileNum * (lTileSize + ListNode<VOID>::GetAddedSize()) + ListNode<tagBlockNode>::GetAddedSize();

	void *pNewBuf = malloc(lAllocSize);
	if(NULL == pNewBuf)
		return FALSE;

	//! 内存块添加到管理
	ListNode<tagBlockNode> *pBlockNode = new(pNewBuf)ListNode<tagBlockNode>();
	tagBlockNode *p = pBlockNode->GetNodeData();
	p->lBlockSize = lAllocSize;

	MemHeap._BlockList.PushNode(pBlockNode);
	MemHeap._lTileNum += lTileNum;
	MemHeap._lLastAddNum = lTileNum;

	//! 内存片添加到管理
	LPBYTE pTileNode = (LPBYTE)(pBlockNode->GetTile());

	for (LONG i = 0; i < lTileNum; ++i)
	{
		ListNode<VOID> *pTmpNode = (ListNode<VOID>*)(pTileNode + i * (lTileSize + ListNode<VOID>::GetAddedSize()));
		new(pTmpNode)ListNode<VOID>();
		MemHeap._TileFreeList.PushNode((ListNode<VOID>*)pTmpNode);
	}

	return TRUE;
}

//! 释放一个尺寸索引的所有内存块
VOID MemPool::FreeAllBlockByIndex(LONG lIndex)
{
	MyAssert(MAX_TILE_BUF_SZIE > lIndex);
	tagHeap &MemHeap = m_arrFreeList[lIndex];

	MyAssert(MemHeap._lTileNum == MemHeap._TileFreeList.Size());

	MemHeap._TileFreeList.Release();
	ListNode<tagBlockNode> *pCurrNode = MemHeap._BlockList.PopNode();
	while(NULL != pCurrNode)
	{
		pCurrNode->Rlease();
		free(pCurrNode);
		pCurrNode = MemHeap._BlockList.PopNode();
	}
	MemHeap._lTileNum = 0;
}

//! 得到一个尺寸的索引
inline LONG MemPool::GetSizeIndex(LONG lSize)
{
	if(MAX_TILE_BUF_SZIE * MIN_BUF_SIZE_DIFFERENCE < lSize)
		return -1;
	MyAssert(0 < lSize);
	return lSize / MIN_BUF_SIZE_DIFFERENCE - ((0 == (lSize % MIN_BUF_SIZE_DIFFERENCE)) ? 1 : 0);
}


#pragma endregion

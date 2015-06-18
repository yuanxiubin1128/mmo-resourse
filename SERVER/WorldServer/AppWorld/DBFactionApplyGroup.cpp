#include "stdafx.h"
#include "../worldServer/game.h"
#include "DBFactionApplyGroup.h"
#include "DBentityManager.h"
#include "DBDupRgn.h"
#include "DBFaction.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CDBFactionApplyGroup::CDBFactionApplyGroup()
: CBaseDBEntity()
{
	GetGame()->GetDBEntityManager()->GenerateGUID(m_GUID);
}

CDBFactionApplyGroup::CDBFactionApplyGroup(const CGUID& id)
: CBaseDBEntity(id)
{
}
CDBFactionApplyGroup::~CDBFactionApplyGroup()
{
	set<CDBFactionApply*>::iterator itr = m_setApplyGroup.begin();
	for(; itr != m_setApplyGroup.end(); itr++)
	{
		SAFE_DELETE(*itr);
	}
	m_setApplyGroup.clear();
}

// virtual interfaces
void CDBFactionApplyGroup::ProcessMsg(DB_OPERATION_TYPE op, long msgid, BYTE* msgBuf, long& msgBufPos, long bufSize)
{
	switch(op)
	{
	case DB_OPER_SAVE:
		{
		}
		break;
	case DB_OPER_LOAD:
		{
			Load(msgid, msgBuf, msgBufPos, bufSize);
		}
		break;
	}
}

void CDBFactionApplyGroup::AddToByteArray(vector<BYTE>& pBA)
{
	_AddToByteArray(&pBA, m_GUID); // ʵ��ID
	_AddToByteArray(&pBA, m_OwnerGUID); // ӵ����ID
	_AddToByteArray(&pBA, (WORD)m_setApplyGroup.size()); // ��Ʒ����

	set<CDBFactionApply*>::iterator itr = m_setApplyGroup.begin();
	for(; itr != m_setApplyGroup.end(); itr++)
	{
		(*itr)->AddEntityDataToMsg(DB_OPER_LOAD, pBA);
	}
}
void CDBFactionApplyGroup::Clear()
{
	m_mapDirtyAttrData.clear();
	set<CDBFactionApply*>::iterator itr = m_setApplyGroup.begin();
	for(; itr != m_setApplyGroup.end(); itr++)
		SAFE_DELETE(*itr);
	m_setApplyGroup.clear();
}

void CDBFactionApplyGroup::Load(long msgid, BYTE* msgBuf, long& msgBufPos, long bufSize)
{ 
	Clear();

//	_GetBufferFromByteArray(msgBuf, msgBufPos, m_OwnerGUID, bufSize); // ӵ����ID

	WORD goodsNum = _GetWordFromByteArray(msgBuf, msgBufPos, bufSize); // ��Ʒ����

	for(int j=0; j<goodsNum; j++)
	{
		// ������Ϣ
		CGUID goodsID;
		_GetBufferFromByteArray(msgBuf, msgBufPos, goodsID, bufSize);
		CDBFactionApply* existGoods = new CDBFactionApply(goodsID);
		if(existGoods)
		{
			existGoods->Load(0, msgBuf, msgBufPos, bufSize);
			m_setApplyGroup.insert(existGoods);
		}
	}
}

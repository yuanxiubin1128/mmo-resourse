//EntityManager.cpp/////////////////////////////////////////////////////////////////////
//对象类:该对象管理所有的对象实例和对象的配置信息数据以及与DBS的会话管理
//Author:安海川
//Create Time:2008.11.03
#include "StdAfx.h"
#include "../../WorldServer/game.h"
#include "entityManager.h"
#include "../Container\CEquipmentContainer.h"
#include "../Container\CSubpackContainer.h"
#include "EntityGroup.h"
#include "../../worldserver/WorldServer.h"
#include "../public/readwrite.h"
#include "../../nets/networld/Message.h"
#include "..\dbentity/entityManager.h"

#include "../session/CSessionFactory.h"
#include "../session/WorldServerSession.h"
#include "../session/CSession.h"
#include "../Country/CountryHandler.h"
#include "../Country/Country.h"
#include "../goods\CGoods.h"
#include "../goods/CGoodsFactory.h"
#include "../goods/CGoodsBaseProperties.h"
#include "../goods/IncrementShopManager.h"
#include "../organizingsystem/OrganizingParam.h"
#include "../organizingsystem/Faction.h"
#include "../organizingsystem/OrganizingCtrl.h"
#include "../Linkman/LinkmanSystem.h"
#include "../script/VariableList.h"
#include "../GlobalRgnManager.h"
#include "../Mail/Mail.h"
#include "../Mail/MailManager.h"
#include "../PetCtrl.h"
#include "../business/CBusinessManager.h"
#include "..\..\public\QuestIndexXml.h"
#include "..\..\public\LotteryXml.h"
#include "entity.h"



using namespace std;

// 是否还在运行
bool bDBSExit;
std::string NULL_STRING = "";

tagEntityBuildInfo::tagEntityBuildInfo()
: lDbQueryType(-1),
lComType(0),strComFlag(""),lHeight(0),
guid(NULL_GUID),strTblName(""),lDbOperFlag(0),lLeafNum(0),
strPrimarykey(""),lHhasAttrFlag(0),lLeafComType(0),
strLeafComFlag(""),lHasDetailLeaves(0),strDbQueryName("")
{
	memset(pQueryVar, 0, sizeof(pQueryVar));
	pDataMgr.clear();
}

tagEntityBuildInfo::~tagEntityBuildInfo()
{
	pDataMgr.clear();
	map<string, tagBuildFileInfo*>::iterator itr = mapBuildFileInfo.begin();
	for(; itr != mapBuildFileInfo.end(); itr++)
		OBJ_RELEASE(tagBuildFileInfo,itr->second);
}

void tagBuildFileInfo::DecodeFromByteArray(BYTE* buf, long& pos, long bufSize)
{
	_GetBufferFromByteArray(buf, pos, leafGuid, bufSize);
	lLevel = _GetLongFromByteArray(buf, pos, bufSize);
	lComType = _GetLongFromByteArray(buf, pos, bufSize);
	char szStr[128];
	_GetStringFromByteArray(buf, pos, szStr, bufSize);
	strLeafComFlag = szStr;
	_GetStringFromByteArray(buf, pos, szStr, bufSize);
	strRootComFlag = szStr;
	_GetBufferFromByteArray(buf, pos, rootGuid, bufSize);		
}
void tagEntityBuildInfo::DecodeFromByteArray(BYTE* buf, long& pos, long bufSize)
{
	lComType = _GetLongFromByteArray(buf, pos, bufSize);
	char szStr[128];
	_GetStringFromByteArray(buf, pos, szStr, bufSize);
	strComFlag = szStr;
	lHeight = _GetLongFromByteArray(buf, pos, bufSize);
	_GetBufferFromByteArray(buf, pos, guid, bufSize);
	_GetStringFromByteArray(buf, pos, szStr, bufSize);
	strTblName = szStr;
	lDbOperFlag =_GetLongFromByteArray(buf, pos, bufSize);
	lLeafNum = _GetLongFromByteArray(buf, pos, bufSize);
	_GetStringFromByteArray(buf, pos, szStr, bufSize);
	strPrimarykey = szStr;
	lHhasAttrFlag = _GetLongFromByteArray(buf, pos, bufSize);
	lLeafComType = _GetLongFromByteArray(buf, pos, bufSize);
	_GetStringFromByteArray(buf, pos, szStr, bufSize);
	strLeafComFlag = szStr;
	lHasDetailLeaves = _GetLongFromByteArray(buf, pos, bufSize);

	_GetStringFromByteArray(buf, pos, szStr, bufSize);		//数据库查找变量名
	if (szStr[0] != '\0')
	{
		strDbQueryName = szStr;
	}
	BYTE flag = _GetByteFromByteArray(buf, pos, bufSize);
	if(flag == 1)
	{
		lDbQueryType = _GetLongFromByteArray(buf, pos, bufSize);		//数据库查找变量类型
		_GetBufferFromByteArray(buf, pos, pQueryVar, sizeof(pQueryVar), bufSize);
	}
	// 解析消息
	WORD attrNum = _GetWordFromByteArray(buf, pos, bufSize); // 属性种类个数
	if(attrNum)
	{
		char varName[1024] = {0};
		for(int i=0; i<(int)attrNum; i++)
		{
			tagEntityAttrInfo tagAttrInfo;
			_GetStringFromByteArray(buf, pos, varName, bufSize);//变量名字符串
			tagAttrInfo.lAttrTypeEnum = _GetLongFromByteArray(buf, pos, bufSize);//变量类型枚举值（long）
			tagAttrInfo.eNumDataType = (DATA_OBJECT_TYPE)_GetLongFromByteArray(buf, pos, bufSize);//变量类型（long）
			GetGame()->GetEntityManager()->AddAttrEnumAndStr(string(varName), tagAttrInfo.lAttrTypeEnum);
			pDataMgr[string(varName)] = tagAttrInfo;
		}
	}

	WORD tNum = _GetWordFromByteArray(buf, pos, bufSize);
	if(tNum)
	{
		map<string, tagBuildFileInfo*>::iterator tbItr = mapBuildFileInfo.begin();
		for(; tbItr != mapBuildFileInfo.end(); tbItr++)
		{
			OBJ_RELEASE(tagBuildFileInfo,tbItr->second);
		}
		mapBuildFileInfo.clear();
	}
	for(int i=0; i<tNum; i++)
	{
		tagBuildFileInfo* tFileInfo = OBJ_CREATE(tagBuildFileInfo);
		tFileInfo->DecodeFromByteArray(buf, pos, bufSize);
		mapBuildFileInfo[tFileInfo->strLeafComFlag] = tFileInfo;
	}
}

CEntityManager::CEntityManager()
:m_lAttrEnumValue(-1),m_lNewDbPlayerNum(0)
{
}
CEntityManager::~CEntityManager()
{
	std::map<string, tagEntityBuildInfo*>::iterator deItr = m_mapObjAttrDef.begin();
	for(; deItr != m_mapObjAttrDef.end(); deItr++)
	{
		OBJ_RELEASE(tagEntityBuildInfo,deItr->second);
	}
	m_mapObjAttrDef.clear();

	DBEntityComponentMapItr enItr = m_mapEntitys.begin();
	for (; enItr != m_mapEntitys.end(); enItr++)
	{
		map<CGUID, CEntityGroup*>::iterator entityItr = enItr->second.begin();
		for(; entityItr != enItr->second.end(); entityItr++)
			DelBaseEntity((CBaseEntity*)entityItr->second);
	}
	m_mapEntitys.clear();

	AccountMapItr accItr = m_mapAccounts.begin();
	for(; accItr != m_mapAccounts.end(); accItr++)
		DelBaseEntity((CBaseEntity*)accItr->second);
	m_mapAccounts.clear();
}

//-------------- interfaces
// CBaseEntity
// 添加一个BaseEntity到Map末尾
void CEntityManager::AddOneRootEntity(CEntityGroup* entity)
{
	if(entity)
	{
		DBEntityComponentMapItr itr = m_mapEntitys.find(entity->GetCompositeFlag());
		if(itr == m_mapEntitys.end())
		{
			map<CGUID, CEntityGroup*> entityMap;
			entityMap[entity->GetGUID()] = entity;
			m_mapEntitys[entity->GetCompositeFlag()] = entityMap;
			return;
		}
		else
		{
			map<CGUID, CEntityGroup*>::iterator enItr = itr->second.find(entity->GetGUID());
			if(enItr != itr->second.end())
			{
				if(entity != enItr->second)
				{
					MP_DELETE(enItr->second);
				}
				else
				{
					char szGuid[128];
					entity->GetGUID().tostring(szGuid);
					AddLogText("AddOneRootEntity:old ptr[%s],type[%s] has found.", szGuid, entity->GetCompositeFlag().c_str());
				}
				itr->second.erase(enItr);
			}
			itr->second[entity->GetGUID()] = entity;
		}
	}
}
// 删除一个BaseEntity
void CEntityManager::DelOneRootEntity(CEntityGroup* entity)
{
	DBEntityComponentMapItr itr = m_mapEntitys.find(entity->GetCompositeFlag());
	if(itr != m_mapEntitys.end())
	{
		map<CGUID, CEntityGroup*>::iterator enItr = itr->second.find(entity->GetGUID());
		if(enItr != itr->second.end())
		{
			DelBaseEntity((CBaseEntity*)enItr->second);
			itr->second.erase(enItr);
		}
	}
}
// 删除一个BaseEntity
void CEntityManager::DelOneRootEntity(const string& comFlag, const CGUID& playerID)
{
	DBEntityComponentMapItr itr = m_mapEntitys.find(comFlag);
	if(itr != m_mapEntitys.end())
	{
		map<CGUID, CEntityGroup*>::iterator enItr = itr->second.find(playerID);
		if(enItr != itr->second.end())
		{
			DelBaseEntity((CBaseEntity*)enItr->second);
			itr->second.erase(enItr);
		}
	}

}
// 从消息取得操作对象
CEntityGroup* CEntityManager::FindRootEntity(const string& flag, const CGUID& guid)
{
	DBEntityComponentMapItr itr = m_mapEntitys.find(flag);
	if(itr != m_mapEntitys.end())
	{
		std::map<CGUID, CEntityGroup*>::iterator enItr = itr->second.find(guid);
		if(enItr != itr->second.end())
			return enItr->second;
	}
	return NULL;
}
// 创建对象
CEntityGroup* CEntityManager::CreateRootEntity(const string& flag, const CGUID& guid)
{
	CEntityGroup* retEntity = NULL;
	if(GetGame()->GetDbIsReady())
	{
		retEntity = (CEntityGroup*)GetGame()->GetEntityManager()->NewBaseEntity(flag, guid);
		// 默认此构造对象为root
		if(retEntity)
		{
			AddOneRootEntity(retEntity);
		}
	}
	else
	{
		AddLogText("CreateRootEntity() 构造对象[%s]时未获取DBS的配置数据!", flag.c_str());
	}
	return retEntity;
}
void CEntityManager::ProcessMsg(CMessage* pMsg)
{
	//需要返回的消息类型
	CGUID SessionID;
	pMsg->GetGUID(SessionID);

	long retFlag = pMsg->GetLong();

	// root comflag
	char rootComFlag[64];
	memset(rootComFlag, 0, sizeof(rootComFlag));
	pMsg->GetStr(rootComFlag, sizeof(rootComFlag));

	// root guid
	CGUID rootGuid;
	pMsg->GetGUID(rootGuid);

	// root name
	char szRootName[128];
	pMsg->GetStr(szRootName, sizeof(szRootName));

	BYTE rootFindFlag = pMsg->GetByte();

	long bufSize = pMsg->GetLong();
	long pos = 0;
	BYTE* buf = (BYTE*)M_ALLOC(bufSize);
	pMsg->GetEx(buf, bufSize);

	_GetByteFromByteArray(buf, pos, bufSize);
	char szComFlag[128];
	_GetStringFromByteArray(buf, pos, szComFlag, bufSize);
	CGUID leafGuid;
	_GetBufferFromByteArray(buf, pos, leafGuid, bufSize);
	BYTE DbOperFlag = _GetByteFromByteArray(buf, pos, bufSize);
	BYTE DBOperType = _GetByteFromByteArray(buf, pos, bufSize);

	// find entity
	CEntityGroup* entity = NULL;

	if( strcmp(rootComFlag, szComFlag) != 0 )// 是子节点
	{
		CEntityGroup* rootEntity = NULL;
		if( strcmp(rootComFlag, "[account]") == 0 )
			rootEntity = (CEntityGroup*)FindAccount(szRootName);
		else
			rootEntity = FindRootEntity(string(rootComFlag), rootGuid);

		if(rootEntity)
		{
			entity = (CEntityGroup*)(rootEntity->FindEntityBytravelTree(leafGuid));
		}
	}
	else
	{
		if( strcmp(rootComFlag, "[account]") == 0 )
		{
			entity = FindAccount(szRootName);
		}
		else if (strcmp(rootComFlag, "[friendgroup]") == 0 )
		{
			entity = (CEntityGroup*)GetGame()->GetEntityManager()->NewBaseEntity(string(rootComFlag), rootGuid);
		}
		else
			entity = FindRootEntity(string(rootComFlag), rootGuid);
	}

	if(entity)
	{
		entity->SetCurDbOperType(DBOperType);
		entity->SetCurDbOperFlag(DbOperFlag);
	}

	CSession* pSession = const_cast<CSession*>(GetGame()->GetSessionFactoryInst()->QuerySession(SessionID));
	if(pSession)
	{
		CWorldServerSession* wsPlug = static_cast<CWorldServerSession*>(pSession);
		if(wsPlug)
		{
			if(!wsPlug->IsEndSessionStepItr()) //会话还未结束
			{
#ifdef _RUNSTACKINFO_
				char procInfo[256];
				char procGuid[64];
				rootGuid.tostring(procGuid);
				sprintf(procInfo, "ProcessMsg() entity:%s|%s, step:%d Start.", rootComFlag, procGuid, wsPlug->GetCurSessionStep().lStep);
				CMessage::AsyWriteFile(GetGame()->GetStatckFileName(), procInfo);
#endif

				switch((CWorldServerSession::WORLD_SESSION_STEP_DEF)wsPlug->GetCurSessionStep().lStep)
				{
				case CWorldServerSession::WORLD_SESSION_LOAD_ACCOUNT:
					{
						if(!entity && szRootName[0] != '\0')
						{
							entity = MP_NEW Account;
							CGUID accGuid;
							CGUID::CreateGUID(accGuid);
							entity->SetGUID(accGuid);
							((Account*)entity)->SetName(szRootName);
							if(!AddAccount((Account*)entity))
							{
								MP_DELETE(entity);
							}
						}
						if(entity)
						{
							entity->ProcessMsg(true, SessionID, retFlag, buf, pos, bufSize);
						}
						wsPlug->ProcessLoadAccount(retFlag, entity);
					}
					break;
				case CWorldServerSession::WORLD_SESSION_LOAD_COUNTRY:
					{
						if(entity)
						{
							entity->ReleaseChilds();
							entity->ProcessMsg(true, SessionID, retFlag, buf, pos, bufSize);
						}

						wsPlug->ProcessLoadCountry(retFlag, entity);
					}
					break;
				case CWorldServerSession::WORLD_SESSION_CREATE_PLAYER_CHECK_CONDITION:
					{
						if(entity)
						{
							entity->ProcessMsg(false, SessionID, retFlag, buf, pos, bufSize);
							wsPlug->ProcessCreatePlayerCheck(retFlag, entity);
						}
					}
					break;
				case CWorldServerSession::WORLD_SESSION_LOAD_PLAYER:
					{
						CEntityGroup* pentity = (CEntityGroup*)GetGame()->GetEntityManager()->NewBaseEntity(rootComFlag, rootGuid);
						if(pentity)
						{
							pentity->ProcessMsg(true, SessionID, retFlag, buf, pos, bufSize);
							wsPlug->ProcessLoadPlayerData(retFlag, pentity);
						}
						GetGame()->GetEntityManager()->DelBaseEntity((CBaseEntity*)pentity);
					}
					break;
				case CWorldServerSession::WORLD_SESSION_LOAD_LINKMAN:
					{
						if(entity)
						{
							entity->ProcessMsg(true, SessionID, retFlag, buf, pos, bufSize);
							if(wsPlug->GetTempDataVec().size()==1) // 有玩家GUID数据记录
							{
								CGUID guid;
								wsPlug->GetTempDataVec()[0]->GetGuidAttr(guid);
								CEntityGroup* pLinkManGroup = (CEntityGroup*)FindLeafByComFlag(entity, string("[playerfriendgroup]"));
								if (pLinkManGroup)
								{
									CEntityGroup* pLinkManGroupOwner = (CEntityGroup*)FindLeafByComFlag(pLinkManGroup, string("[playerfriendgroupOwner]"));
									CEntityGroup* pLinkManGroupAim = (CEntityGroup*)FindLeafByComFlag(pLinkManGroup, string("[playerfriendgroupAim]"));
									GetGame()->GetEntityManager()->CDBFriendGroupToCPlayerFriend(guid, pLinkManGroupOwner, pLinkManGroupAim);
								}
							}
							else
								AddErrorLogText("CWorldServerSession::WORLD_SESSION_LOAD_LINKMAN  wsPlug->GetTempDataVec().size() = %d!!", (long)wsPlug->GetTempDataVec().size());

							DelBaseEntity(entity);
							entity = NULL;
						}
					}
					break;
				case CWorldServerSession::WORLD_SESSION_LOAD_REGION:
					{
						if(entity)
						{
							entity->ProcessMsg(true, SessionID, retFlag, buf, pos, bufSize);
						}
						wsPlug->ProcessLoadRegion(retFlag, entity);
					}
					break;
				case CWorldServerSession::WORLD_SESSION_LOAD_GLOBALVAR:
					{
						if(entity)
						{
							entity->ReleaseChilds();
							entity->ProcessMsg(true, SessionID, retFlag, buf, pos, bufSize);
						}
						wsPlug->ProcessLoadGlobalVar(retFlag, entity);
					}
					break;
					case CWorldServerSession::WORLD_SESSION_LOAD_BUSINESS:
					{
						if(entity)
						{
							entity->ProcessMsg(true, SessionID, retFlag, buf, pos, bufSize);
						}						
						GetInst( Business::CBusinessManager ).DecodeFromEntityGroup( retFlag, entity );
					}
					break;
				case CWorldServerSession::WORLD_SESSION_LOAD_GOODS_LIMIT_RECORD:
					{
						if(entity)
						{
							entity->ProcessMsg(true,SessionID,retFlag,buf,pos,bufSize);
						}
						CIncrementShopManager::GetSingleton().DecodeFromEntityGroup(retFlag,entity);
					}
					break;
				case CWorldServerSession::WORLD_SESSION_INSERT_PLAYER_MAIL:
					{
						if(entity)
						{
							entity->ProcessMsg(false, SessionID, retFlag, buf, pos, bufSize);
						}
						
						wsPlug->ProcessInsertOnlineMail(retFlag, entity);
					}
					break;
				case CWorldServerSession::WORLD_SESSION_SAVE_ONLINE_PLAYER_MAILS:
					{
						if(entity)
						{
							entity->ProcessMsg(false, SessionID, retFlag, buf, pos, bufSize);
						}
						wsPlug->ProcessSaveOnlineMail(retFlag, entity);
					}
					break;
				case CWorldServerSession::WORLD_SESSION_DELETE_ONLINE_PLAYER_MAILS:
					{
						if(entity)
						{
							entity->ProcessMsg(false, SessionID, retFlag, buf, pos, bufSize);
						}
						wsPlug->ProcessDeleteOnlineMails(retFlag, entity);
					}
					break;
				case CWorldServerSession::WORLD_SESSION_LOAD_PLAYER_MAILS:
					{
						if(entity)
						{
							entity->ProcessMsg(true, SessionID, retFlag, buf, pos, bufSize);
						}
						wsPlug->ProcessLoadPlayerMails(retFlag, entity);
					}
					break;
				case CWorldServerSession::WORLD_SESSION_LOAD_FACTION:
					{
						if(entity)
						{
							entity->ProcessMsg(true, SessionID, retFlag, buf, pos, bufSize);
						}
						wsPlug->ProcessLoadFaction(retFlag, entity);
					}
					break;
				case CWorldServerSession::WORLD_SESSION_LOAD_UNION:
					{
						if(entity)
						{
							entity->ProcessMsg(true, SessionID, retFlag, buf, pos, bufSize);
						}
						wsPlug->ProcessLoadUnion(retFlag, entity);
					}
					break;
				case CWorldServerSession::WORLD_SESSION_CHECK_OFFLINE_PLAYER_NAME:
					{
						if(entity)
						{
							entity->ProcessMsg(false, SessionID, retFlag, buf, pos, bufSize);
						}
						wsPlug->ProcessCheckOfflinePlayerNameProc(retFlag, entity);
					}	
					break;
				case CWorldServerSession::WORLD_SESSION_BAN_PLAYER:
					{
						if(entity)
						{
							entity->ProcessMsg(false, SessionID, retFlag, buf, pos, bufSize);
						}
						wsPlug->ProcessBanPlayer(retFlag, entity);
					}	
					break;
				case CWorldServerSession::WORLD_SESSION_LOAD_PHRGN:
					{
						if(entity)
						{
							entity->ProcessMsg(true, SessionID, retFlag, buf, pos, bufSize);
						}
						wsPlug->ProcessLoadPhRgnGroupMsg(retFlag, entity);
					}
					break;
				case CWorldServerSession::WORLD_SESSION_CHANGE_NAME:
					{
						if(entity)
						{
							entity->ProcessMsg(false, SessionID, retFlag, buf, pos, bufSize);
						}
						wsPlug->ProcessChangeNameProcMsg(retFlag, entity);
					}
					break;
				}

				if(retFlag == S_OK || wsPlug->GetCurOperCount() >= 1)
				{
					// 清除该Procedure数据
					if(entity)
					{
						if( strcmp(entity->GetCompositeFlag().c_str(), "[procedure]") == 0 
							|| strcmp(entity->GetCompositeFlag().c_str(), "[mailgroup]") == 0 
							|| strcmp(entity->GetCompositeFlag().c_str(), "[friendgroup]") == 0)
						{
							DelOneRootEntity(entity->GetCompositeFlag(), entity->GetGUID());
						}
					}

					wsPlug->NextCurSessionStepItr();
					if (wsPlug->IsEndSessionStepItr()) // 本次会话所有步骤结束
					{
						// 回收Session垃圾
						GetGame()->GetSessionFactoryInst()->GarbageCollect_Session(wsPlug->GetExID());
					}
				}
#ifdef _RUNSTACKINFO_
				CMessage::AsyWriteFile(GetGame()->GetStatckFileName(), "ProcessMsg() End.");
#endif
			}
		}
	}

	M_FREE(buf,bufSize);
}



// 添加一个Account到Map末尾
bool CEntityManager::AddAccount(Account* acc)
{
	if(acc)
	{
		string szCdkey = acc->GetName();
		if(szCdkey != "")
		{
			AccountMap::iterator itr = m_mapAccounts.find(szCdkey);
			if(itr != m_mapAccounts.end())
			{
				if(acc != itr->second)
				{
					MP_DELETE(itr->second);
					m_mapAccounts[szCdkey] = acc;
				}
				else
				{
					AddLogText("AddAccount: old ptr[%s] has found.", szCdkey);
				}
			}
			else
			{
				m_mapAccounts[szCdkey] = acc;
			}
			return true;
		}
		else
		{
			AddLogText("AddAccount: szCdkey is NULL.");
		}
	}
	return false;
}
// 删除一个Account
void CEntityManager::DelAccount(Account* acc)
{
	if(acc)
	{
		// 现在vector中查找是否已经存在
		AccountMap::iterator itr = m_mapAccounts.find(acc->GetName());
		if(itr != m_mapAccounts.end())
		{
			MP_DELETE(itr->second);
			m_mapAccounts.erase(itr);
		}
	}
}

Account* CEntityManager::FindAccount(const std::string& strCdkey)
{
	// 现在vector中查找是否已经存在
	AccountMap::iterator itr = m_mapAccounts.find(strCdkey);
	if(itr != m_mapAccounts.end())
	{
		return itr->second;
	}
	return NULL;
}

void CEntityManager::CreateLoadAccountSession(Account* pAcc)
{
	// 检查玩家是否处于会话中
	if(!pAcc) return;

	//##创建会话
	CWorldServerSession* pSession			= (CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_LOAD_ACCOUNT);
		pSession->SendLoadAccountMsg(pAcc);
	}
}
void CEntityManager::CreateSaveAccountSession(Account* pAcc)
{
	CWorldServerSession pSession(6000);
	pSession.SendSaveAccountMsg(pAcc);
}
void CEntityManager::CreateLoadFactionGroupSession(void)
{
	//##创建会话
	CWorldServerSession* pSession			= (CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_LOAD_FACTION);
		pSession->SendLoadFactionGroupMsg();
	}
}
void CEntityManager::CreateLoadUnionGroupSession(void)
{
	//##创建会话
	CWorldServerSession* pSession			= (CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_LOAD_UNION);
		pSession->SendLoadUnionGroupMsg();
	}
}
void CEntityManager::CreateLoadCountrySession(void)
{
	//##创建会话
	CWorldServerSession* pSession			= (CWorldServerSession*)(CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_LOAD_COUNTRY);
		pSession->SendLoadCountryMsg();
	}
}
void CEntityManager::CreateLoadRegionSession(void)
{
	//##创建会话
	CWorldServerSession* pSession			= (CWorldServerSession*)(CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_LOAD_REGION);
		pSession->SendLoadRegionGroupMsg();
	}
}
void CEntityManager::CreateSaveRegionSession(CEntityGroup* pRegion)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendSaveRegionMsg(pRegion);

}
void CEntityManager::CreateCreateRegionSession(CEntityGroup* pRegion)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendCreateRegionMsg(pRegion);
}
void CEntityManager::CreateSaveFactionSession(vector<CEntityGroup*> &vCEntityGroup)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendSaveFactionMsg(vCEntityGroup);
}

void CEntityManager::CreateSaveUnionSession(CEntityGroup* pUnion)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendSaveUnionMsg(pUnion);
}

void CEntityManager::CreateInsertFactionSession(CEntityGroup* pFaction)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendInsertFactionMsg(pFaction);
}

void CEntityManager::CreateInsertUnionSession(CEntityGroup* pUnion)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendInsertUnionMsg(pUnion);
}

void CEntityManager::CreateLoadPhRgnSession(void)
{
	//##创建会话
	CWorldServerSession* pSession			= (CWorldServerSession*)(CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_LOAD_PHRGN);
		pSession->SendLoadPhRgnGroupMsg();
	}
}
void CEntityManager::CreateSavePhRgnSession(CEntityGroup* pPhRgn)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendSavePhRgnMsg(pPhRgn);
}
void CEntityManager::CreateLoadGlobalVarSession(void)
{
	//##创建会话
	CWorldServerSession* pSession			= (CWorldServerSession*)(CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_LOAD_GLOBALVAR);
		pSession->SendLoadGlobalVarGroupMsg();
	}
}
void CEntityManager::CreateSaveGlobalVarSession(void)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendSaveGlobalVarGroupMsg();
}

void CEntityManager::CreateLoadBusinessSession()
{
	CWorldServerSession *pSession = (CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession( 1800000, ST_WS_SESSION );
	if( pSession != NULL && pSession->Start() )
	{
		pSession->AddOneSessionStep( CWorldServerSession::WORLD_SESSION_LOAD_BUSINESS );
		pSession->SendLoadBusinessMsg();
	}
}

void CEntityManager::CreateSaveBusinessSession()
{
	CWorldServerSession pSession(6000);
	pSession.SendSaveBusinessMsg();
}

void CEntityManager::CreateSaveLimitGoodsRecordSession()
{
	CWorldServerSession pSession(6000);
	pSession.SendLimitGoodsRecordUpdateMsg();
}

void CEntityManager::CreateLoadLimitGoodsRecordSession()
{
	CWorldServerSession* pSession=(CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000,ST_WS_SESSION);
	if(pSession!=NULL && pSession->Start())
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_LOAD_GOODS_LIMIT_RECORD);
		pSession->SendLoadLimitGoodsRecordUpdateMsg();
	}
}
BOOL CEntityManager::CreateDelFactionSession(const CGUID &FactionGuid)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	return pSession.SendDelFactionProcMsg(FactionGuid);
}
void CEntityManager::CreateDelUnionSession(const CGUID &UnionGuid)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendDelUnionProcMsg(UnionGuid);
}
// 更新玩家的删除时间
void CEntityManager::CreateUpdateLoginPlayerDelTimeSession(const char* szCdkey, const CGUID& guid)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendUpdateLoginPlayerTimeMsg(szCdkey, guid);
}
// 恢复玩家的删除时间
void CEntityManager::CreateRestoreLoginPlayerDelTimeSession(const char* szCdkey, const CGUID& guid)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendRestoreLoginPlayerTimeMsg(szCdkey, guid);
}
void CEntityManager::CreatePlayerCreateSession(const char* szCdkey, const char* szName, BYTE nOccu, 
											   BYTE sex,BYTE nHead, BYTE nFace, BYTE btCountry, BYTE btMaxCharactersNum,
											   BYTE byConstellation,bool bSelectRecommCountry)
{
	CWorldServerSession* pSession			= (CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_CREATE_PLAYER_CHECK_CONDITION);
		pSession->SendCreatePlayerCheckProcMsg(szCdkey, szName, nOccu, sex, nHead, nFace, btCountry, btMaxCharactersNum,byConstellation,bSelectRecommCountry);
	}
}
void CEntityManager::CreateLoadPlayerDataSession(const char* szCdkey, const CGUID& guid)
{
	if(!szCdkey) return;


	// 要限制其登录次数，避免多次Load操作
	Account* pAcc = FindAccount(szCdkey);
	if(pAcc)
	{
		if(pAcc->GetCurWsSessionStep() == CWorldServerSession::WORLD_SESSION_LOAD_PLAYER)// 已经读取了一次
		{
			return;
		}
	}
	//##创建会话
	CWorldServerSession* pSession			= (CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		// 初始化plug的sessionstep
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_LOAD_PLAYER);
		pSession->SendLoadPlayerDataMsg(szCdkey, guid);
		pAcc->SetCurWsSessionStep(CWorldServerSession::WORLD_SESSION_LOAD_PLAYER);
	}
}
void CEntityManager::CreateLoadLinkmanSession(const CGUID& guid)
{
	//##创建会话
	CWorldServerSession* pSession = (CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		// 初始化plug的sessionstep
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_LOAD_LINKMAN);
		pSession->SendLoadLinkmanMsg(guid);
	}
}
void CEntityManager::CreateSaveLoginPlayerDataSession(const CGUID& guid)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendSaveLoginPlayerDataMsg(guid);
}
void CEntityManager::CreateSavePlayerDataSession(const CGUID& guid, long bitValue/*SAVE_DETAIL_ATTR_TYPE*/)
{
	CPlayer* player = GetGame()->GetMapPlayer(guid);
	if(!player) return;

	string strPlayer = "[player]";
	// 创建一个DBPlayer
	CEntityGroup* pPlayer = (CEntityGroup*)GetGame()->GetEntityManager()->NewBaseEntity(strPlayer, guid);
	if(pPlayer)
	{
		CWorldServerSession pTempSession(5000);
		pTempSession.NakeSendSavePlayerDataMsg(bitValue, player, pPlayer, false);
	}
	GetGame()->GetEntityManager()->DelBaseEntity((CBaseEntity*)pPlayer);
	return;
}

void CEntityManager::CreateInitLoadMailSession(const CGUID& guid)
{
	//##创建会话
	CWorldServerSession* pSession			= (CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_LOAD_PLAYER_MAILS);
		pSession->SendInitLoadMail(guid);
	}
}

// 在线玩家删除信件
void CEntityManager::CreateDeleteOnlineMailSession(const CGUID& ownerID, vector<CMail*> &vMails)
{
	CWorldServerSession pSession(6000);
	pSession.SendDeleteOnlineMailMsg(ownerID, vMails);			
}
void CEntityManager::CreateInsertMailSession(CMail* mail)
{
	//##创建会话
	CWorldServerSession* pSession			= (CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_INSERT_PLAYER_MAIL);
		pSession->SendInsertMailMsg(mail);
	}
}
// 在线玩家保存信件
void CEntityManager::CreateSaveOnlineMailSession(const CGUID& ownerID, vector<CMail*> &mails)
{
	//##创建会话
	CWorldServerSession pSession(6000);
	pSession.SendSaveOnlineMailMsg(ownerID, mails);
}

void CEntityManager::CreateBanPlayerSession(const char* szName, long time)
{
	//##创建会话
	CWorldServerSession* pSession			= (CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_BAN_PLAYER);
		// 记录临时数据
		pSession->TempDataRelease();
		
		// 记录临时数据
		CTempEntityProperty* pTime = new CTempEntityProperty(string("time"), DATA_OBJECT_LONG);
		if(pTime)
		{
			pTime->SetLongAttr(time);
			pSession->GetTempDataVec().push_back(pTime);
		}
		pSession->SendBanPlayerProcMsg(szName, time);
	}
}

//--转换接口
//------------------------------------------------------
void CEntityManager::CDBCountryToCCountry(CBaseEntity* pEntity, CCountry* country)
{
	if(pEntity && country)
	{
		CBaseEntity* pCountry = pEntity;

		country->SetID(pCountry->GetLongAttr(string("id")));
		CGUID tGuid;
		pCountry->GetGuidAttr(string("king"), tGuid);
		country->SetKing(tGuid);
		country->SetPower(pCountry->GetLongAttr(string("power")));
		country->SetTechLevel(pCountry->GetLongAttr(string("tech_level")));
	}
}
void CEntityManager::CCountryToCDBCountry(CCountry* country, CBaseEntity* pEntity)
{
	if(pEntity && country)
	{
		CBaseEntity* pCountry = pEntity;

		pCountry->SetGuidAttr(string("king"),country->GetKing());
		pCountry->SetLongAttr(string("id"),country->GetID());
		pCountry->SetLongAttr(string("power"),country->GetPower());
		pCountry->SetLongAttr(string("tech_level"), country->GetTechLevel());
	}
}
void CEntityManager::CDBRegionToCRegion(CEntityGroup* pBaseEntity, CWorldRegion* pRegion)
{
	if(pBaseEntity && pRegion)
	{
		CEntityGroup* pDbRegion = (CEntityGroup*)pBaseEntity;

		tagRegionParam RegionParam;
		RegionParam.lID					=	pDbRegion->GetLongAttr(string("RegionID"));
		pDbRegion->GetGuidAttr(string("OwnedFactionID"),RegionParam.OwnedFactionID);
		pDbRegion->GetGuidAttr(string("OwnedUnionID"),RegionParam.OwnedUnionID);
		RegionParam.lCurrentTaxRate		=	pDbRegion->GetLongAttr(string("CurTaxRate"));
		RegionParam.dwTodayTotalTax		=	pDbRegion->GetLongAttr(string("TodayTotalTax"));
		RegionParam.dwTotalTax			=	pDbRegion->GetLongAttr(string("TotalTax"));
		RegionParam.dwBusinessTotalTax	=	pDbRegion->GetLongAttr(string("BusinessTotalTax"));
		RegionParam.lBusinessTaxRate	=	pDbRegion->GetLongAttr(string("BusinessTaxRate"));
		RegionParam.lYuanbaoTaxRate		=	pDbRegion->GetLongAttr(string("YuanbaoTaxRate"));
		pRegion->SetRegionParam(RegionParam);
	}
}
void CEntityManager::CRegionToCDBRegion(CWorldRegion* pRegion, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && pRegion)
	{
		CEntityGroup* pDbRegion = (CEntityGroup*)pBaseEntity;

		pDbRegion->SetLongAttr(string("RegionID"),			pRegion->GetRegionParam().lID);
		pDbRegion->SetGuidAttr(string("OwnedFactionID"),	pRegion->GetRegionParam().OwnedFactionID);
		pDbRegion->SetGuidAttr(string("OwnedUnionID"),		pRegion->GetRegionParam().OwnedUnionID);
		pDbRegion->SetLongAttr(string("CurTaxRate"),		pRegion->GetRegionParam().lCurrentTaxRate);
		pDbRegion->SetLongAttr(string("TodayTotalTax"),		pRegion->GetRegionParam().dwTodayTotalTax);
		pDbRegion->SetLongAttr(string("TotalTax"),			pRegion->GetRegionParam().dwTotalTax);
		pDbRegion->SetLongAttr(string("BusinessTotalTax"),	pRegion->GetRegionParam().dwBusinessTotalTax);
		pDbRegion->SetLongAttr(string("BusinessTaxRate"),	pRegion->GetRegionParam().lBusinessTaxRate);
		pDbRegion->SetLongAttr(string("YuanbaoTaxRate"),	pRegion->GetRegionParam().lYuanbaoTaxRate);
	}
}
void CEntityManager::CDBFactionToCFaction(CEntityGroup* pBaseEntity, CFaction* fac)
{
	if(pBaseEntity && fac)
	{
		CEntityGroup* dbFac = (CEntityGroup*)pBaseEntity;

		//! 基本属性表
		CGUID tGuid;
		dbFac->GetGuidAttr(string("FactionGuid"), tGuid);
		fac->SetExID(tGuid);
		fac->SetName(dbFac->GetStringAttr(string("Name")));
		//! 当前使用的技能ID
		fac->GetPublicData().lCurrSkillID = dbFac->GetLongAttr(string("CurrSkillID"));

		//! 工会等级
		fac->GetPublicData().lLevel = dbFac->GetLongAttr(string("Levels"));

		//! 基本信息数据
		fac->GetBaseData().lCountryID = dbFac->GetLongAttr(string("CountryID"));
		fac->GetBaseData().bOpenResWar = dbFac->GetLongAttr(string("OpenResWar"));
		dbFac->GetGuidAttr(string("MasterGuid"), fac->GetBaseData().MasterGuid );
		const char* szMastName = dbFac->GetStringAttr(string("MasterName"));
		if(szMastName)
			strcpy( fac->GetBaseData().szMasterName, szMastName);
		fac->GetBaseData().CreateTime = dbFac->GetLongAttr(string("CreateTime"));
		fac->GetBaseData().lCurrRes = dbFac->GetLongAttr(string("CurrRes"));
		fac->GetBaseData().lCurrExp = dbFac->GetLongAttr(string("CurrExp"));
		fac->GetBaseData().lSubExpLevel = dbFac->GetLongAttr(string("SubjoinExpLevel"));	
		fac->GetBaseData().lSpecialityLevel = dbFac->GetLongAttr(string("DepotLevel"));	
		fac->GetBaseData().lBattleLevel = dbFac->GetLongAttr(string("SmithingLevel"));	
		fac->GetBaseData().lBussinessLevel = dbFac->GetLongAttr(string("MetallurgyLevel"));
		fac->GetBaseData().lNoblelevel = dbFac->GetLongAttr(string("SewingLevel"));
		dbFac->GetGuidAttr(string("SuperiorGuid"), fac->GetBaseData().SuperiorGuid);
		fac->GetBaseData().bIsRecruiting = dbFac->GetLongAttr(string("IsRecruiting"));

		strcpy( fac->GetBaseData().arr_szJobName[0], dbFac->GetStringAttr(string("JobName1")) );
		strcpy( fac->GetBaseData().arr_szJobName[1], dbFac->GetStringAttr(string("JobName2")) );
		strcpy( fac->GetBaseData().arr_szJobName[2], dbFac->GetStringAttr(string("JobName3")) );
		strcpy( fac->GetBaseData().arr_szJobName[3], dbFac->GetStringAttr(string("JobName4")) );
		strcpy( fac->GetBaseData().arr_szJobName[4], dbFac->GetStringAttr(string("JobName5")) );
		fac->GetBaseData().arrJobPurview[0] = dbFac->GetLongAttr(string("JobPurview1"));
		fac->GetBaseData().arrJobPurview[1] = dbFac->GetLongAttr(string("JobPurview2"));
		fac->GetBaseData().arrJobPurview[2] = dbFac->GetLongAttr(string("JobPurview3"));
		fac->GetBaseData().arrJobPurview[3] = dbFac->GetLongAttr(string("JobPurview4"));
		fac->GetBaseData().arrJobPurview[4] = dbFac->GetLongAttr(string("JobPurview5"));

		//! 宣言
		dbFac->GetGuidAttr(string("Pronounce_MemberGuid"), fac->GetPronounceWord().MemberGuid);
		const char* szPMemName = dbFac->GetStringAttr(string("Pronounce_MemberName"));
		if(szPMemName)
			strcpy(fac->GetPronounceWord().szName, szPMemName);
		string strPBody = "Pronounce_Body";
		long lAttrBufSize = dbFac->GetBufSize(strPBody);
		if(lAttrBufSize > 0)
		{
			dbFac->GetBufAttr(strPBody, fac->GetPronounceWord().szBody, lAttrBufSize);
		}
		fac->GetPronounceWord().Time = dbFac->GetLongAttr(string("Pronounce_TimeSpouseParam"));

		dbFac->GetGuidAttr(string("Leaveword_MemberGuid"), fac->GetLeaveword().MemberGuid);
		const char* szLMemName = dbFac->GetStringAttr(string("Leaveword_MemberName"));
		if(szLMemName)
			strcpy(fac->GetLeaveword().szName, szLMemName);
		string strLBody = "Leaveword_Body";
		lAttrBufSize = dbFac->GetBufSize(strLBody);
		if(lAttrBufSize > 0)
		{
			dbFac->GetBufAttr(strLBody, fac->GetLeaveword().szBody, lAttrBufSize);
		}
		
		fac->GetLeaveword().Time = dbFac->GetLongAttr(string("Leaveword_TimeSpouseParam"));

		//! 会徽
		string strIcon = "Icon_Data";
		lAttrBufSize = dbFac->GetBufSize(strIcon);
		if(lAttrBufSize > sizeof(LONG))
		{
			BYTE* IconData = (BYTE*)M_ALLOC(lAttrBufSize);
			dbFac->GetBufAttr(strIcon, IconData, lAttrBufSize);
			fac->SetIcon(IconData, lAttrBufSize);
		}

		fac->m_dwDisbandTime = dbFac->GetLongAttr(string("DisbandTime"));
		fac->m_dwNegativeTime = dbFac->GetLongAttr(string("NegativeTime"));
		fac->m_dwSubResTime = dbFac->GetLongAttr(string("SubResTime"));
		fac->GetBaseData().lFacBattle=dbFac->GetLongAttr(string("Battle"));
		fac->GetBaseData().lFacArmyInvest=dbFac->GetLongAttr(string("ArmyInvest"));
		
		//解码家族科技
		string strTechLv="TechLv";
		lAttrBufSize=dbFac->GetBufSize(strTechLv);
		if (lAttrBufSize>sizeof(LONG))
		{
			BYTE* buf=(BYTE*)M_ALLOC(lAttrBufSize);
			dbFac->GetBufAttr(strTechLv,buf,lAttrBufSize);
			long pos=0;
			fac->DecodeTechLvFromByteArray(buf,pos);
			M_FREE(buf,lAttrBufSize);
		}
		//解码据点信息
		string strBase="BaseInfo";
		lAttrBufSize=dbFac->GetBufSize(strBase);
		if (lAttrBufSize>sizeof(LONG))
		{
			BYTE* buf=(BYTE*)M_ALLOC(lAttrBufSize);
			dbFac->GetBufAttr(strBase,buf,lAttrBufSize);
			long pos=0;
			fac->DecodeBaseInfoFromByteArray(buf,pos);
			M_FREE(buf,lAttrBufSize);
		}

		//! 成员表
		CEntityGroup* tMemberGroup = NULL;
		map<string, CGUID>::iterator memItr = dbFac->GetGuidByComFlagMap().find(string("[factionmembergroup]"));
		if(memItr != dbFac->GetGuidByComFlagMap().end())
		{
			map<CGUID, CBaseEntity*>::iterator enItr = dbFac->GetEntityGroupMap().find(memItr->second);
			if(enItr != dbFac->GetEntityGroupMap().end())
				tMemberGroup = (CEntityGroup*)enItr->second;
		}
		if(tMemberGroup)
		{
			map<CGUID, CBaseEntity*>::iterator memItr = tMemberGroup->GetEntityGroupMap().begin();
			for(; memItr != tMemberGroup->GetEntityGroupMap().end(); memItr++)
			{
				tagFacMemInfo tFacMemInfo;
				//! 成员信息列表
				memItr->second->GetGuidAttr(string("MemberGuid"), tFacMemInfo.MemberGuid);
				strcpy(tFacMemInfo.szName, memItr->second->GetStringAttr(string("Name_Members")));
				strcpy(tFacMemInfo.szTitle, memItr->second->GetStringAttr(string("Title")));
				tFacMemInfo.lLvl = memItr->second->GetLongAttr(string("Levels_Members"));
				tFacMemInfo.lOccu = memItr->second->GetLongAttr(string("Occu_Members"));
				tFacMemInfo.lJobLvl = memItr->second->GetLongAttr(string("JobLvl"));
				tFacMemInfo.LastOnlineTime = memItr->second->GetLongAttr(string("LastOnlineTime"));
				tFacMemInfo.lContribute = memItr->second->GetLongAttr(string("Contribute"));
				tFacMemInfo.lBattle=memItr->second->GetLongAttr(string("Battle"));
				tFacMemInfo.lArmyInvest=memItr->second->GetLongAttr(string("ArmyInvest"));
				fac->GetMemberMap()[tFacMemInfo.MemberGuid] = tFacMemInfo;
			}
		}
		//! 申请加入列表
		CEntityGroup* tApplyGroup = NULL;
		memItr = dbFac->GetGuidByComFlagMap().find(string("[factionapplygroup]"));
		if(memItr != dbFac->GetGuidByComFlagMap().end())
		{
			map<CGUID, CBaseEntity*>::iterator enItr = dbFac->GetEntityGroupMap().find(memItr->second);
			if(enItr != dbFac->GetEntityGroupMap().end())
				tApplyGroup = (CEntityGroup*)enItr->second;
		}
		if(tApplyGroup)
		{
			map<CGUID, CBaseEntity*>::iterator applyItr = tApplyGroup->GetEntityGroupMap().begin();
			for(; applyItr != tApplyGroup->GetEntityGroupMap().end(); applyItr++)
			{
				tagFacApplyPerson tFacAppyPerson;
				//! 成员信息列表
				applyItr->second->GetGuidAttr(string("PlayerGuid"), tFacAppyPerson.PlayerGuid);
				strcpy(tFacAppyPerson.szName, applyItr->second->GetStringAttr(string("Name_Apply")));
				tFacAppyPerson.lLvl = applyItr->second->GetLongAttr(string("Levels_Apply"));
				tFacAppyPerson.lOccu = applyItr->second->GetLongAttr(string("Occu_Apply"));
				tFacAppyPerson.lTime = applyItr->second->GetLongAttr(string("Time"));
				fac->GetApplyMap()[tFacAppyPerson.PlayerGuid] = tFacAppyPerson;
			}
		}
	}
}
void CEntityManager::CFactionToCDBFaction(CFaction* fac, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && fac)
	{
		CEntityGroup* dbFac = (CEntityGroup*)pBaseEntity;

		//! 基本属性表
		{
			dbFac->SetGUID(fac->GetExID());
			dbFac->SetGuidAttr(string("FactionGuid"), fac->GetExID());
			dbFac->SetStringAttr(string("Name"), fac->GetName());
			//! 当前使用的技能ID
			dbFac->SetLongAttr(string("CurrSkillID"), fac->GetPublicData().lCurrSkillID);

			//! 工会等级
			dbFac->SetLongAttr(string("Levels"), fac->GetLvl(eUT_FactionLevel));
			//! 基本信息数据
			dbFac->SetLongAttr(string("CountryID"), fac->GetBaseData().lCountryID);
			dbFac->SetLongAttr(string("OpenResWar"), fac->GetBaseData().bOpenResWar);
			dbFac->SetGuidAttr(string("MasterGuid"), fac->GetBaseData().MasterGuid);
			dbFac->SetStringAttr(string("MasterName"), fac->GetBaseData().szMasterName);
			dbFac->SetLongAttr(string("CreateTime"),  fac->GetBaseData().CreateTime);
			dbFac->SetLongAttr(string("CurrRes"),  fac->GetBaseData().lCurrRes);
			dbFac->SetLongAttr(string("CurrExp"),  fac->GetBaseData().lCurrExp);
			dbFac->SetGuidAttr(string("SuperiorGuid"), fac->GetBaseData().SuperiorGuid);
			dbFac->SetLongAttr(string("IsRecruiting"),  fac->GetBaseData().bIsRecruiting);
			dbFac->SetLongAttr(string("SubjoinExpLevel"),  fac->GetBaseData().lSubExpLevel);
			dbFac->SetLongAttr(string("DepotLevel"),  fac->GetBaseData().lSpecialityLevel);
			dbFac->SetLongAttr(string("SmithingLevel"),  fac->GetBaseData().lBattleLevel);
			dbFac->SetLongAttr(string("MetallurgyLevel"), fac->GetBaseData().lBussinessLevel);
			dbFac->SetLongAttr(string("SewingLevel"),  fac->GetBaseData().lNoblelevel);

			
			dbFac->SetLongAttr(string("JobPurview1"), fac->GetBaseData().arrJobPurview[0]);
			dbFac->SetLongAttr(string("JobPurview2"), fac->GetBaseData().arrJobPurview[1]);
			dbFac->SetLongAttr(string("JobPurview3"), fac->GetBaseData().arrJobPurview[2]);
			dbFac->SetLongAttr(string("JobPurview4"), fac->GetBaseData().arrJobPurview[3]);
			dbFac->SetLongAttr(string("JobPurview5"), fac->GetBaseData().arrJobPurview[4]);
			dbFac->SetStringAttr(string("JobName1"), fac->GetBaseData().arr_szJobName[0]);
			dbFac->SetStringAttr(string("JobName2"), fac->GetBaseData().arr_szJobName[1]);
			dbFac->SetStringAttr(string("JobName3"), fac->GetBaseData().arr_szJobName[2]);
			dbFac->SetStringAttr(string("JobName4"), fac->GetBaseData().arr_szJobName[3]);
			dbFac->SetStringAttr(string("JobName5"), fac->GetBaseData().arr_szJobName[4]);

			//! 宣言
			dbFac->SetGuidAttr(string("Pronounce_MemberGuid"), fac->GetPronounceWord().MemberGuid);
			dbFac->SetStringAttr(string("Pronounce_MemberName"),  fac->GetPronounceWord().szName);
			dbFac->SetBufAttr(string("Pronounce_Body"), &(fac->GetPronounceWord().szBody[0]), MAX_PronounceCharNum);
			dbFac->SetLongAttr(string("Pronounce_TimeSpouseParam"),  fac->GetPronounceWord().Time);

			dbFac->SetGuidAttr(string("Leaveword_MemberGuid"), fac->GetLeaveword().MemberGuid);
			dbFac->SetStringAttr(string("Leaveword_MemberName"),  fac->GetLeaveword().szName);
			dbFac->SetBufAttr(string("Leaveword_Body"), &(fac->GetLeaveword().szBody[0]), MAX_PronounceCharNum);
			dbFac->SetLongAttr(string("Leaveword_TimeSpouseParam"),  fac->GetLeaveword().Time);

			//! 会徽
			vector<BYTE> vecIconData;
			fac->GetIcon(vecIconData);
			if(vecIconData.size())
				dbFac->SetBufAttr(string("Icon_Data"),  &vecIconData[0], vecIconData.size());
			else
			{
				long lbuf = 0;
				dbFac->SetBufAttr(string("Icon_Data"),  (BYTE*)&lbuf, sizeof(LONG));
			}

			dbFac->SetLongAttr(string("DisbandTime"),  fac->m_dwDisbandTime);
			dbFac->SetLongAttr(string("NegativeTime"),fac->m_dwNegativeTime);
			dbFac->SetLongAttr(string("SubResTime"),fac->m_dwSubResTime);
			dbFac->SetLongAttr(string("Battle"),fac->GetBaseData().lFacBattle);
			dbFac->SetLongAttr(string("ArmyInvest"),fac->GetBaseData().lFacArmyInvest);
			//保存家族科技等级
			vector<BYTE>vecInfo;
			fac->AddTechLvToByteArray(&vecInfo);
			dbFac->SetBufAttr(string("TechLv"),(BYTE*)&vecInfo[0],vecInfo.size());
			vecInfo.clear();
			//保存据点信息
			fac->AddBaseInfoToByteArray(&vecInfo);
			dbFac->SetBufAttr(string("BaseInfo"),(BYTE*)&vecInfo[0],vecInfo.size());

		}

		//! 成员表
		{
			SetEntityLeavesDbOperFlag(dbFac, string("[factionmembergroup]"), true);
			CEntityGroup* tMemberGroup = NULL;
			map<string, CGUID>::iterator memItr = dbFac->GetGuidByComFlagMap().find(string("[factionmembergroup]"));
			if(memItr != dbFac->GetGuidByComFlagMap().end())
			{
				map<CGUID, CBaseEntity*>::iterator enItr = dbFac->GetEntityGroupMap().find(memItr->second);
				if(enItr != dbFac->GetEntityGroupMap().end())
					tMemberGroup = (CEntityGroup*)enItr->second;
			}
			if(tMemberGroup)
			{
				tMemberGroup->ReleaseChilds();
				
				map<CGUID, tagFacMemInfo>::iterator memItr = fac->GetMemberMap().begin();
				for(; memItr != fac->GetMemberMap().end(); memItr++)
				{
					//! 成员信息列表
					CEntity* tFacMember = (CEntity*)GetGame()->GetEntityManager()->NewBaseEntity(string("[faction_member]"), memItr->second.MemberGuid);
					tFacMember->SetGuidAttr(string("FactionGuid_members"), fac->GetExID());
					tFacMember->SetGuidAttr(string("MemberGuid"), memItr->second.MemberGuid);
					tFacMember->SetStringAttr(string("Name_Members"), memItr->second.szName);
					tFacMember->SetStringAttr(string("Title"),  memItr->second.szTitle);
					tFacMember->SetLongAttr(string("Levels_Members"),  memItr->second.lLvl);
					tFacMember->SetLongAttr(string("Occu_Members"),  memItr->second.lOccu);
					tFacMember->SetLongAttr(string("JobLvl"), memItr->second.lJobLvl);
					tFacMember->SetLongAttr(string("LastOnlineTime"),  memItr->second.LastOnlineTime);
					tFacMember->SetLongAttr(string("Contribute"), memItr->second.lContribute);
					tFacMember->SetLongAttr(string("Battle"),memItr->second.lBattle);
					tFacMember->SetLongAttr(string("ArmyInvest"),memItr->second.lArmyInvest);
					tMemberGroup->AddChild(tFacMember);
				}
			}
		}


		//! 申请加入列表
		{
			SetEntityLeavesDbOperFlag(dbFac, string("[factionapplygroup]"), true);
			//! 申请列表
			CEntityGroup* tApplyGroup = NULL;
			map<string, CGUID>::iterator memItr = dbFac->GetGuidByComFlagMap().find(string("[factionapplygroup]"));
			if(memItr != dbFac->GetGuidByComFlagMap().end())
			{
				map<CGUID, CBaseEntity*>::iterator enItr = dbFac->GetEntityGroupMap().find(memItr->second);
				if(enItr != dbFac->GetEntityGroupMap().end())
					tApplyGroup = (CEntityGroup*)enItr->second;
			}
			if(tApplyGroup)
			{
				tApplyGroup->ReleaseChilds();

				map<CGUID, tagFacApplyPerson>::iterator applyItr = fac->GetApplyMap().begin();
				for(; applyItr != fac->GetApplyMap().end(); applyItr++)
				{
					//! 成员信息列表
					CEntity* tFacApply = (CEntity*)GetGame()->GetEntityManager()->NewBaseEntity(string("[faction_apply]"),applyItr->second.PlayerGuid);
					tFacApply->SetGuidAttr(string("FactionGuid_Apply"), fac->GetExID());
					tFacApply->SetGuidAttr(string("PlayerGuid"), applyItr->second.PlayerGuid);
					tFacApply->SetStringAttr(string("Name_Apply"),  applyItr->second.szName);
					tFacApply->SetLongAttr(string("Levels_Apply"),  applyItr->second.lLvl);
					tFacApply->SetLongAttr(string("Occu_Apply"),  applyItr->second.lOccu);
					tFacApply->SetLongAttr(string("Time"),  applyItr->second.lTime);
					tApplyGroup->AddChild(tFacApply);
				}
			}
		}


	}
}

void CEntityManager::CUnionToCDBUnion(CUnion* pUnion, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && pUnion)
	{
		CEntityGroup* dbUnion = (CEntityGroup*)pBaseEntity;

		//! 基本属性表
		{
			const tagUnionBaseData& UnionBaseData = pUnion->GetUnionBaseData();

			dbUnion->SetGUID(UnionBaseData.guid);
			dbUnion->SetGuidAttr(string("UnionGuid"),		UnionBaseData.guid);
			dbUnion->SetStringAttr(string("Name"),			UnionBaseData.szName);
			dbUnion->SetLongAttr(string("CountryID"),		UnionBaseData.lCountryID);
			dbUnion->SetGuidAttr(string("MasterGuid"),		UnionBaseData.MasterGuid);
			dbUnion->SetStringAttr(string("MasterName"),	UnionBaseData.szMasterName);
			dbUnion->SetGuidAttr(string("MasterFacGuid"),	UnionBaseData.MasterFacGuid);
			dbUnion->SetLongAttr(string("CreateTime"),		UnionBaseData.CreateTime);

		}
		//! 成员表
		{
			SetEntityLeavesDbOperFlag(dbUnion, string("[union_member_group]"), true);
			CEntityGroup* tMemberGroup = (CEntityGroup*)FindLeafByComFlag(dbUnion, string("[union_member_group]"));
		
			if(tMemberGroup)
			{
				tMemberGroup->ReleaseChilds();

				const	map<CGUID, tagUnionMemInfo>& mapMember = pUnion->GetUnionMemInfoMap();
				map<CGUID, tagUnionMemInfo>::const_iterator memItr = mapMember.begin();
				for(; memItr != mapMember.end(); memItr++)
				{
					//! 成员信息列表
					CEntity* tFacMember = (CEntity*)GetGame()->GetEntityManager()->NewBaseEntity(string("[union_member]"), memItr->second.MemberGuid);
					
					tFacMember->SetGuidAttr(string("UnionGuid"),		dbUnion->GetGUID());
					tFacMember->SetGuidAttr(string("MemberGuid"),		memItr->second.MemberGuid);
					tFacMember->SetStringAttr(string("NameMembers"),	memItr->second.szName);
					tFacMember->SetLongAttr(string("JoinDate"),			memItr->second.lJoinDate);
					tFacMember->SetLongAttr(string("Purview"),			memItr->second.lPurview);
		
					tMemberGroup->AddChild(tFacMember);
				}
			}
		}
	}
}
void CEntityManager::CDBUnionToCUnion(CEntityGroup* pBaseEntity, CUnion* pUnion)
{
	if(pBaseEntity && pUnion)
	{
		CEntityGroup* dbUnion = (CEntityGroup*)pBaseEntity;

		//! 基本属性表
		tagUnionBaseData &UnionBaseData = pUnion->GetUnionBaseData();
		
		dbUnion->GetGuidAttr(string("UnionGuid"), UnionBaseData.guid);
		pUnion->SetName(dbUnion->GetStringAttr(string("Name")));
		UnionBaseData.lCountryID = dbUnion->GetLongAttr(string("CountryID"));
		dbUnion->GetGuidAttr(string("MasterGuid"), UnionBaseData.MasterGuid);
		dbUnion->GetGuidAttr(string("MasterFacGuid"), UnionBaseData.MasterFacGuid);
		UnionBaseData.CreateTime = dbUnion->GetLongAttr(string("CreateTime"));
		pUnion->SetMasterName(dbUnion->GetStringAttr(string("MasterName")));

		//! 成员表
		CEntityGroup* tMemberGroup = (CEntityGroup*)FindLeafByComFlag(dbUnion, string("[union_member_group]"));

		if(tMemberGroup)
		{
			map<CGUID, CBaseEntity*>::iterator memItr = tMemberGroup->GetEntityGroupMap().begin();
			for(; memItr != tMemberGroup->GetEntityGroupMap().end(); memItr++)
			{
				tagUnionMemInfo tUnionMemInfo;
				//! 成员信息列表
				memItr->second->GetGuidAttr(string("MemberGuid"), tUnionMemInfo.MemberGuid);
				tUnionMemInfo.lJoinDate = memItr->second->GetLongAttr(string("JoinDate"));
				tUnionMemInfo.lPurview = memItr->second->GetLongAttr(string("Purview"));
				const char *pNameMembers = memItr->second->GetStringAttr(string("NameMembers"));
				if(NULL != pNameMembers)
				{
					LONG lSize = strlen(pNameMembers);
					lSize = (MAX_MEMBER_NAME_SIZE <= lSize) ? MAX_MEMBER_NAME_SIZE - 1 : lSize;
					memmove(tUnionMemInfo.szName, pNameMembers, lSize);
					tUnionMemInfo.szName[lSize] = 0;
				}
				
				pUnion->GetUnionMemInfoMap()[tUnionMemInfo.MemberGuid] = tUnionMemInfo;
			}
		}
	}
}




void CEntityManager::DetailSysMailCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		string strBuf = "SysMailList";
		long lAttrBufSize = dbPlayer->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* buf = (BYTE*)M_ALLOC(lAttrBufSize);
			dbPlayer->GetBufAttr(strBuf, buf, lAttrBufSize);
			map<long,long>& psl = pPlayer->GetSysMail();
			psl.clear();
			long pos = 0;
			long lNum = _GetLongFromByteArray(buf,pos); 
			for(int i=0; i<lNum; i++)
			{
				long lID = _GetLongFromByteArray(buf,pos);
				long lEndTime = _GetLongFromByteArray(buf,pos);
				pPlayer->AddSysMail(lID,lEndTime);
			}
			M_FREE(buf,lAttrBufSize);
		}
	}
}
void CEntityManager::CDBMailToCMail(CEntityGroup* pDBMail, CMail* pMail)
{
	if(pMail && pDBMail)
	{
		CGUID ownerID;
		CGUID guid;
		CGUID WriterGuid;
		pDBMail->GetGuidAttr(string("guid"), guid);
		pMail->SetExID(guid);
		pDBMail->GetGuidAttr(string("PlayerID"),  ownerID);
		pDBMail->GetGuidAttr(string("WriterID"),WriterGuid);
		pMail->SetWriterID(WriterGuid);
		CPlayer *pPlayer = GetGame()->GetMapPlayer(ownerID);

		{
			
			pMail->SetType(pDBMail->GetLongAttr(string("type")));
			pMail->SetSubject(pDBMail->GetStringAttr(string("Subject")));
			pMail->SetWriter(pDBMail->GetStringAttr(string("Writer")));		

			pMail->SetText(pDBMail->GetStringAttr(string("Text")));
			pMail->SetExText(pDBMail->GetStringAttr(string("ExText")));

			pMail->SetRemainTime(pDBMail->GetLongAttr(string("RemainTime")));
			pMail->SetGold(pDBMail->GetLongAttr(string("Gold")));
			pMail->SetRead(pDBMail->GetLongAttr(string("ReadFlag")));
			pMail->SetReject(pDBMail->GetLongAttr(string("Reject")));
			pMail->SetReceiver(pDBMail->GetStringAttr(string("Receiver")));
			pMail->SetReceiverExID(ownerID);
			

			string strBuf = "IncGoods";
			long lAttrBufSize = pDBMail->GetBufSize(strBuf);
			if(lAttrBufSize > 0)
			{
				unsigned char* buf = (BYTE*)M_ALLOC(lAttrBufSize);
				pDBMail->GetBufAttr(strBuf, buf, lAttrBufSize);
				long pos=0;
				long lNum = _GetLongFromByteArray(buf,pos);
				for(int i=0; i<lNum; i++)
				{
					tagSGoods *ptgSGoods = OBJ_CREATE(tagSGoods);
					ptgSGoods->lIndex = _GetLongFromByteArray(buf,pos);
					ptgSGoods->lNum   = _GetLongFromByteArray(buf,pos);
					pMail->GetSGood().push_back(ptgSGoods);
				}
				M_FREE(buf,lAttrBufSize);
			}

			// 物品转换
			pMail->ClearMailGoodsContainer();
			CEntityGroup* goodsGroup = (CEntityGroup*)FindLeafByComFlag(pDBMail, string("[mailgoodsgroup]"));
			if(goodsGroup)
			{
				map<CGUID, CBaseEntity*>::iterator goodsItr = goodsGroup->GetEntityGroupMap().begin();
				for(; goodsItr!=goodsGroup->GetEntityGroupMap().end(); goodsItr++)
				{
	#ifdef __MEMORY_LEAK_CHECK__
					CBaseObject::SetConstructFlag(37);
	#endif
					CGoods* pGoods = CGoodsFactory::CreateGoods((goodsItr->second)->GetLongAttr(string("goodsIndex")),2);
					if(pGoods)
					{
						pGoods->SetExID(const_cast<CGUID&>(goodsItr->first));
					CDBMailGoodsToCGoods(goodsItr->second, pGoods, ownerID);
					pMail->GetMGoodsContainer().push_back(pGoods);
				}
			}
		}
	}
	}
}
// 邮件物品对象转换
void CEntityManager::CDBMailGoodsToCGoods(CBaseEntity* dbGood, CGoods* good, const CGUID& ownerID)
{
	CDBGoodToCGoods(dbGood, good, ownerID);
}


void CEntityManager::DetailSysMailCplayerToCDBPlayer	(CPlayer* pPlayer, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;
		map<long,long> &psl = pPlayer->GetSysMail();
		long lSysNum = psl.size();
		long buffsize = (lSysNum*2 + 1)*sizeof(long);
		BYTE* buf = (BYTE*)M_ALLOC(buffsize);	

		vector<BYTE> vSysBuf;
		_AddToByteArray(&vSysBuf,(long)(lSysNum));
		map<long,long>::iterator itr = psl.begin();

		//long num = 0;
		for(; itr != psl.end(); itr++)
		{
			_AddToByteArray(&vSysBuf,(*itr).first);
			_AddToByteArray(&vSysBuf,(*itr).second);				
		}
		for (int i=0;i<buffsize;++i)
		{
			buf[i] = vSysBuf[i];
		}
		dbPlayer->SetBufAttr(string("SysMailList"), buf, (long)buffsize);
		M_FREE(buf,buffsize);

	}
}

void CEntityManager::CLimitGoodsRecordToCDBRecord(CEntityGroup* pEntity)
{
	if( pEntity == NULL )
	{
		return;
	}
	pEntity->ReleaseChilds();

	// encode
	map<DWORD,DWORD>::iterator recordIt=CIncrementShopManager::GetSingleton().GetLimitGoodsMap().begin();
	for(;recordIt!=CIncrementShopManager::GetSingleton().GetLimitGoodsMap().end();recordIt++)
	{
		CGUID guid;
		CGUID::CreateGUID( guid );
		CEntity *entity_child = (CEntity*)GetGame()->GetEntityManager()->NewBaseEntity( "[limit_goods_record]", guid );
		
		entity_child->SetLongAttr( "goods_id",recordIt->first);
		
		entity_child->SetLongAttr( "goods_num",recordIt->second);

		pEntity->AddChild( entity_child );
	}
}

void CEntityManager::CDBLimitGoodsRecordToCRecord(CEntityGroup* pEntity)
{

}
// 邮件对象转化
void CEntityManager::CMailToCDBMail(CMail* pMail, CEntityGroup* pDBMail)
{
	if(pDBMail->GetCompositeType() != COM_COMPOSITE) return;

	if(pMail && pDBMail)
	{
		pDBMail->SetGUID(pMail->GetExID());
		pDBMail->SetGuidAttr(string("PlayerID"),	pMail->GetReceiverExID());
		pDBMail->SetGuidAttr(string("guid"),		pMail->GetExID());
		pDBMail->SetLongAttr(string("type"),		pMail->GetType());
		pDBMail->SetStringAttr(string("Subject"),	pMail->GetSubject().c_str());
		pDBMail->SetStringAttr(string("Writer"),	pMail->GetWriter().c_str());
		pDBMail->SetStringAttr(string("Text"),		pMail->GetText().c_str());
		pDBMail->SetStringAttr(string("ExText"),	pMail->GetExText().c_str());
		pDBMail->SetLongAttr(string("RemainTime"),	pMail->GetRemainTime());	
		pDBMail->SetLongAttr(string("Gold"),		pMail->GetGold());
		pDBMail->SetLongAttr(string("ReadFlag"),	pMail->GetRead());
		pDBMail->SetLongAttr(string("Reject"),		pMail->GetReject());
		pDBMail->SetStringAttr(string("Receiver"),	pMail->GetReceiver().c_str());
		pDBMail->SetGuidAttr(string("WriterID"),	pMail->GetWriterExID());
		

		long IncGoodsNum = 0;
		long incGoodsBufSize = pMail->GetSGood().size() * sizeof(tagSGoods)+sizeof(long);
		BYTE* incGoodsBuf = (BYTE*)M_ALLOC(incGoodsBufSize);
		
		vector<BYTE> vGoodsBuf;
		list<tagSGoods*>::iterator IncGoodsItr = pMail->GetSGood().begin();

		_AddToByteArray(&vGoodsBuf,(long)(pMail->GetSGood().size()));
		for(; IncGoodsItr != pMail->GetSGood().end(); IncGoodsItr++)
		{
			_AddToByteArray(&vGoodsBuf,(*IncGoodsItr)->lIndex);
			_AddToByteArray(&vGoodsBuf,(*IncGoodsItr)->lNum);			
		}
		for (int i=0; i<incGoodsBufSize; ++i)
		{
			incGoodsBuf[i]=vGoodsBuf[i];		
		}		

		pDBMail->SetBufAttr(string("IncGoods"), incGoodsBuf, incGoodsBufSize);
		M_FREE(incGoodsBuf,incGoodsBufSize);

		// 物品转换
		((CEntityGroup*)pDBMail)->ClearLeafChilds((((CEntityGroup*)pDBMail)->GetEntityGroupMap()).begin()->first);
		CEntityGroup* pGoodsGroup = (CEntityGroup*)((((CEntityGroup*)pDBMail)->GetEntityGroupMap()).begin()->second);
		vector<CGoods*>::iterator goodsItr = pMail->GetMGoodsContainer().begin();
		for(; goodsItr!=pMail->GetMGoodsContainer().end(); goodsItr++)
		{
			CEntity* pDBMailGoods = (CEntity*)GetGame()->GetEntityManager()->NewBaseEntity(string("[mailgoods]"), (*goodsItr)->GetExID());
			CGoodsToCDBMailGoods(*goodsItr, pDBMailGoods, pMail->GetExID());
			pGoodsGroup->AddChild(pDBMailGoods);
		}
	}
}
// 邮件物品对象转换
void CEntityManager::CGoodsToCDBMailGoods(CGoods* goods, CBaseEntity* pDBGoods, const CGUID& ownerID)
{
	if(!goods || !pDBGoods) return;

	CBaseEntity* pGoods = pDBGoods;
	pGoods->SetGuidAttr(string("goodsID"), goods->GetExID());
	pGoods->SetLongAttr(string("goodsIndex"),  goods->GetBasePropertiesIndex());
	pGoods->SetGuidAttr(string("MailID"),  ownerID);
	pGoods->SetStringAttr(string("name"),  goods->GetName());
	pGoods->SetLongAttr(string("price"),  goods->GetPrice());
	pGoods->SetLongAttr(string("amount"),  goods->GetAmount());	

	vector<BYTE> m_vecAddonProperty;
	vector<CGoods::tagAddonProperty>& vecAddonProperty=goods->GetAllAddonProperties();
	CGoodsBaseProperties* pBaseProperty=CGoodsFactory::QueryGoodsBaseProperties(goods->GetBasePropertiesIndex());
	if(!pBaseProperty)
		return;
	DWORD dwAddonNum=0;
	DWORD dwPropertyId=0;
	vector<BYTE> vecAddonData;
	for( size_t i = 0; i < vecAddonProperty.size(); i ++ )
	{		
		if(pBaseProperty->IsHasAddonPropertie(vecAddonProperty[i].gapType))	
		{	
			dwPropertyId=vecAddonProperty[i].gapType;
			if(CGoodsFactory::s_GoodsAttrDBSetup[dwPropertyId][0]==1)
			{			
				dwAddonNum++;
				_AddToByteArray(&vecAddonData,(DWORD)vecAddonProperty[i].gapType);
				_AddToByteArray(&vecAddonData,vecAddonProperty[i].lValues[0]);
				_AddToByteArray(&vecAddonData,vecAddonProperty[i].lValues[1]);
			}
		}
		else
		{
			dwAddonNum++;
			_AddToByteArray(&vecAddonData,(DWORD)vecAddonProperty[i].gapType);
			_AddToByteArray(&vecAddonData,vecAddonProperty[i].lValues[0]);
			_AddToByteArray(&vecAddonData,vecAddonProperty[i].lValues[1]);
		}
	}
	_AddToByteArray( &m_vecAddonProperty,dwAddonNum);	

	if(vecAddonData.size()>0)
	{
		_AddToByteArray(&m_vecAddonProperty,&vecAddonData[0],vecAddonData.size());
	}
	_AddToByteArray(&m_vecAddonProperty,goods->GetMakerName());

	//enchase data
	DWORD dwMaxHole=goods->GetMaxEnchaseHoleNum();
	_AddToByteArray(&m_vecAddonProperty,dwMaxHole);
	if(dwMaxHole>0)
	{
		LONG* pHoleData=goods->GetEnchaseHoleData();
		if(pHoleData)
		{
			_AddToByteArray(&m_vecAddonProperty,pHoleData[0]);
			_AddToByteArray(&m_vecAddonProperty,pHoleData[1]);
			_AddToByteArray(&m_vecAddonProperty,pHoleData[2]);
			_AddToByteArray(&m_vecAddonProperty,pHoleData[3]);
			_AddToByteArray(&m_vecAddonProperty,pHoleData[4]);

		}
		M_FREE(pHoleData,5*sizeof(long));
	}

	pGoods->SetBufAttr(string("AddonProperty"),&m_vecAddonProperty[0],m_vecAddonProperty.size());
}
void CEntityManager::CDBGlobalVarGroupToCGlobalVar(CEntityGroup* pBaseEntity)
{
	if(!pBaseEntity) return;

	CVariableList *pVarList = GetGame()->GetGeneralVariableList();
	if (!pVarList) return;

	CEntityGroup* pGenVarGroup = (CEntityGroup*)pBaseEntity;

	//vl->Release();
	map<CGUID, CBaseEntity*>::iterator itr = pGenVarGroup->GetEntityGroupMap().begin();
	for(; itr != pGenVarGroup->GetEntityGroupMap().end(); itr++)
	{
		char* pStr = (char*)(itr->second)->GetStringAttr(string("varName"));
		if(pStr)
		{
			if(pStr[0] == '#') // 字符串
			{
				char* pValue = (char*)(itr->second)->GetStringAttr(string("CValue"));
				if(pValue)
					pVarList->AddVar(pStr, pValue);
			}
			else if(pStr[0] == '$') // 数组
			{
				char* pValue = (char*)(itr->second)->GetStringAttr(string("CValue"));
				if(pValue)
				{
					int pos = 0;
					int arrayPos = 0;
					char value[64];
					memset(value, 0, sizeof(value));
					int tmpPos = 0;
					// 先查找是否是数组
					bool isArray = false;
					while(pValue[pos] != '\0')
					{
						if(pValue[pos] == ',')
						{
							isArray = true;
							break;
						}
						pos++;
					}
					pos = 0;
					if(isArray)
					{
						// 统计数组长度
						while(pValue[pos] != '\0')
						{
							if(pValue[pos] == ',')
							{
								arrayPos++;
							}
							pos++;
						}

						pVarList->AddVar(pStr, arrayPos, 0.0f);

						// 为数组赋值
						pos = 0;
						arrayPos = 0;
						while(pValue[pos] != '\0')
						{
							if(pValue[pos] != ',')
							{
								value[tmpPos] = pValue[pos];
								tmpPos++;
							}
							else if(pValue[pos] == ',')
							{
								pVarList->SetVarValue(pStr, arrayPos, atof(value));
								arrayPos++;
								memset(value, 0, sizeof(value));
								tmpPos = 0;
							}
							pos++;
						}
					}
					else
					{
						pVarList->AddVar(pStr, atof(pValue));
					}
				}
			}
			else if(pStr[0] == '@') // 数组
			{
				char* pValue = (char*)(itr->second)->GetStringAttr(string("CValue"));
				if(pValue)
				{
					pVarList->AddGuid(pStr, CGUID(pValue));
				}
			}
		}
	}
}
void CEntityManager::CGlobalVarGroupToCGDBlobalVar(CEntityGroup* pBaseEntity)
{
	if(!pBaseEntity) return;

	CVariableList *pVarList = GetGame()->GetGeneralVariableList();
	if (!pVarList) return;

	CEntityGroup* pGenVarGroup = (CEntityGroup*)pBaseEntity;
	pGenVarGroup->ReleaseChilds();

	string tblName = "CSL_SCRIPTVAR";
	for(int i=0; i<pVarList->GetVarNum(); i++)
	{
		CGUID tGuid;
		CGUID::CreateGUID(tGuid);
		CEntity* pGenVar = (CEntity*)GetGame()->GetEntityManager()->NewBaseEntity(string("[scriptvar]"), tGuid);
		pGenVar->SetStringAttr(string("varName"), pVarList->GetVarList()[i]->Name);
		if(pVarList->GetVarList()[i]->Array < 0) // 字符串
		{
			pGenVar->SetStringAttr(string("SValue"), pVarList->GetVarList()[i]->strValue);
			pGenVar->SetStringAttr(string("CValue"), pVarList->GetVarList()[i]->strValue);
		}
		else if(pVarList->GetVarList()[i]->Array > 0) // 数组
		{
			char* szArray = (char*)M_ALLOC(64*pVarList->GetVarList()[i]->Array);

			memset(szArray, 0, 64*pVarList->GetVarList()[i]->Array);
			for(int j=0; j<pVarList->GetVarList()[i]->Array; j++)
			{
				char szTmp[128];
				gcvt(*(pVarList->GetVarList()[i]->Value), 10, szTmp);
				strcat(szArray, szTmp);
				strcat(szArray, ",");
			}
			pGenVar->SetStringAttr(string("SValue"), szArray);

			memset(szArray, 0, 64*pVarList->GetVarList()[i]->Array);
			for(int m=0; m<pVarList->GetVarList()[i]->Array; m++)
			{
				char szTmp[128];
				gcvt((pVarList->GetVarList()[i]->Value[m]), 10, szTmp);
				strcat(szArray, szTmp);
				strcat(szArray, ",");
			}
			pGenVar->SetStringAttr(string("CValue"), szArray);
			M_FREE(szArray,64*pVarList->GetVarList()[i]->Array);
		}
		else if(pVarList->GetVarList()[i]->Array == 0) // 整数
		{
			char szTmp[128];
			gcvt(*(pVarList->GetVarList()[i]->Value), 10, szTmp);
			pGenVar->SetStringAttr(string("SValue"),  szTmp);
			gcvt(*(pVarList->GetVarList()[i]->Value), 10, szTmp);
			pGenVar->SetStringAttr(string("CValue"),  szTmp);
		}
		pGenVarGroup->AddChild(pGenVar);
	}

	CVariableList::GuidList::iterator itr = pVarList->GetGuidList().begin();
	for(; itr!=pVarList->GetGuidList().end(); itr++)
	{
		CGUID tGuid;
		CGUID::CreateGUID(tGuid);
		CEntity* pGenVar = (CEntity*)GetGame()->GetEntityManager()->NewBaseEntity(string("[scriptvar]"), tGuid);
		pGenVar->SetStringAttr(string("varName"),  itr->first.c_str());
		char szGuid[128];
		itr->second->tostring(szGuid);
		pGenVar->SetStringAttr(string("SValue"), szGuid);
		pGenVar->SetStringAttr(string("CValue"), szGuid);
		pGenVarGroup->AddChild(pGenVar);
	}
}
void CEntityManager::CDBPhRgnToCPhRgn(CEntityGroup* pBaseEntity, CWorldRegion* pRegion)
{
	// 从DBDupRgn中找相应CWorldRegion对象的CDBDupRgn对象
	DBEntityComponentMapItr dupRgnMapItr = GetBaseEntityMap().find(string("[phrgn]"));
	if(dupRgnMapItr != GetBaseEntityMap().end())
	{
		map<CGUID, CEntityGroup*>::iterator rgnItr = dupRgnMapItr->second.begin();
		for(; rgnItr != dupRgnMapItr->second.end(); rgnItr++)
		{
			CWorldRegion* wRgn = GetGame()->GetGlobalRgnManager()->FindRgnByGUID(rgnItr->second->GetGUID());
			if(!wRgn)
			{
				wRgn = MP_NEW CWorldRegion;
				GetGame()->GetGlobalRgnManager()->GetRgnMap()[rgnItr->second->GetGUID()] = wRgn;
			}

			if(wRgn)
			{
				wRgn->SetExID(rgnItr->second->GetGUID());
				wRgn->SetID(rgnItr->second->GetLongAttr(string("TemplateRgnID")));
				wRgn->SetResourceID(rgnItr->second->GetLongAttr(string("ResourceID")));
				CGUID oGUID;
				rgnItr->second->GetGuidAttr(string("OwnerGUID"),oGUID);
				wRgn->SetOwnerGUID(oGUID);
				wRgn->SetRgnType((long)RGN_PERSONAL_HOUSE);
				CPlayer* owner = GetGame()->GetMapPlayer(oGUID);
				if(owner)
					owner->SetPersonalHouseRgnGUID(wRgn->GetExID());
			}

			wRgn->SetGsid( rgnItr->second->GetLongAttr(string("GSID")) );// GSID
			wRgn->SetName( (char*)rgnItr->second->GetStringAttr(string("name")) );
			
			string strBuf = "VarList";
			long lAttrBufSize = rgnItr->second->GetBufSize(strBuf);
			if(lAttrBufSize > 0)
			{
				CVariableList* tmpVarList = MP_NEW CVariableList;
				BYTE* buf = (BYTE*)M_ALLOC(lAttrBufSize);
				rgnItr->second->GetBufAttr(strBuf, buf, lAttrBufSize);
				long bufPos = 0;
				tmpVarList->DecordFromByteArray(buf, bufPos, lAttrBufSize);
				wRgn->SetVariableList(tmpVarList);
				M_FREE(buf,lAttrBufSize);
			}
		}
	}
}
void CEntityManager::CPhRgnToCDBPhRgn(CWorldRegion* pRegion, CEntityGroup* pBaseEntity)
{	
	pBaseEntity->SetGUID(pRegion->GetExID());
	pBaseEntity->SetLongAttr(string("TemplateRgnID"),  pRegion->GetID());
	pBaseEntity->SetLongAttr(string("ResourceID"), pRegion->GetResourceID());

	pBaseEntity->SetLongAttr(string("GSID"),  pRegion->GetGsid() );// GSID
	pBaseEntity->SetStringAttr(string("name"),  pRegion->GetName());

	vector<BYTE> pBA;
	pRegion->GetVariableList()->AddToByteArray(&pBA);
	pBaseEntity->SetBufAttr(string("VarList"),  &pBA[0], (long)pBA.size());
}
void CEntityManager::CDBPhRgnGroupToCPhRgnGroup(CEntityGroup* pPhRgnGroup)
{
	if(!pPhRgnGroup) return;
	map<CGUID, CBaseEntity*>::iterator rgnItr = pPhRgnGroup->GetEntityGroupMap().begin();
	for(; rgnItr != pPhRgnGroup->GetEntityGroupMap().end(); rgnItr++)
	{
		CWorldRegion* pPhRgn = GetGame()->GetGlobalRgnManager()->FindRgnByGUID(rgnItr->first);
		if(pPhRgn)
			CDBPhRgnToCPhRgn((CEntityGroup*)rgnItr->second, pPhRgn);
		else
		{
			pPhRgn = MP_NEW CWorldRegion;
			CDBPhRgnToCPhRgn((CEntityGroup*)rgnItr->second, pPhRgn);
			pPhRgn->SetResourceID(pPhRgn->GetID());
			//pPhRgn->Load();
		}
	}
}
void CEntityManager::CPhRgnGroupToCDBPhRgnGroup(CEntityGroup* pPhRgnGroup)
{
	if(!pPhRgnGroup) return;
	// 先清除db对象
	pPhRgnGroup->ReleaseChilds();
	CGlobalRgnManager::MapRgnItr rgnItr = GetGame()->GetGlobalRgnManager()->GetRgnMap().begin();
	for(; rgnItr != GetGame()->GetGlobalRgnManager()->GetRgnMap().end(); rgnItr++)
	{
		if((eRgnType)rgnItr->second->GetRgnType() == RGN_PERSONAL_HOUSE) // 是个人房屋
		{
			CEntityGroup* pDbRgn = (CEntityGroup*)GetGame()->GetEntityManager()->NewBaseEntity(string("[phrgn]"), rgnItr->second->GetExID());
			CPhRgnToCDBPhRgn(rgnItr->second, pDbRgn);
			pPhRgnGroup->AddChild(pDbRgn);
		}
	}
}
void CEntityManager::CDBLoginPlayerToCPlayer(CEntityGroup* pEntity, CPlayer* player)
{
	if(pEntity && player)
	{
		if(pEntity->GetGUID() == NULL_GUID)
		{
			AddLogText("CDBLoginPlayerToCPlayer LoginPlayer ID NULL, name[%s].", player->GetName());
			pEntity->SetGUID(player->GetExID());
		}
		if(pEntity->GetGUID() != player->GetExID())
		{
			char szDBID[128];
			char szCID[128];
			pEntity->GetGUID().tostring(szDBID);
			player->GetExID().tostring(szCID);
			AddLogText("CDBLoginPlayerToCPlayer LoginPlayer DBID[%s] is Error, CID[%s].", szDBID, szCID);
		}

		CEntityGroup* dbLPlayer = (CEntityGroup*)pEntity;
		player->SetName(dbLPlayer->GetStringAttr(string("Name")));
		player->SetAccount(dbLPlayer->GetStringAttr(string("Account")));
		player->SetLevel(dbLPlayer->GetLongAttr(string("Levels")));
		player->SetOccupation(dbLPlayer->GetLongAttr(string("Occupation")));
		player->SetSex(dbLPlayer->GetLongAttr(string("Sex")));
		player->SetCountry(dbLPlayer->GetLongAttr(string("Country")));
		player->SetRegionID(dbLPlayer->GetLongAttr(string("Region")));

		player->SetHeadPic(dbLPlayer->GetLongAttr(string("HEAD")));
		player->SetFacePic(dbLPlayer->GetLongAttr(string("FACE")));

		//player->SetCreateNo(dbLPlayer->GetLongAttr(string("CreateNo")));
		player->SetLastExitGameTime(dbLPlayer->GetLongAttr(string("LogoutTime")));
	}
}
void CEntityManager::CPlayerToCDBLoginPlayer(CPlayer* pPlayer, CEntityGroup* pEntity)
{
	if(pEntity && pPlayer)
	{
		CEntityGroup* dbLPlayer = (CEntityGroup*)pEntity;
		string tableName = "csl_player_base";
		dbLPlayer->SetName(pPlayer->GetName());
		dbLPlayer->SetGuidAttr(string("guid"), pPlayer->GetExID());
		dbLPlayer->SetStringAttr(string("Name"), pPlayer->GetName());
		dbLPlayer->SetStringAttr(string("Account"), pPlayer->GetAccount());
		dbLPlayer->SetLongAttr(string("Levels"), pPlayer->GetLevel());
		dbLPlayer->SetLongAttr(string("Occupation"), pPlayer->GetOccupation());
		dbLPlayer->SetLongAttr(string("Sex"), pPlayer->GetSex());
		dbLPlayer->SetLongAttr(string("Country"), pPlayer->GetCountry());
		dbLPlayer->SetLongAttr(string("Region"), pPlayer->GetRegionID());
		//dbLPlayer->SetLongAttr(string("CreateNo"), pPlayer->GetCreateNo());
		dbLPlayer->SetLongAttr(string("LogoutTime"), pPlayer->GetLastExitGameTime());

		//	头发
		dbLPlayer->SetLongAttr(string("HEAD"), pPlayer->GetHeadPic());

		//	头发
		dbLPlayer->SetLongAttr(string("HAIR"), pPlayer->GetHeadPic());
		//	头发
		dbLPlayer->SetLongAttr(string("FACE"), pPlayer->GetFacePic());

		BYTE dwNull = 0;
		CGoods* pGoods = NULL;
		// ----------------------------
		//	1：头盔
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_HEAD );
		// ID
		dbLPlayer->SetLongAttr(string("HeadLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	2：项链
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_NECKLACE );
		dbLPlayer->SetLongAttr(string("NECKLACE"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("NecklaceLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	3：翅膀
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_WING );
		dbLPlayer->SetLongAttr(string("WING"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("WingLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	4：盔甲
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_BODY );
		dbLPlayer->SetLongAttr(string("BODY"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("BodyLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	5：腰带
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_BACK );
		dbLPlayer->SetLongAttr(string("BACK"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("BackLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	6：手套	
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_GLOVE );
		dbLPlayer->SetLongAttr(string("GLOVE"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("GloveLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	7：鞋子
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_BOOT );
		dbLPlayer->SetLongAttr(string("BOOT"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("BootLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	8：头饰
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_HEADGEAR );
		dbLPlayer->SetLongAttr(string("HEADGEAR"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("HeadgearLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	9：外套		
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_FROCK );
		dbLPlayer->SetLongAttr(string("FROCK"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("FrockLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	10：左戒指
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_LRING );
		dbLPlayer->SetLongAttr(string("LRING"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("LRingLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	11：右戒指		
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_RRING );
		dbLPlayer->SetLongAttr(string("RRING"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("RRingLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	12：勋章1
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_MEDAL1 );
		dbLPlayer->SetLongAttr(string("MEDAL1"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("Medal1Level"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	13	勋章2
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_MEDAL2 );
		dbLPlayer->SetLongAttr(string("MEDAL2"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("Medal2Level"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	14	勋章3
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_MEDAL3 );
		dbLPlayer->SetLongAttr(string("MEDAL3"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("Medal3Level"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	15：小精灵
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_FAIRY );
		dbLPlayer->SetLongAttr(string("FAIRY"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("FairyLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
		//	16：武器
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_WEAPON );
		dbLPlayer->SetLongAttr(string("WEAPON"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("WeaponLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level

		//	披风
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_CLOAK );
		dbLPlayer->SetLongAttr(string("CLOAK"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) );// ID
		dbLPlayer->SetLongAttr(string("CloakLevel"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level

		//	17：副手武器
		pGoods = pPlayer -> m_cEquipment.GetGoods(CEquipmentContainer::EC_WEAPON2 );
		dbLPlayer->SetLongAttr(string("WEAPON2"), (DWORD)(pGoods?pGoods->GetBasePropertiesIndex():dwNull) ); // ID
		dbLPlayer->SetLongAttr(string("Weapon2Level"), (DWORD)(pGoods?pGoods->GetAddonPropertyValues(GAP_WEAPON_LEVEL,1):dwNull) ); // Level
	}
}

void CEntityManager::CDBPlayerToCPlayer(CEntityGroup* pEntity, CPlayer* pPlayer)
{
	if(pEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pEntity;

		string playerTableName = "baseproperty";
		string goodsTableName = "equipment";

		DetailPropertyCDBPlayerToCPlayer(dbPlayer, pPlayer);

		//! 仓库信息
		pPlayer->m_PlayerDepot.SetThawdate(dbPlayer->GetLongAttr(string("DepotFrostDate")));
		pPlayer->m_PlayerDepot.SetSubpackFlag(dbPlayer->GetLongAttr(string("DepotSubFlag")));
		pPlayer->m_PlayerDepot.SetPassword(dbPlayer->GetStringAttr(string("DepotPwd")));

		DetailAreaCreditCDBPlayerToCPlayer(dbPlayer, pPlayer);
		DetailLimitGoodsRecordCDBPlayerToCPlayer(dbPlayer,pPlayer);
		DetailIncTradeRecordCDBPlayerToCPlayer(dbPlayer,pPlayer);

		string strBuf = "GoodsCooldown";
		long lAttrBufSize = dbPlayer->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* cooldownBuf=(BYTE*)M_ALLOC(lAttrBufSize);
			dbPlayer->GetBufAttr(strBuf, cooldownBuf, lAttrBufSize);
			pPlayer->GetGoodsIdCooldownMap().clear();
			pPlayer->GetClassIdCooldownMap().clear();
			long loffset=0;
			DWORD dwNum=((DWORD*)cooldownBuf)[0];
			DWORD dwCooldownId=0,dwCooldownTime=0;
			loffset+=sizeof(DWORD);
			for(int i=0;i<dwNum;i++)
			{
				dwCooldownId=*(DWORD*)(&cooldownBuf[loffset]);
				loffset+=sizeof(DWORD);
				dwCooldownTime=*(DWORD*)(&cooldownBuf[loffset]);
				loffset+=sizeof(DWORD);
				pPlayer->GetClassIdCooldownMap()[dwCooldownId]=dwCooldownTime;
			}
			dwNum=*(DWORD*)(&cooldownBuf[loffset]);
			loffset+=sizeof(DWORD);
			for(int i=0;i<dwNum;i++)
			{
				dwCooldownId=*(DWORD*)(&cooldownBuf[loffset]);
				loffset+=sizeof(DWORD);
				dwCooldownTime=*(DWORD*)(&cooldownBuf[loffset]);
				loffset+=sizeof(DWORD);
				pPlayer->GetGoodsIdCooldownMap()[dwCooldownId]=dwCooldownTime;
			}
			M_FREE(cooldownBuf,lAttrBufSize);
		}

		//! 装备物品
		DetailEquipmentCDBPlayerToCPlayer(dbPlayer, pPlayer);

		//! 原始背包
		DetailOrignalPackCDBPlayerToCPlayer(dbPlayer, pPlayer);
		// subpack 1 goods
		for (int subpackNum=0; subpackNum < 5; subpackNum++)
		{
			DetailSubpackCDBPlayerToCPlayer(dbPlayer, pPlayer, subpackNum);
		}

		// wallet goods
		DetailWalletCDBPlayerToCPlayer(dbPlayer, pPlayer);
		DetailSilverCDBPlayerToCPlayer(dbPlayer,pPlayer);

		//! 主仓库
		DetailOrignalDepotCDBPlayerToCPlayer(dbPlayer, pPlayer);
		// sudepot goods
		for (int subpackNum=0; subpackNum < 5; subpackNum++)
		{
			DetailSubDepotCDBPlayerToCPlayer(dbPlayer, pPlayer, subpackNum);
		}

		DetailDepotWalletCDBPlayerToCPlayer(dbPlayer, pPlayer);
		DetailDepotSilverCDBPlayerToCPlayer(dbPlayer, pPlayer);

		// 商业背包
		DetailBusinessPackCDBPlayerToCPlayer(dbPlayer, pPlayer);

		// 加载宠物列表
		if( !LoadPlayerPet(dbPlayer, pPlayer) ) return;

		// skill data
		DetailSkillCDBPlayerToCPlayer(dbPlayer, pPlayer);
		// Vars data
		DetailVarDataCDBPlayerToCPlayer(dbPlayer, pPlayer);
		// states Data
		DetailStateCDBPlayerToCPlayer(dbPlayer, pPlayer);
		// Quest data [任务系统数据库修改]
		DetailQuestCDBPlayerToCPlayer(dbPlayer, pPlayer);
		// 俑兵任务数据
		DetailMerQuestCDBPlayerToCPlayer(dbPlayer, pPlayer);

		DetailMedalCDBPlayerToCPlayer(dbPlayer, pPlayer);
		//系统邮件列表
		DetailSysMailCDBPlayerToCPlayer(dbPlayer, pPlayer);

		//-------------------------------------------------------------------------------
		// 最近购买的10种商品列表. by Fox.		2008-02-26
		DetailIncGoodsCDBPlayerToCPlayer( dbPlayer, pPlayer );

		// new Occupation
		strBuf = "DOccuData";
		lAttrBufSize = dbPlayer->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* occuBuf = (BYTE*)M_ALLOC(lAttrBufSize);
			dbPlayer->GetBufAttr(strBuf, occuBuf, lAttrBufSize);
			long doccuPos = 0;
			if(lAttrBufSize > 0)
				pPlayer->DecordDOccuDataFromByteArray(occuBuf, doccuPos);
			M_FREE(occuBuf,lAttrBufSize);
		}
		
		DetailSpriteCDBPlayerToCPlayer(dbPlayer, pPlayer);

		DetailQuestIndexCDBPlayerToCPlayer(dbPlayer, pPlayer);

		// 玩家热键映射
		DetailFuncHotKeyCDBPlayerToCPlayer(dbPlayer,pPlayer);

		DetailLotteryCDBPlayerToCPlayer(dbPlayer,pPlayer);
	}
}
void CEntityManager::CPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* pEntity)
{
	if(pEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pEntity;

		DetailPropertyCPlayerToCDBPlayer(pPlayer, dbPlayer);
		DetailEquipmentCPlayerToCDBPlayer(pPlayer, dbPlayer);
		DetailOrignalPackCPlayerToCDBPlayer(pPlayer, dbPlayer);
		DetailSubpackCPlayerToCDBPlayer(pPlayer, dbPlayer, 0);
		DetailSubpackCPlayerToCDBPlayer(pPlayer, dbPlayer, 1);
		DetailSubpackCPlayerToCDBPlayer(pPlayer, dbPlayer, 2);
		DetailSubpackCPlayerToCDBPlayer(pPlayer, dbPlayer, 3);
		DetailSubpackCPlayerToCDBPlayer(pPlayer, dbPlayer, 4);
		DetailDepotCPlayerToCDBPlayer(pPlayer, dbPlayer);
		DetailSubDepotCPlayerToCDBPlayer(pPlayer, dbPlayer, 0);
		DetailSubDepotCPlayerToCDBPlayer(pPlayer, dbPlayer, 1);
		DetailSubDepotCPlayerToCDBPlayer(pPlayer, dbPlayer, 2);
		DetailSubDepotCPlayerToCDBPlayer(pPlayer, dbPlayer, 3);
		DetailDepotWalletCPlayerToCDBPlayer(pPlayer, dbPlayer);
		DetailDepotSilverCPlayerToCDBPlayer(pPlayer, dbPlayer);

		DetailAreaCreditCPlayerToCDBPlayer(pPlayer, dbPlayer);
		DetailLimitGoodsRecordCPlayerToCDBPlayer(pPlayer,dbPlayer);
		DetailGoodsCooldownCPlayerToCDBPlayer(pPlayer,dbPlayer);
		DetailIncTradeRecordCPlayerToCDBPlayer( pPlayer, dbPlayer );

		//! 钱包
		DetailWalletCPlayerToCDBPlayer(pPlayer, dbPlayer);
		DetailSilverCPlayerToCDBPlayer(pPlayer,dbPlayer);

		DetailSkillCPlayerToCDBPlayer(pPlayer, dbPlayer);

		DetailVarDataCPlayerToCDBPlayer(pPlayer, dbPlayer);

		DetailStateCPlayerToCDBPlayer(pPlayer, dbPlayer);

		DetailQuestCPlayerToCDBPlayer(pPlayer, dbPlayer);

		DetailMerQuestCPlayerToCDBPlayer(pPlayer, dbPlayer);

		DetailMedalCPlayerToCDBPlayer(pPlayer, dbPlayer);
		
		DetailIncGoodsCPlayerToCDBPlayer(pPlayer, dbPlayer);

		// new Occupation
		DetailDeOccuCPlayerToCDBPlayer(pPlayer, dbPlayer);

		// sprite system
		DetailSpriteCPlayerToCDBPlayer(pPlayer, dbPlayer);
		//系统邮件列表
		DetailSysMailCplayerToCDBPlayer(pPlayer, dbPlayer);

		DetailQuestIndexCPlayerToCDBPlayer(pPlayer, dbPlayer);
		// 玩家热键映射表
		DetailFuncHotKeyCPlayerToCDBPlayer(pPlayer,dbPlayer);

		DetailLotteryCPlayerToCDBPlayer(pPlayer,dbPlayer);
	}
}

void CEntityManager::CDBGoodToCGoods(CBaseEntity* pBaseEntity, CGoods* goods, const CGUID& ownerID)
{
	if(!pBaseEntity || !goods) return;

	CBaseEntity* pGoods = pBaseEntity;

	goods->SetExID(const_cast<CGUID&>(pGoods->GetGUID()));
	goods->SetName(pGoods->GetStringAttr(string("name")));
	goods->SetPrice(pGoods->GetLongAttr(string("price")));
	goods->SetAmount(pGoods->GetLongAttr(string("amount")));
	//镶嵌
	goods->InitEnchaseHole();

	CGoodsBaseProperties* pProperty=CGoodsFactory::QueryGoodsBaseProperties(goods->GetBasePropertiesIndex());
	if(pProperty)
	{
		goods->SetGoodsBaseType(pProperty->GetGoodsBaseType());
	}

	string strBuf = "AddonProperty";
	long lAttrBufSize = pBaseEntity->GetBufSize(strBuf);
	if(lAttrBufSize > 0)
	{
		BYTE* addonBuf = (BYTE*)M_ALLOC(lAttrBufSize);
		pBaseEntity->GetBufAttr(strBuf, addonBuf, lAttrBufSize);
		// 将addonBuf数据解码到CGoods
		goods->AddAddonProperty(addonBuf, lAttrBufSize );
		M_FREE(addonBuf,lAttrBufSize);
	}
	
	CGoodsBaseProperties* pBaseProperty=CGoodsFactory::QueryGoodsBaseProperties(goods->GetBasePropertiesIndex());
	if(pBaseProperty)
	{
		BOOL bProExist=FALSE;
		BOOL bRangeExist=FALSE;
		bProExist=goods->IsAddonProperyExist(GAP_EXCUSE_HURT);
		bRangeExist=pBaseProperty->IsHasAddonPropertie(GAP_EXCUSE_HURT_RANGE);
		//值和范围都存在
		if(bProExist && bRangeExist)
		{
			long lProVal=goods->GetAddonPropertyValues(GAP_EXCUSE_HURT,1);
			long lRangeMax=pBaseProperty->GetAddonPropertyValue(GAP_EXCUSE_HURT_RANGE,2);
			if(lProVal>lRangeMax)
				goods->SetAddonPropertyBaseValues(GAP_EXCUSE_HURT,1,lRangeMax);
		}

		bProExist=false;
		bRangeExist=false;
		bProExist=goods->IsAddonProperyExist(GAP_PENETRATE);
		bRangeExist=pBaseProperty->IsHasAddonPropertie(GAP_PENETRATE_RANGE);
		if(bProExist && bRangeExist)
		{
			long lProVal=goods->GetAddonPropertyValues(GAP_PENETRATE,1);
			long lRangeMax=pBaseProperty->GetAddonPropertyValue(GAP_PENETRATE_RANGE,2);
			if(lProVal>lRangeMax)
				goods->SetAddonPropertyBaseValues(GAP_PENETRATE,1,lRangeMax);
		}
	}	
}
void CEntityManager::CGoodsToCDBGood(CGoods* goods, CBaseEntity* pBaseEntity, const CGUID& ownerID)
{
	if(!goods || !pBaseEntity) return;

	CBaseEntity* pGoods = pBaseEntity;
	pGoods->SetGuidAttr(string("goodsID"), goods->GetExID());
	if(goods->GetExID() == NULL_GUID)
	{
		AddLogText("CGoodsToCDBGood() goods id is null!");
	}
	pGoods->SetLongAttr(string("goodsIndex"), goods->GetBasePropertiesIndex());
	pGoods->SetGuidAttr(string("PlayerID"), ownerID);
	pGoods->SetStringAttr(string("name"), goods->GetName());
	pGoods->SetLongAttr(string("price"), goods->GetPrice());
	pGoods->SetLongAttr(string("amount"), goods->GetAmount());

	

	vector<BYTE> m_vecAddonProperty;
	vector<CGoods::tagAddonProperty>& vecAddonProperty=goods->GetAllAddonProperties();
	CGoodsBaseProperties* pBaseProperty=CGoodsFactory::QueryGoodsBaseProperties(goods->GetBasePropertiesIndex());
	if(!pBaseProperty)
		return;
	DWORD dwAddonNum=0;
	DWORD dwPropertyId=0;
	vector<BYTE> vecAddonData;
	for( size_t i = 0; i < vecAddonProperty.size(); i ++ )
	{		
		if(pBaseProperty->IsHasAddonPropertie(vecAddonProperty[i].gapType))	
		{	
			dwPropertyId=vecAddonProperty[i].gapType;
			if(CGoodsFactory::s_GoodsAttrDBSetup[dwPropertyId][0]==1)
			{			
				dwAddonNum++;
				_AddToByteArray(&vecAddonData,(DWORD)vecAddonProperty[i].gapType);
				_AddToByteArray(&vecAddonData,vecAddonProperty[i].lValues[0]);
				_AddToByteArray(&vecAddonData,vecAddonProperty[i].lValues[1]);
			}
		}
		
		else
		{
			dwAddonNum++;
			_AddToByteArray(&vecAddonData,(DWORD)vecAddonProperty[i].gapType);
			_AddToByteArray(&vecAddonData,vecAddonProperty[i].lValues[0]);
			_AddToByteArray(&vecAddonData,vecAddonProperty[i].lValues[1]);
		}
	}
	_AddToByteArray( &m_vecAddonProperty,dwAddonNum);	

	if(vecAddonData.size()>0)
	{
		_AddToByteArray(&m_vecAddonProperty,&vecAddonData[0],vecAddonData.size());
	}
	//Maker Name Length	
	_AddToByteArray(&m_vecAddonProperty,goods->GetMakerName());


	//enchase data
	DWORD dwMaxHole=goods->GetMaxEnchaseHoleNum();
	_AddToByteArray(&m_vecAddonProperty,dwMaxHole);
	if(dwMaxHole>0)
	{
		LONG* pHoleData=goods->GetEnchaseHoleData();
		if(pHoleData)
		{
			_AddToByteArray(&m_vecAddonProperty,pHoleData[0]);
			_AddToByteArray(&m_vecAddonProperty,pHoleData[1]);
			_AddToByteArray(&m_vecAddonProperty,pHoleData[2]);
			_AddToByteArray(&m_vecAddonProperty,pHoleData[3]);
			_AddToByteArray(&m_vecAddonProperty,pHoleData[4]);

		}
		M_FREE(pHoleData,5*sizeof(long));
	}

	pGoods->SetBufAttr(string("AddonProperty"),&m_vecAddonProperty[0],m_vecAddonProperty.size());
}

void CEntityManager::CPlayerFriendToCDBFriendGroup(const CGUID &PlayerGuid, CEntityGroup* pBaseEntityOwner)
{
	if(pBaseEntityOwner)
	{
		if(pBaseEntityOwner->GetCompositeType() == COM_COMPOSITE)
			LinkmanSystem::GetInstance().AddToByteArray_ForDBS(PlayerGuid, (CEntityGroup*)pBaseEntityOwner);
	}
}
void CEntityManager::CDBFriendGroupToCPlayerFriend(const CGUID &PlayerGuid, CEntityGroup*linkmanGroupOwner, CEntityGroup*linkmanGroupAim)
{
	if(linkmanGroupOwner && linkmanGroupAim)
	{
		if(linkmanGroupOwner->GetCompositeType() == COM_COMPOSITE && linkmanGroupAim->GetCompositeType()  == COM_COMPOSITE)
			LinkmanSystem::GetInstance().DecodeDataFromDBS(PlayerGuid, (CEntityGroup*)linkmanGroupOwner, linkmanGroupAim);
	}
}

void CEntityManager::DetailSpriteCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* pEntity)
{
	if(pPlayer && pEntity)
	{
		CEntityGroup* pDbPlayer = (CEntityGroup*)pEntity;
		CEntityGroup* pSpriteGroup = NULL;
		map<string, CGUID>::iterator guidItr = pDbPlayer->GetGuidByComFlagMap().find(string("[playerspritegroup]"));
		if(guidItr != pDbPlayer->GetGuidByComFlagMap().end())
		{
			map<CGUID, CBaseEntity*>::iterator enItr = pDbPlayer->GetEntityGroupMap().find(guidItr->second);
			if(enItr != pDbPlayer->GetEntityGroupMap().end())
				pSpriteGroup = (CEntityGroup*)enItr->second;
		}
		
		pSpriteGroup->SetCurDbOperFlag(1);
		pSpriteGroup->SetCurDbOperType(DB_OPER_DELETE_INSERT);
		pSpriteGroup->ReleaseChilds();

		CEntity* pSprite = (CEntity*)GetGame()->GetEntityManager()->NewBaseEntity(string("[player_sprite]"), NULL_GUID);

		pSprite->SetGuidAttr(string("PlayerGuid"), pDbPlayer->GetGUID());

		vector<BYTE> doPBA;
		doPBA.clear();
		pPlayer->m_SpriteSystem.AddToByteArray(doPBA);
		pSprite->SetBufAttr(string("AutoSupplyData"), &doPBA[0], (long)doPBA.size());

		doPBA.clear();
		pPlayer->m_SetupOnServer.AddToByteArray(doPBA);
		pSprite->SetBufAttr(string("PlayerSetup"), &doPBA[0], (long)doPBA.size());

        //稳定挂机时间

        pSprite->SetLongAttr(string("AutoStableHookTime"), pPlayer->m_SpriteSystem.GetStableTime());

		pSprite->SetCurDbOperFlag(1);
		pSprite->SetCurDbOperType(DB_OPER_DELETE_INSERT);
		pSpriteGroup->AddChild(pSprite);
	}
}
void CEntityManager::DetailSpriteCDBPlayerToCPlayer(CEntityGroup* pEntity, CPlayer* pPlayer)
{
	if(pPlayer && pEntity)
	{
		CEntityGroup* pDbPlayer = (CEntityGroup*)pEntity;
		CEntityGroup* pSpriteGroup = NULL;
		map<string, CGUID>::iterator guidItr = pDbPlayer->GetGuidByComFlagMap().find(string("[playerspritegroup]"));
		if(guidItr != pDbPlayer->GetGuidByComFlagMap().end())
		{
			map<CGUID, CBaseEntity*>::iterator enItr = pDbPlayer->GetEntityGroupMap().find(guidItr->second);
			if(enItr != pDbPlayer->GetEntityGroupMap().end())
				pSpriteGroup = (CEntityGroup*)enItr->second;
		}

		if( pSpriteGroup != NULL &&
			pSpriteGroup->GetEntityGroupMap().size() != 0)
		{
			CEntityGroup* pSprite = (CEntityGroup*)pSpriteGroup->GetEntityGroupMap().begin()->second;
			if(pSprite)
			{
				string strBuf = "AutoSupplyData";
				long lAttrBufSize = pSprite->GetBufSize(strBuf);
				if(lAttrBufSize > 0)
				{
					BYTE* buf = (BYTE*)M_ALLOC(lAttrBufSize);
					pSprite->GetBufAttr( strBuf, buf, lAttrBufSize);
					long pos = 0;
					pPlayer->m_SpriteSystem.GetFormByteArray(buf, pos);
					M_FREE(buf,lAttrBufSize);
				}

				strBuf = "PlayerSetup";
				lAttrBufSize = pSprite->GetBufSize(strBuf);
				if(lAttrBufSize > 0)
				{
					BYTE* buf = (BYTE*)M_ALLOC(lAttrBufSize);
					pSprite->GetBufAttr(strBuf,  buf, lAttrBufSize);
					long pos = 0;
					pPlayer->m_SetupOnServer.GetFormByteArray(buf, pos);
					M_FREE(buf,lAttrBufSize);	
				}
                //读取稳定挂机时间
                strBuf = "AutoStableHookTime";
                long lAttrValue = pSprite->GetLongAttr(strBuf);
                pPlayer->m_SpriteSystem.SetStableTime(lAttrValue);
			}
		}
	}
}
// 分层转换 CPlayer 到 CDBPlayer
void CEntityManager::DetailPropertyCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		if(0 > pPlayer->GetRegionID() || pPlayer->GetRegionID() >= 100000)
		{
			AddLogText("CShape::SetRegionID RegionID[%d] is Error!", pPlayer->GetRegionID());
		}
		//// player BaseProperties
		dbPlayer->SetName(pPlayer->GetName());
		dbPlayer->SetGuidAttr(string("guid"), pPlayer->GetExID());
		dbPlayer->SetStringAttr(string("Account"), pPlayer->GetAccount());
		dbPlayer->SetStringAttr(string("Name"), pPlayer->GetName());
		dbPlayer->SetLongAttr(string("Levels"), (long)pPlayer->GetLevel());
		dbPlayer->SetLongAttr(string("Occupation"), (long)pPlayer->GetOccupation());
		dbPlayer->SetLongAttr(string("Const"), (long)pPlayer->GetConst());
		dbPlayer->SetLongAttr(string("byAssOccu"), (long)pPlayer->GetDOccupation());
		dbPlayer->SetLongAttr(string("Sex"), (long)pPlayer->GetSex());
		dbPlayer->SetLongAttr(string("Country"), (long)pPlayer->GetCountry());
		dbPlayer->SetLongAttr(string("HeadPic"), (long)pPlayer->GetHeadPic());
		dbPlayer->SetLongAttr(string("FacePic"), (long)pPlayer->GetFacePic());
		dbPlayer->SetLongAttr(string("RegionID"), (long)pPlayer->GetRegionID());
		dbPlayer->SetLongAttr(string("PosX"), (long)pPlayer->GetPosX());
		dbPlayer->SetLongAttr(string("PosY"), (long)pPlayer->GetPosY());

		//// player ablities
		dbPlayer->SetLongAttr(string("Hp"), (long)pPlayer->GetHP());
		dbPlayer->SetLongAttr(string("Mp"), (long)pPlayer->GetMP());
		dbPlayer->SetLongAttr(string("Energy"), (long)pPlayer->GetEnergy());
		dbPlayer->SetLongAttr(string("Stamina"), (long)pPlayer->GetStamina());
		dbPlayer->SetLongAttr(string("ticket"), (long)pPlayer->GetTicket());
		dbPlayer->SetLongAttr(string("Levels"), (long)pPlayer->GetLevel());
		dbPlayer->SetLongAttr(string("Exp"), (long)pPlayer->GetExp());
		dbPlayer->SetLongAttr(string("ExpMultiple"), (long)pPlayer->GetExpMultiple());
		
		dbPlayer->SetLongAttr(string("PresentExp"), (long)pPlayer->GetPresentExp()); 

		dbPlayer->SetLongAttr(string("lState"), (long)pPlayer->GetState());
		dbPlayer->SetLongAttr(string("byShowFashion"), (long)pPlayer->GetShowFashion());
		dbPlayer->SetLongAttr(string("ZanDoHunterCredit"),(long)pPlayer->GetZanDoCredit());//赞多狩猎声望
		dbPlayer->SetLongAttr(string("ChurchCredit"),(long)pPlayer->GetChurchCredit());//光之教会声望
		////
		dbPlayer->SetLongAttr(string("RankOfNobility"), (long)pPlayer->GetRankOfNobility());
		dbPlayer->SetLongAttr(string("RankOfNobCredit"), (long)pPlayer->GetRankOfNobCredit());
		dbPlayer->SetLongAttr(string("RankOfMercenary"), (long)pPlayer->GetRankOfMercenary());
		dbPlayer->SetLongAttr(string("MercenaryCredit"), (long)pPlayer->GetMercenaryCredit());
		dbPlayer->SetLongAttr(string("BatakCredit"), (long)pPlayer->GetBatakCredit());
		dbPlayer->SetLongAttr(string("MedalScores"), (long)pPlayer->GetMedalScores());
		dbPlayer->SetLongAttr(string("CountryContribute"), (long)pPlayer->GetCountryContribute());
		dbPlayer->SetLongAttr(string("SpouseID"), (long)pPlayer->GetSpouseId());
		dbPlayer->SetLongAttr(string("SpouseParam"), (long)pPlayer->GetSpouseParam());	
		dbPlayer->SetLongAttr(string("BusinessLevel"), (long)pPlayer->GetBusinessLevel());
		dbPlayer->SetLongAttr(string("IsBusinessMan"), (long)(pPlayer->IsBusinessMan() ? 1 : 0 )); //是否是商人
		dbPlayer->SetLongAttr(string("BusinessExp"), (long)pPlayer->GetBusinessExp());
		dbPlayer->SetLongAttr(string("ArtisanLevel"), (long)pPlayer->GetArtisanLevel());
		dbPlayer->SetLongAttr(string("ArtisanExp"), (long)pPlayer->GetArtisanExp());
		dbPlayer->SetLongAttr(string("PKCount"), (long)pPlayer->GetPkCount());
		dbPlayer->SetLongAttr(string("PKValue"), (long)pPlayer->GetPkValue());
		dbPlayer->SetLongAttr(string("PVPCount"), (long)pPlayer->GetPVPCount());	
		dbPlayer->SetLongAttr(string("PVPValue"), (long)pPlayer->GetPVPValue());
		dbPlayer->SetLongAttr(string("PKOnOff"), (long)pPlayer->GetPKOnOff());
		dbPlayer->SetLongAttr(string("LastExitGameTime"), (long)pPlayer->GetLastExitGameTime());
		dbPlayer->SetGuidAttr(string("PHGUID"), pPlayer->GetPersonalHouseRgnGUID());
		dbPlayer->SetGuidAttr(string("DupRgnID"), pPlayer->GetRegionExID());
		dbPlayer->SetLongAttr(string("DepotFrostDate"),	pPlayer->m_PlayerDepot.GetThawdate());
		dbPlayer->SetStringAttr(string("DepotPwd"),		pPlayer->m_PlayerDepot.GetPassword());
		dbPlayer->SetLongAttr(string("DepotSubFlag"),	pPlayer->m_PlayerDepot.GetSubpackFlag());
		dbPlayer->SetLongAttr(string("ShowCountry"), pPlayer->GetShowCountry());
		//! 玩家当前的称号类型
		dbPlayer->SetLongAttr(string("TileType"), (LONG)pPlayer->GetTitleType());
		dbPlayer->SetBufAttr(string("HotKey"), (BYTE*)pPlayer->GetHotKeyArray(), sizeof(DWORD)*24);
		//职业等级
		dbPlayer->SetBufAttr(string("OccuLvl"), pPlayer->GetOccuLvl(), sizeof(BYTE)*OCC_Max);
		//职业经验
		dbPlayer->SetBufAttr(string("OccuExp"), pPlayer->GetOccuExp(), sizeof(DWORD)*OCC_Max);
		//职业SP
		dbPlayer->SetBufAttr(string("OccuSP"), pPlayer->GetOccuSP(), sizeof(DWORD)*OCC_Max);
		dbPlayer->SetLongAttr(string("silence"), (long)pPlayer->GetSilenceTime());

		//防外挂相关数据 BAIYUN@20090602
		dbPlayer->SetBufAttr(string("AntiCheatData"), &pPlayer->GetACProperty()->m_dbData, sizeof(CPlayer::dbAntiCheatData));
		dbPlayer->SetLongAttr(string("ACWrongTimes"), pPlayer->GetACProperty()->m_dbData.m_nAccWrongTimes );

		//隐匿属性
		dbPlayer->SetLongAttr(string("CanHide"), pPlayer->GetCanHide());
		dbPlayer->SetLongAttr(string("HideFlag"), pPlayer->GetHideFlag());

		//! 活力点属性
		dbPlayer->SetLongAttr(string("CurrBaseActive"), pPlayer->GetCurrBaseActive());
		dbPlayer->SetLongAttr(string("CurrExActive"), pPlayer->GetCurrExActive());
		dbPlayer->SetLongAttr(string("ActiveTime"), pPlayer->GetActiveTime());

		//! CP属性
		dbPlayer->SetLongAttr( "CurPKCP", pPlayer->GetCurPKCP() );
		//! 结婚属性
		dbPlayer->SetLongAttr(string("MarriageStep"), pPlayer->GetMarriageStep());
		//! 卡片密码
		pBaseEntity->SetLongAttr(string("CardPwdThawTime"), pPlayer->m_CardPwdThawTime);
		pBaseEntity->SetStringAttr(string("CardPwd"), pPlayer->m_CardPwd);
		// 定时器
		string sCountTimer = string("CountTimerList");
		std::vector<BYTE> pCountBA;
		_AddToByteArray(&pCountBA, pPlayer->GetCountTimerNum());
		map<DWORD, CPlayer::tagCountTimer>::iterator timeritr = pPlayer->GetTimerMap().begin();
		for(; timeritr!=pPlayer->GetTimerMap().end(); timeritr++)
		{
			CPlayer::tagCountTimer& timer = timeritr->second;
			_AddToByteArray(&pCountBA, (DWORD)timer.m_dwID);
			_AddToByteArray(&pCountBA, (char)timer.m_bCountType);
			_AddToByteArray(&pCountBA, (DWORD)timer.m_dwCountTime);
			_AddToByteArray(&pCountBA, (DWORD)timer.m_dwRetTime);
			_AddToByteArray(&pCountBA, (DWORD)timer.m_dwStartRetTime);
			_AddToByteArray(&pCountBA, (DWORD)timer.m_dwTimerType);	
		}
		dbPlayer->AddBufAttr(sCountTimer, &pCountBA[0], (long)pCountBA.size());
		//! 战队积分
		pBaseEntity->SetLongAttr(string("SentaiPoints"), pPlayer->GetSentaiPoints());
		pBaseEntity->SetLongAttr(string("PenaltyPoints"), pPlayer->GetPenaltyPoints());
		////! 抽奖积分
		pBaseEntity->SetLongAttr(string("PersonalCredit"), pPlayer->GetPersonalCredit());
		pBaseEntity->SetLongAttr(string("PersonalCreditTotal"), pPlayer->GetPersonalCreditTotal());
		pBaseEntity->SetLongAttr(string("PersonalCreditVelue"), pPlayer->GetPersonalCreditVelue());
		pBaseEntity->SetLongAttr(string("WorldCreditVelue"), pPlayer->GetWorldCreditVelue());
		pBaseEntity->SetLongAttr(string("LotteryLevel"), pPlayer->GetLotteryLevel());
	}
}
void CEntityManager::DetailEquipmentCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		// 清空CDBPlayer装备物品
		ClearPlayerContainerMap(dbPlayer, PEI_EQUIPMENT);

		map<CEquipmentContainer::EQUIPMENT_COLUMN,CGoods*>::iterator itr = pPlayer->GetEquipmentGoodsMap()->begin();
		for(; itr != pPlayer->GetEquipmentGoodsMap()->end(); itr++) //
		{
			DWORD pos = 0;
			pPlayer->m_cEquipment.QueryGoodsPosition(itr->second, pos);
			AddGoodsToPlayer(dbPlayer, itr->second, pos, PEI_EQUIPMENT);
		}
	}
}

void CEntityManager::DetailOrignalPackCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		// 清空CDBPlayer装备物品
		ClearPlayerContainerMap(dbPlayer, PEI_PACKET);

		hash_map<CGUID,CGoods*,hash_guid_compare>::iterator itr = pPlayer->GetOriginPackGoodsMap()->begin();
		for(; itr != pPlayer->GetOriginPackGoodsMap()->end(); itr++) //
		{
			DWORD pos = 0;
			pPlayer->getOriginPack()->QueryGoodsPosition(itr->second, pos);
			AddGoodsToPlayer(dbPlayer, itr->second, pos, PEI_PACKET);
		}
	}
}
void CEntityManager::DetailSubpackCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* pBaseEntity, long subPackPos)
{
	if(pPlayer && pBaseEntity)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		ClearPlayerContainerMap(dbPlayer, (PLAYER_EXTEND_ID)(PEI_PACK1+subPackPos));
		ClearPlayerSubpackSelfMap(dbPlayer, subPackPos);

		// 先添加子背包对象物品
		// 子背包对象容器
		CSubpackContainer* pSubpackContainer = pPlayer->GetSubpackContainer();
		CGoods *pGoods = pSubpackContainer->GetGoods(10001+subPackPos);
		if(pGoods)
		{
			AddGoodsToPlayer(dbPlayer, pGoods, (long)(PEI_PACK1+subPackPos), PEI_PACK);
		}

		// 再添加子背包物品
		// 背包内物品
		set<CGUID> dirtySubpackGoods;
		if(pGoods)
		{
			hash_map<CGUID,CGoods*,hash_guid_compare>::iterator itr =pPlayer->GetSubPackGoodsMap(subPackPos)->begin();
			for(; itr != pPlayer->GetSubPackGoodsMap(subPackPos)->end(); itr++) //
			{
				DWORD pos = 0;
				pPlayer->getSubpackContainer()->GetSubpack(subPackPos)->pSubpackContainer->QueryGoodsPosition(itr->second, pos);
				AddGoodsToPlayer(dbPlayer, itr->second, pos, (PLAYER_EXTEND_ID)(PEI_PACK1+subPackPos));
			}	
		}
	}
}
void CEntityManager::DetailWalletCPlayerToCDBPlayer(CPlayer* player, CEntityGroup* pBaseEntity)
{
	if(player && pBaseEntity)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		ClearPlayerContainerMap(dbPlayer, PEI_WALLET);

		CGoods *pMoneyGoods = player->m_cWallet.GetGoodsCoins();

		if(NULL == pMoneyGoods) // 钱包为空,清除DBGoods对象
			return;

		AddGoodsToPlayer(dbPlayer, pMoneyGoods, 0, PEI_WALLET);
	}
}

void CEntityManager::DetailSilverCPlayerToCDBPlayer(CPlayer* player, CEntityGroup* pBaseEntity)
{
	if(player && pBaseEntity)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		ClearPlayerContainerMap(dbPlayer, PEI_SILVERWALLET);

		CGoods *pMoneyGoods = player->m_cSilverWallet.GetGoodsCoins();

		if(NULL == pMoneyGoods) // 钱包为空,清除DBGoods对象
			return;

		AddGoodsToPlayer(dbPlayer, pMoneyGoods, 0, PEI_SILVERWALLET);
	}
}

void CEntityManager::DetailQuestCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		// Quest data [任务系统数据库操作] 
		vector<BYTE> pqd;
		pqd.clear();
		pPlayer->AddQuestDataToByteArray(&pqd);
		if(pqd.size())
			dbPlayer->SetBufAttr(string("ListQuest"), (BYTE*)&pqd[0], (long)pqd.size());
	}
}
void CEntityManager::DetailMerQuestCPlayerToCDBPlayer	(CPlayer* pPlayer, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		// Quest data [任务系统数据库操作] 
		vector<BYTE> pqd;
		pqd.clear();
		pPlayer->AddMerQuestDataByteArray(&pqd);
		if(pqd.size())
			dbPlayer->SetBufAttr(string("MercenaryQuest"), (BYTE*)&pqd[0], (long)pqd.size());
	}
}
void CEntityManager::DetailSkillCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		//skills id field
		list<CPlayer::tagSkill>& psl = pPlayer->GetSkillList();
		long skillNum = psl.size();
		long bufSize = pPlayer->GetSkillList().size() * sizeof(CPlayer::tagSkill) + sizeof(long);
		if(skillNum)
		{
			BYTE* buf = (BYTE*)M_ALLOC(bufSize);
			memcpy(&buf[0], &skillNum, sizeof(long));
			list<CPlayer::tagSkill>::iterator itr = psl.begin();
			long num = 0;
			for(; itr != psl.end(); itr++)
			{
				memcpy(&(buf[sizeof(long)+sizeof(CPlayer::tagSkill)*num]), &(*itr), sizeof(CPlayer::tagSkill));
				num++;
			}
			dbPlayer->SetBufAttr(string("ListSkill"), (BYTE*)&buf[0], (long)bufSize);
			M_FREE(buf,bufSize);
		}
		else
		{
			long tmpValue = 0;
			dbPlayer->SetBufAttr(string("ListSkill"), (BYTE*)&tmpValue, sizeof(long));
		}
	}
}
void CEntityManager::DetailStateCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		// states Data
		if(pPlayer->GetExStates().size())
		{
			dbPlayer->SetBufAttr(string("ListState"), (BYTE*)&pPlayer->GetExStates()[0], (long)pPlayer->GetExStates().size());
		}
		else
		{
			DWORD tmpValue = 0;
			dbPlayer->SetBufAttr(string("ListState"), (BYTE*)&tmpValue, sizeof(DWORD));
		}
	}
}
void CEntityManager::DetailVarDataCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		// 先删除CDBPlayer的老数据 
		// Vars num
		long vNum = pPlayer->GetVariableNum();
		// Vars guid num
		long gNum = pPlayer->GetGuidNum();
		// vardata len
		long vLen = pPlayer->GetVariableDataLength();

		// Vars Data
		long bufSize = vLen + sizeof(long)*3;
		BYTE* buf = (BYTE*)M_ALLOC(bufSize);
		memcpy(&buf[0], &vNum, sizeof(long));
		memcpy(&buf[sizeof(long)], &gNum, sizeof(long));
		memcpy(&buf[sizeof(long)+sizeof(long)], &vLen, sizeof(long));
		char* pVariableList = pPlayer->GetVariableData();
		if(pVariableList && vLen)
			memcpy(&buf[sizeof(long)*3], pVariableList, vLen);

		dbPlayer->SetBufAttr(string("VariableList"), (BYTE*)buf, (long)bufSize);
		M_FREE(buf,bufSize);

	}
}

void CEntityManager::DetailDepotCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		// 清空CDBPlayer装备物品
		ClearPlayerContainerMap(dbPlayer, PEI_DCT_Primary);

		CGoodsContainer* pContainer = pPlayer->m_PlayerDepot.FindContainer(PEI_DCT_Primary);
		if(NULL == pContainer)
		{
			char outStr[128] = {0};
			sprintf(outStr, "DecodeDepotGoodsMsg:Goods's Container is NULL!");
			AddLogText(outStr);
			return;
		}
		long lNum = 0;
		for (long i = 0; lNum < pContainer->GetGoodsAmount(); ++i)
		{
			long lPos = i;
			CGoods *pGoods = pContainer->GetGoods(lPos);
			if (NULL != pGoods)
			{
				AddGoodsToPlayer(dbPlayer, pGoods, lPos, PEI_DCT_Primary);
				++lNum;
			}
			//! 逻辑上来说，i若超过256（仓库和子背包的最大容量都远小于这个数），一定是出了错，不需要在继续找物品了
			assert(256 > i);
			if (256 < i) break;
		}

		//! 新仓库信息
		dbPlayer->SetLongAttr(string("DepotFrostDate"),	pPlayer->m_PlayerDepot.GetThawdate());
		dbPlayer->SetStringAttr(string("DepotPwd"),		pPlayer->m_PlayerDepot.GetPassword());
		dbPlayer->SetLongAttr(string("DepotSubFlag"),	pPlayer->m_PlayerDepot.GetSubpackFlag());
	}
}

void CEntityManager::DetailSubDepotCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* pBaseEntity, long subPackPos)
{
	if(pPlayer && pBaseEntity)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		ClearPlayerContainerMap(dbPlayer, (PLAYER_EXTEND_ID)(PEI_DCT_Secondary1+subPackPos));
		ClearPlayerSubDepotSelfMap(dbPlayer, subPackPos);

		// 先添加子背包对象物品
		// 子背包对象容器
		CSubpackContainer *pContainer = (CSubpackContainer*)pPlayer->m_PlayerDepot.FindContainer(eDCT_Secondary);
		assert(NULL != pContainer);
		if(NULL == pContainer) return;
		
		CGoods *pGoods = pContainer->GetGoods(10001+subPackPos);
		if(pGoods)
		{
			AddGoodsToPlayer(dbPlayer, pGoods, (long)(10001+subPackPos), PEI_DCT_Secondary);
		}

		// 再添加子背包物品
		// 背包内物品
		if(pGoods)
		{
			hash_map<CGUID,CGoods*,hash_guid_compare>::iterator itr =pContainer->GetSubpack(subPackPos)->pSubpackContainer->GetGoodsMap()->begin();
			for(; itr != pContainer->GetSubpack(subPackPos)->pSubpackContainer->GetGoodsMap()->end(); itr++) //
			{
				DWORD pos = 0;
				pContainer->GetSubpack(subPackPos)->pSubpackContainer->QueryGoodsPosition(itr->second, pos);
				AddGoodsToPlayer(dbPlayer, itr->second, pos, (PLAYER_EXTEND_ID)(PEI_DCT_Secondary1+subPackPos));
			}	
		}
	}
}
void CEntityManager::DetailDepotWalletCPlayerToCDBPlayer(CPlayer* player, CEntityGroup* pBaseEntity)
{
	if(player && pBaseEntity)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		ClearPlayerContainerMap(dbPlayer, PEI_DCT_Gold);

		CGoods *pMoneyGoods = ((CWallet*)(player->m_PlayerDepot.FindContainer(PEI_DCT_Gold)))->GetGoodsCoins();

		if(NULL == pMoneyGoods) // 钱包为空,清除DBGoods对象
			return;

		AddGoodsToPlayer(dbPlayer, pMoneyGoods, 0, PEI_DCT_Gold);
	}
}

void CEntityManager::DetailDepotSilverCPlayerToCDBPlayer(CPlayer* player, CEntityGroup* pBaseEntity)
{
	if(player && pBaseEntity)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		ClearPlayerContainerMap(dbPlayer, PEI_DCT_Silver);

		CGoods *pMoneyGoods = ((CWallet*)(player->m_PlayerDepot.FindContainer(PEI_DCT_Silver)))->GetGoodsCoins();

		if(NULL == pMoneyGoods) // 钱包为空,清除DBGoods对象
			return;

		AddGoodsToPlayer(dbPlayer, pMoneyGoods, 0, PEI_DCT_Silver);
	}
}

// 分层转换 CDBPlayer 到 CPlayer
void CEntityManager::DetailPropertyCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		// player BaseProperties
		pPlayer->SetAccount(dbPlayer->GetStringAttr(string("Account")));
		pPlayer->SetName(dbPlayer->GetStringAttr(string("Name")));
		pPlayer->SetLevel(dbPlayer->GetLongAttr(string("Levels")));
		pPlayer->SetOccupation(dbPlayer->GetLongAttr(string("Occupation")));
		pPlayer->SetConst(eConst(dbPlayer->GetLongAttr(string("Const"))) );
		pPlayer->SetDOccupation(eDeputyOccu(dbPlayer->GetLongAttr(string("byAssOccu"))));
		pPlayer->SetSex(dbPlayer->GetLongAttr(string("Sex")));
		pPlayer->SetCountry(dbPlayer->GetLongAttr(string("Country")));
		pPlayer->SetRegionID(dbPlayer->GetLongAttr(string("RegionID")));
		pPlayer->SetGraphicsID(pPlayer->GetSex()+1);
		CGUID tCurRgnID;
		dbPlayer->GetGuidAttr(string("DupRgnID"), tCurRgnID);
		pPlayer->SetRegionExID(tCurRgnID);
		//// player ablities
		pPlayer->SetPosX(dbPlayer->GetLongAttr(string("PosX")));
		pPlayer->SetPosY(dbPlayer->GetLongAttr(string("PosY")));
		pPlayer->SetDir(dbPlayer->GetLongAttr(string("Dir")));
		pPlayer->SetHeadPic(dbPlayer->GetLongAttr(string("HeadPic")));
		pPlayer->SetFacePic(dbPlayer->GetLongAttr(string("FacePic")));
		pPlayer->SetHP(dbPlayer->GetLongAttr(string("Hp")));
		pPlayer->SetMP(dbPlayer->GetLongAttr(string("Mp")));
		pPlayer->SetEnergy(dbPlayer->GetLongAttr(string("Energy")));
		pPlayer->SetStamina(dbPlayer->GetLongAttr(string("Stamina")));
		pPlayer->SetTicket(dbPlayer->GetLongAttr(string("ticket")));
		pPlayer->SetExp(dbPlayer->GetLongAttr(string("Exp")));
		pPlayer->SetExpMultiple(dbPlayer->GetLongAttr(string("ExpMultiple")));
		pPlayer->SetPresentExp(dbPlayer->GetLongAttr(string("PresentExp")));
		pPlayer->SetRankOfNobility(dbPlayer->GetLongAttr(string("RankOfNobility")));
		pPlayer->SetRankOfNobCredit(dbPlayer->GetLongAttr(string("RankOfNobCredit")));
		pPlayer->SetRankOfMercenary(dbPlayer->GetLongAttr(string("RankOfMercenary")));
		pPlayer->SetMercenaryCredit(dbPlayer->GetLongAttr(string("MercenaryCredit")));
		pPlayer->SetBatakCredit(dbPlayer->GetLongAttr(string("BatakCredit")));
		pPlayer->SetMedalScores(dbPlayer->GetLongAttr(string("MedalScores")));
		pPlayer->SetCountryContribute(dbPlayer->GetLongAttr(string("CountryContribute")));
		pPlayer->SetSpouseId(dbPlayer->GetLongAttr(string("SpouseID")));
		pPlayer->SetSpouseParam(dbPlayer->GetLongAttr(string("SpouseParam")));
		pPlayer->SetBusinessLevel(dbPlayer->GetLongAttr(string("BusinessLevel")));
		pPlayer->SetBusinessMan(dbPlayer->GetLongAttr(string("IsBusinessMan")) != 0); //是否是商人
		pPlayer->SetBusinessExp(dbPlayer->GetLongAttr(string("BusinessExp")));
		pPlayer->SetArtisanLevel(dbPlayer->GetLongAttr(string("ArtisanLevel")));
		pPlayer->SetArtisanExp(dbPlayer->GetLongAttr(string("ArtisanExp")));
		pPlayer->SetPkCount(dbPlayer->GetLongAttr(string("PKCount")));
		pPlayer->SetPkValue(dbPlayer->GetLongAttr(string("PKValue")));
		pPlayer->SetPVPCount(dbPlayer->GetLongAttr(string("PVPCount")));
		pPlayer->SetPVPValue(dbPlayer->GetLongAttr(string("PVPValue")));
		pPlayer->SetPKOnOff(dbPlayer->GetLongAttr(string("PKOnOff")));
		pPlayer->SetLastExitGameTime(dbPlayer->GetLongAttr(string("LastExitGameTime")));
		pPlayer->SetTitleType(dbPlayer->GetLongAttr(string("TileType")));
		pPlayer->SetShowCountry(dbPlayer->GetLongAttr(string("ShowCountry"))!=0);
		pPlayer->SetSilenceTime(dbPlayer->GetLongAttr(string("silence")));
		pPlayer->SetState(dbPlayer->GetLongAttr(string("lState")));
		pPlayer->SetShowFashion(dbPlayer->GetLongAttr(string("byShowFashion")));
		pPlayer->SetZanDoCredit(dbPlayer->GetLongAttr(string("ZanDoHunterCredit")));
		pPlayer->SetChurchCredit(dbPlayer->GetLongAttr(string("ChurchCredit")));
		//! 活力点属性
		pPlayer->SetCurrBaseActive(dbPlayer->GetLongAttr(string("CurrBaseActive")));
		pPlayer->SetCurrExActive(dbPlayer->GetLongAttr(string("CurrExActive")));
		pPlayer->SetActiveTime(dbPlayer->GetLongAttr(string("ActiveTime")));
		//! 结婚属性
		pPlayer->SetMarriageStep(dbPlayer->GetLongAttr(string("MarriageStep")));

		CGUID tGuid;
		dbPlayer->GetGuidAttr(string("PHGUID"), tGuid);
		pPlayer->SetPersonalHouseRgnGUID(tGuid);
		BYTE* hotKeyBuf = (BYTE*)M_ALLOC(sizeof(DWORD)*24);
		dbPlayer->GetBufAttr(string("HotKey"), hotKeyBuf, sizeof(DWORD)*24);
		for(int i = 0; i < 24; i++)
		{
			pPlayer->SetHotKey(i, ((DWORD*)hotKeyBuf)[i]);
		}
		M_FREE(hotKeyBuf,sizeof(DWORD)*24);
		
		BYTE temptBuff[1024];
		memset(temptBuff,0,1024);
		//职业等级
		long lSize = dbPlayer->GetBufAttr(string("OccuLvl"), temptBuff, sizeof(BYTE)*OCC_Max);
		lSize = min((sizeof(BYTE)*OCC_Max),lSize);
		pPlayer->SetOccuLvl(temptBuff,lSize);
		//职业经验
		memset(temptBuff,0,1024);
		lSize = dbPlayer->GetBufAttr(string("OccuExp"), temptBuff, sizeof(DWORD)*OCC_Max);
		lSize = min((sizeof(DWORD)*OCC_Max),lSize);
		pPlayer->SetOccuExp(temptBuff,lSize);
		//职业SP
		memset(temptBuff,0,1024);
		lSize = dbPlayer->GetBufAttr(string("OccuSP"), temptBuff, sizeof(DWORD)*OCC_Max);
		lSize = min((sizeof(DWORD)*OCC_Max),lSize);
		pPlayer->SetOccuSP(temptBuff,lSize);

		//防外挂相关数据 BAIYUN@20090602
		CPlayer::dbAntiCheatData tmpDBACData;
		memset(&tmpDBACData,0,sizeof(CPlayer::dbAntiCheatData));
		dbPlayer->GetBufAttr(string("AntiCheatData"), &tmpDBACData,sizeof(CPlayer::dbAntiCheatData));
		pPlayer->SetDBACProperty(&tmpDBACData);
		// not used
		dbPlayer->GetLongAttr(string("ACWrongTimes"));

		//隐匿属性
		pPlayer->SetCanHide(dbPlayer->GetLongAttr(string("CanHide")));
		pPlayer->SetHideFlag(dbPlayer->GetLongAttr(string("HideFlag")));

		pPlayer->SetCurPKCP( dbPlayer->GetLongAttr( "CurPKCP" ) );
		//! 卡片密码
		pPlayer->m_CardPwdThawTime = dbPlayer->GetLongAttr(string("CardPwdThawTime"));
		strcpy_s(pPlayer->m_CardPwd, PASSWORD_SIZE, dbPlayer->GetStringAttr(string("CardPwd")));
		
		// 定时器
		string sCountTimer = string("CountTimerList");
		long cBufSize = dbPlayer->GetBufSize(sCountTimer);
		if(cBufSize)
		{
			BYTE* countTimerBuf = new BYTE[cBufSize];
			dbPlayer->GetBufAttr(sCountTimer, countTimerBuf, cBufSize);
			long pos = 0;
			long timerNum = _GetLongFromByteArray(countTimerBuf, pos, cBufSize);
			for(int i=0; i<timerNum; i++)
			{
				CPlayer::tagCountTimer timer;
				timer.m_dwID = _GetLongFromByteArray(countTimerBuf, pos, cBufSize);
				timer.m_bCountType = (0 != _GetByteFromByteArray(countTimerBuf, pos, cBufSize));
				timer.m_dwCountTime = _GetDwordFromByteArray(countTimerBuf, pos, cBufSize);
				timer.m_dwRetTime = _GetDwordFromByteArray(countTimerBuf, pos, cBufSize);
				timer.m_dwStartRetTime = _GetDwordFromByteArray(countTimerBuf, pos, cBufSize);
				timer.m_dwTimerType = _GetDwordFromByteArray(countTimerBuf, pos, cBufSize);
				pPlayer->AddCountTimer(timer);
			}
		}

		//! 战队积分
		pPlayer->SetSentaiPoints(dbPlayer->GetLongAttr(string("SentaiPoints")), false);
		pPlayer->SetPenaltyPoints(dbPlayer->GetLongAttr(string("PenaltyPoints")));
		////! 抽奖积分
		pPlayer->SetPersonalCredit(dbPlayer->GetLongAttr(string("PersonalCredit")));
		pPlayer->SetPersonalCreditTotal(dbPlayer->GetLongAttr(string("PersonalCreditTotal")));
		pPlayer->SetPersonalCreditVelue(dbPlayer->GetLongAttr(string("PersonalCreditVelue")));
		pPlayer->SetWorldCreditVelue(dbPlayer->GetLongAttr(string("WorldCreditVelue")));
		pPlayer->SetLotteryLevel(dbPlayer->GetLongAttr(string("LotteryLevel")));
	}
}
void CEntityManager::DetailEquipmentCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		const CGUID& goodsGroupID = dbPlayer->QueryChildGuidByComFlag(string("[goodsgroup]"));
		CEntityGroup* pGoodsGroup = (CEntityGroup*)dbPlayer->GetChild(goodsGroupID);
		if(!pGoodsGroup) return;
		string containGoodsComFlag = GetComFlagByExtendID(PEI_EQUIPMENT);
		const CGUID& containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
		CEntityGroup* pContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(containerGoodsGroupID);
		if(pContainerGoodsGroup)
		{
			map<CGUID, CBaseEntity*>::iterator itr = pContainerGoodsGroup->GetEntityGroupMap().begin();
			for(; itr != pContainerGoodsGroup->GetEntityGroupMap().end(); itr++)
			{
				CEntityGroup* dbGoods = (CEntityGroup*)itr->second;

				long GPlace = dbGoods->GetLongAttr(string("place"));
				if(GPlace == PEI_EQUIPMENT)
				{
					long GPos = dbGoods->GetLongAttr(string("position"));
#ifdef __MEMORY_LEAK_CHECK__
					CBaseObject::SetConstructFlag(43);
#endif
					CGoods* tGoods=CGoodsFactory::CreateGoods(dbGoods->GetLongAttr(string("goodsIndex")),3);
					if(tGoods != NULL)
						tGoods->SetExID(const_cast<CGUID&>(dbGoods->GetGUID()));
				
					BOOL isAddOkFlag = pPlayer->m_cEquipment.AddFromDB(tGoods, GPos);
					if(isAddOkFlag == FALSE)
						CGoodsFactory::GarbageCollect(&tGoods);

					if(NULL == tGoods)
					{
						char szGuid[128];
						pPlayer->GetExID().tostring(szGuid);
						char outStr[1024];
						sprintf(outStr, "CDBPlayerToCPlayer() ID[%s] Equipment CGoods指针为空", szGuid);
						PutStringToFile("Login_WS_Info",outStr);
					}
					
						CDBGoodToCGoods(dbGoods, tGoods, dbPlayer->GetGUID());
				}
			}
		}
	}
}
void CEntityManager::DetailOrignalPackCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;
		CGUID goodsGroupID = dbPlayer->QueryChildGuidByComFlag(string("[goodsgroup]"));
		CEntityGroup* pGoodsGroup = (CEntityGroup*)dbPlayer->GetChild(goodsGroupID);
		if(!pGoodsGroup) return;

		// 背包内物品
		string containGoodsComFlag = GetComFlagByExtendID((PLAYER_EXTEND_ID)PEI_PACKET);
		CGUID containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
		CEntityGroup* pContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(containerGoodsGroupID);
		if(pContainerGoodsGroup)
		{
			map<CGUID, CBaseEntity*>::iterator itr = pContainerGoodsGroup->GetEntityGroupMap().begin();
			for(; itr != pContainerGoodsGroup->GetEntityGroupMap().end(); itr++)
			{
				CGoods* tGoods = NULL;
				CEntityGroup* dbGoods = (CEntityGroup*)itr->second;
				if(dbGoods)
				{
					long GPlace = dbGoods->GetLongAttr(string("place"));
					long GPos = dbGoods->GetLongAttr(string("position"));
					// original goods
					if(PEI_PACKET == (PLAYER_EXTEND_ID)GPlace)
					{
#ifdef __MEMORY_LEAK_CHECK__
						CBaseObject::SetConstructFlag(47);
#endif
						tGoods=CGoodsFactory::CreateGoods(dbGoods->GetLongAttr(string("goodsIndex")),4);
						if(tGoods != NULL)
						{
							tGoods->SetExID(const_cast<CGUID&>(dbGoods->GetGUID()));
						}
						
						BOOL isAddOkFlag = pPlayer->getOriginPack()->AddFromDB(tGoods, GPos);
						if(isAddOkFlag == FALSE)
							CGoodsFactory::GarbageCollect(&tGoods);

						if(NULL == tGoods)
						{
							char szGuid[128];
							pPlayer->GetExID().tostring(szGuid);
							char outStr[1024];
							sprintf(outStr, "CDBPlayerToCPlayer() ID[%s] original CGoods指针为空", szGuid);
							PutStringToFile("Login_WS_Info",outStr);
						}
						
							CDBGoodToCGoods(dbGoods, tGoods, dbPlayer->GetGUID());
					}
				}
			}
		}
	}
}
void CEntityManager::DetailSubpackCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* player, long subPackPos)
{
	if(player && pBaseEntity)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		CGUID goodsGroupID = dbPlayer->QueryChildGuidByComFlag(string("[goodsgroup]"));
		CEntityGroup* pGoodsGroup = (CEntityGroup*)dbPlayer->GetChild(goodsGroupID);
		if(!pGoodsGroup) return;
		string containGoodsComFlag = GetComFlagByExtendID((PLAYER_EXTEND_ID)(PEI_PACK));
		CGUID containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
		CEntityGroup* pContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(containerGoodsGroupID);
		if(pContainerGoodsGroup)
		{
			// 创建子背包容器对象
			map<CGUID, CBaseEntity*>::iterator itr = pContainerGoodsGroup->GetEntityGroupMap().begin();
			for(; itr != pContainerGoodsGroup->GetEntityGroupMap().end(); itr++)
			{
				CEntityGroup* dbGoods = (CEntityGroup*)itr->second;
				CGoods* tGoods = NULL;
				if(dbGoods)
				{
					long GPlace = dbGoods->GetLongAttr(string("place"));
					long GPos = dbGoods->GetLongAttr(string("position"));

					if( PEI_PACK == GPlace && (subPackPos + PEI_PACK1) == GPos )
					{
#ifdef __MEMORY_LEAK_CHECK__
						CBaseObject::SetConstructFlag(48);
#endif
						tGoods=CGoodsFactory::CreateGoods(dbGoods->GetLongAttr(string("goodsIndex")),5);
						if(tGoods != NULL)
						{
							tGoods->SetExID(const_cast<CGUID&>(dbGoods->GetGUID()));
						}
						
						BOOL isAddOkFlag = player->getSubpackContainer()->AddFromDB(tGoods, GPos);
						if(isAddOkFlag == FALSE)
							CGoodsFactory::GarbageCollect(&tGoods);

						if(NULL == tGoods)
						{
							char szGuid[128];
							player->GetExID().tostring(szGuid);
							char outStr[1024];
							sprintf(outStr, "CDBPlayerToCPlayer() ID[%s] subpackCon CGoods指针为空", szGuid);
							PutStringToFile("Login_WS_Info",outStr);
						}

						CDBGoodToCGoods(dbGoods, tGoods, dbPlayer->GetGUID());
					}
				}
			}
		}

		// 创建子背包内物品对象
		string subContainGoodsComFlag = GetComFlagByExtendID((PLAYER_EXTEND_ID)(PEI_PACK1+subPackPos));
		CGUID subContainerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(subContainGoodsComFlag);
		CEntityGroup* pSubContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(subContainerGoodsGroupID);
		if(pSubContainerGoodsGroup)
		{
			// 创建物品对象
			map<CGUID, CBaseEntity*>::iterator itr = pSubContainerGoodsGroup->GetEntityGroupMap().begin();
			for(; itr != pSubContainerGoodsGroup->GetEntityGroupMap().end(); itr++)
			{
				CEntityGroup* dbGoods = (CEntityGroup*)itr->second;
				CGoods* tGoods = NULL;
				if(dbGoods)
				{
					long GPlace = dbGoods->GetLongAttr(string("place"));
					long GPos = dbGoods->GetLongAttr(string("position"));

					
					{
#ifdef __MEMORY_LEAK_CHECK__
						CBaseObject::SetConstructFlag(48);
#endif
						tGoods = CGoodsFactory::CreateGoods(dbGoods->GetLongAttr(string("goodsIndex")),6);
						if(tGoods != NULL)
						{
							tGoods->SetExID(const_cast<CGUID&>(dbGoods->GetGUID()));
						}

						BOOL isAddOkFlag = player->getSubpackContainer()->GetSubpack(subPackPos)->pSubpackContainer->AddFromDB(tGoods, GPos);
						if(isAddOkFlag == FALSE)
							CGoodsFactory::GarbageCollect(&tGoods);

						if(NULL == tGoods)
						{
							char szGuid[128];
							player->GetExID().tostring(szGuid);
							char outStr[1024];
							sprintf(outStr, "CDBPlayerToCPlayer() ID[%s] insubpack CGoods指针为空", szGuid);
							PutStringToFile("Login_WS_Info",outStr);
						}

						CDBGoodToCGoods(dbGoods, tGoods, dbPlayer->GetGUID());
					}
				}
			}
		}
	}
}
void CEntityManager::DetailWalletCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pPlayer && pBaseEntity)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		CGUID goodsGroupID = dbPlayer->QueryChildGuidByComFlag(string("[goodsgroup]"));
		CEntityGroup* pGoodsGroup = (CEntityGroup*)dbPlayer->GetChild(goodsGroupID);
		if(!pGoodsGroup) return;
		string containGoodsComFlag = GetComFlagByExtendID(PEI_WALLET);
		CGUID containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
		CEntityGroup* pContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(containerGoodsGroupID);
		if(pContainerGoodsGroup)
		{
			// 创建子背包容器对象
			map<CGUID, CBaseEntity*>::iterator itr = pContainerGoodsGroup->GetEntityGroupMap().begin();
			for(; itr != pContainerGoodsGroup->GetEntityGroupMap().end(); itr++)
			{
				CEntityGroup* dbGoods = (CEntityGroup*)itr->second;
				CGoods* tGoods = NULL;
				if(dbGoods)
				{
					long GPlace = dbGoods->GetLongAttr(string("place"));
					long GPos = dbGoods->GetLongAttr(string("position"));

					if(PEI_WALLET == GPlace)
					{
#ifdef __MEMORY_LEAK_CHECK__
						CBaseObject::SetConstructFlag(49);
#endif
						tGoods=CGoodsFactory::CreateGoods(dbGoods->GetLongAttr(string("goodsIndex")),7);
						if(tGoods != NULL)
						{
							tGoods->SetExID(const_cast<CGUID&>(dbGoods->GetGUID()));
						}
						
						BOOL isAddOkFlag = pPlayer->m_cWallet.AddFromDB(tGoods);
						if(isAddOkFlag == FALSE)
							CGoodsFactory::GarbageCollect(&tGoods);

						if(NULL == tGoods)
						{
							char szGuid[128];
							pPlayer->GetExID().tostring(szGuid);
							char outStr[1024];
							sprintf(outStr, "CDBPlayerToCPlayer() ID[%s] wallet CGoods指针为空", szGuid);
							PutStringToFile("Login_WS_Info",outStr);
						}

						CDBGoodToCGoods(dbGoods, tGoods, dbPlayer->GetGUID());
					}
				}
			}
		}
	}
}

void CEntityManager::DetailSilverCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pPlayer && pBaseEntity)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		CGUID goodsGroupID = dbPlayer->QueryChildGuidByComFlag(string("[goodsgroup]"));
		CEntityGroup* pGoodsGroup = (CEntityGroup*)dbPlayer->GetChild(goodsGroupID);
		if(!pGoodsGroup) return;
		string containGoodsComFlag = GetComFlagByExtendID(PEI_SILVERWALLET);
		CGUID containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
		CEntityGroup* pContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(containerGoodsGroupID);
		if(pContainerGoodsGroup)
		{
			// 创建子背包容器对象
			map<CGUID, CBaseEntity*>::iterator itr = pContainerGoodsGroup->GetEntityGroupMap().begin();
			for(; itr != pContainerGoodsGroup->GetEntityGroupMap().end(); itr++)
			{
				CEntityGroup* dbGoods = (CEntityGroup*)itr->second;
				CGoods* tGoods = NULL;
				if(dbGoods)
				{
					long GPlace = dbGoods->GetLongAttr(string("place"));
					long GPos = dbGoods->GetLongAttr(string("position"));

					if(PEI_SILVERWALLET == GPlace)
					{
#ifdef __MEMORY_LEAK_CHECK__
						CBaseObject::SetConstructFlag(49);
#endif
						tGoods=CGoodsFactory::CreateGoods(dbGoods->GetLongAttr(string("goodsIndex")),8);
						if(tGoods != NULL)
						{
							tGoods->SetExID(const_cast<CGUID&>(dbGoods->GetGUID()));
						}

						BOOL isAddOkFlag = pPlayer->m_cSilverWallet.AddFromDB(tGoods);
						if(isAddOkFlag == FALSE)
							CGoodsFactory::GarbageCollect(&tGoods);

						if(NULL == tGoods)
						{
							char szGuid[128];
							pPlayer->GetExID().tostring(szGuid);
							char outStr[1024];
							sprintf(outStr, "CDBPlayerToCPlayer() ID[%s] silverwallet CGoods指针为空", szGuid);
							PutStringToFile("Login_WS_Info",outStr);
						}
					
							CDBGoodToCGoods(dbGoods, tGoods, dbPlayer->GetGUID());
					}
				}
			}
		}
	}
}
void CEntityManager::DetailQuestCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		string strBuf = "ListQuest";
		long lAttrBufSize = pBaseEntity->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* buf = (BYTE*)M_ALLOC(lAttrBufSize);
			pBaseEntity->GetBufAttr( strBuf, buf, lAttrBufSize);
			long pos = 0;
			pPlayer->DecordQuestDataFromByteArray(buf, pos);
			M_FREE(buf,lAttrBufSize);
		}
	}
}

void CEntityManager::DetailMerQuestCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;
		string strBuf = "MercenaryQuest";
		long lAttrBufSize = pBaseEntity->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* buf = (BYTE*)M_ALLOC(lAttrBufSize);
			dbPlayer->GetBufAttr(strBuf, buf, lAttrBufSize);
			long pos = 0;
			pPlayer->DecordMerQuestDataFromByteArray(buf,pos);//DecordQuestDataFromByteArray(buf, pos);
			M_FREE(buf,lAttrBufSize);
		}
	}
}

void CEntityManager::DetailSkillCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		//skills id field
		string strBuf = "ListSkill";
		long lAttrBufSize = pBaseEntity->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* buf = (BYTE*)M_ALLOC(lAttrBufSize);
			pBaseEntity->GetBufAttr(strBuf, buf, lAttrBufSize);
			list<CPlayer::tagSkill>& psl = pPlayer->GetSkillList();
			psl.clear();
			long skillNum = ((long*)buf)[0];
			for(int i=0; i<skillNum; i++)
			{
				CPlayer::tagSkill tSkill;
				memcpy(&tSkill, &buf[sizeof(long)+sizeof(CPlayer::tagSkill)*i], sizeof(CPlayer::tagSkill));
				pPlayer->GetSkillList().push_back(tSkill);
			}
			M_FREE(buf,lAttrBufSize);
		}
	}
}
void CEntityManager::DetailStateCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		string strBuf = "ListState";
		long lAttrBufSize = pBaseEntity->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* buf = (BYTE*)M_ALLOC(lAttrBufSize);
			pBaseEntity->GetBufAttr(strBuf, buf, lAttrBufSize);
			pPlayer->SetExStates(&buf[0], lAttrBufSize);
			M_FREE(buf,lAttrBufSize);
		}
	}
}
void CEntityManager::DetailVarDataCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		string strBuf = "VariableList";
		long lAttrBufSize = pBaseEntity->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* buf = (BYTE*)M_ALLOC(lAttrBufSize);
			pBaseEntity->GetBufAttr(strBuf, buf, lAttrBufSize);

			// 先删除Cplayer的老数据 
			char* tmpPtr = NULL;
			pPlayer->SetVariableData(tmpPtr);
			long varNum = ((long*)buf)[0];
			pPlayer->SetVariableNum(varNum);

			// Vars guid num
			long guidNum = ((long*)buf)[1];
			pPlayer->SetGuidNum(guidNum);

			// vardata len
			long varLen = ((long*)buf)[2];
			// Vars Data
			tmpPtr = (char*)M_ALLOC(varLen);
			memset(tmpPtr, 0, varLen);
			memcpy(tmpPtr, &buf[sizeof(long)*3], varLen);
			pPlayer->SetVariableData(tmpPtr);
			pPlayer->SetVariableDataLength(varLen);
			M_FREE(buf,lAttrBufSize);
		}
	}
}
void CEntityManager::DetailAreaCreditCDBPlayerToCPlayer	(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		string strBuf = "AreaCredit";
		long lAttrBufSize = pBaseEntity->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* creditBuf = (BYTE*)M_ALLOC(lAttrBufSize);
			pBaseEntity->GetBufAttr(strBuf, creditBuf, lAttrBufSize);
			pPlayer->ClearCredit();
			DWORD creditNum = ((DWORD*)creditBuf)[0];
			for(int i=0; i<creditNum; i++)
			{
				long areaID = *(long*)(&creditBuf[sizeof(DWORD)+sizeof(DWORD)*2*i]);
				long creditValue = *(long*)(&creditBuf[sizeof(DWORD)+sizeof(DWORD)*2*i+sizeof(DWORD)]);
				pPlayer->AddCredit(areaID, creditValue);
			}
			M_FREE(creditBuf,lAttrBufSize);
		}
	}
}

void CEntityManager::DetailQuestIndexCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		string strBuf = "QuestIndex";
		long lAttrBufSize = pBaseEntity->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* incBuf = (BYTE*)M_ALLOC(lAttrBufSize);
			pBaseEntity->GetBufAttr(strBuf, incBuf, lAttrBufSize);
			pPlayer->ClearQuestIndexMap();
			long pos = 0;
			DWORD dwQuestIndexNum = _GetDwordFromByteArray( incBuf, pos );
			for ( int i = 0; i < dwQuestIndexNum; ++i )
			{
				CQuestIndexXml::tagQuestIndexForServer QuestIndex;
				QuestIndex.lQuestID = _GetDwordFromByteArray( incBuf, pos );
				QuestIndex.lNumParam = _GetDwordFromByteArray( incBuf, pos );
				QuestIndex.iState = _GetDwordFromByteArray( incBuf, pos );
				QuestIndex.lReSetTime = _GetDwordFromByteArray( incBuf, pos );
				pPlayer->AddQuestIndex(QuestIndex);
			}
			M_FREE(incBuf,lAttrBufSize);
		}
	}
}

void CEntityManager::DetailLotteryCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		string strBuf = "Lottery";
		long lAttrBufSize = pBaseEntity->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* incBuf = (BYTE*)M_ALLOC(lAttrBufSize);
			pBaseEntity->GetBufAttr(strBuf, incBuf, lAttrBufSize);
			pPlayer->ClearVecLottery();
			long pos = 0;
			DWORD dwLotteryNum = _GetDwordFromByteArray( incBuf, pos );
			for ( int i = 0; i < dwLotteryNum; ++i )
			{
				tagLottery tagLottery;
				tagLottery.iFieldID = _GetDwordFromByteArray( incBuf, pos );
				tagLottery.dwGoodsIndex = _GetDwordFromByteArray( incBuf, pos );
				tagLottery.lConstel = _GetDwordFromByteArray( incBuf, pos );
				pPlayer->GetVecLottery()->push_back(tagLottery);
			}
			M_FREE(incBuf,lAttrBufSize);
		}
	}
}

void CEntityManager::DetailLimitGoodsRecordCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		string strBuf = "LimitGoodsRecord";
		long lAttrBufSize = pBaseEntity->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* dataBuf = (BYTE*)M_ALLOC(lAttrBufSize);
			pBaseEntity->GetBufAttr(strBuf, dataBuf, lAttrBufSize);
			pPlayer->ClearLimitGoodsRecord();
			
			DWORD dwNum = ((DWORD*)dataBuf)[0];
			for(int i=0; i<dwNum; i++)
			{
				DWORD dwGoodsId = *(DWORD*)(&dataBuf[sizeof(DWORD)+sizeof(DWORD)*2*i]);
				DWORD dwGoodsNum= *(DWORD*)(&dataBuf[sizeof(DWORD)+sizeof(DWORD)*2*i+sizeof(DWORD)]);
				pPlayer->AddLimitGoodsRecord(dwGoodsId, dwGoodsNum);
			}
			M_FREE(dataBuf,lAttrBufSize);
		}
	}
}

void CEntityManager::DetailIncGoodsCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;
		string strBuf = "IncShopCur10";
		long lAttrBufSize = dbPlayer->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* incBuf = (BYTE*)M_ALLOC(lAttrBufSize);
			dbPlayer->GetBufAttr(strBuf, incBuf, lAttrBufSize);
			pPlayer->ClearIncShopCur10();
			CPlayer::IncShopCur10Iter it=pPlayer->GetIncShopCur10().find(CIncrementShopList::TM_YUANBAO);
			assert(it!=pPlayer->GetIncShopCur10().end());
			DWORD* p_dwDataBuf=(DWORD*)incBuf;
			DWORD dwNum=*p_dwDataBuf++;
			DWORD dwVal=0;
			for(DWORD i=0;i<dwNum;i++)
			{
				dwVal=*p_dwDataBuf++;
				it->second.push_back(dwVal);
			}
			it=pPlayer->GetIncShopCur10().find(CIncrementShopList::TM_WEIMIAN);
			assert(it!=pPlayer->GetIncShopCur10().end());
			dwNum=*p_dwDataBuf++;
			for (DWORD i=0;i<dwNum;i++)
			{
				dwVal=*p_dwDataBuf++;
				it->second.push_back(dwVal);
			}
			it=pPlayer->GetIncShopCur10().find(CIncrementShopList::TM_TICKET);
			assert(it!=pPlayer->GetIncShopCur10().end());
			dwNum=*p_dwDataBuf++;
			for (DWORD i=0;i<dwNum;i++)
			{
				dwVal=*p_dwDataBuf++;
				it->second.push_back(dwVal);
			}
			M_FREE(incBuf,lAttrBufSize);
		}
	}
}

void CEntityManager::DetailIncTradeRecordCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;
		string strBuf = "IncShopBuyList";
		long lAttrBufSize = dbPlayer->GetBufSize(strBuf);
		if ( lAttrBufSize > 0 )
		{
			BYTE* incBuf = (BYTE*)M_ALLOC(lAttrBufSize);
			dbPlayer->GetBufAttr(strBuf, incBuf, lAttrBufSize);
			pPlayer->ClearIncTradeList();

			long pos = 0;
			DWORD dwTradeRecNum = _GetDwordFromByteArray( incBuf, pos );

			for ( int i = 0; i < dwTradeRecNum; ++i )
			{
				char szStr[128];
				std::string strBuyTime = _GetStringFromByteArray( incBuf, pos, szStr );
				DWORD dwPriceNum = _GetDwordFromByteArray( incBuf, pos );
				DWORD dwPriceType = _GetDwordFromByteArray( incBuf, pos );
				DWORD dwGoodsIdx = _GetDwordFromByteArray( incBuf, pos );
				DWORD dwBuyNum = _GetDwordFromByteArray( incBuf, pos );
				CIncrementShopList::IncShopTradeLog stIncTradeElem( strBuyTime, dwPriceNum, dwPriceType, dwGoodsIdx, dwBuyNum );
				pPlayer->AddIncTradeRecord( stIncTradeElem );
			}
			M_FREE(incBuf,lAttrBufSize);
		}
	}
}

void CEntityManager::DetailMedalCDBPlayerToCPlayer(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		string strBuf = "MedalData";
		long lAttrBufSize = pBaseEntity->GetBufSize(strBuf);
		if(lAttrBufSize > 0)
		{
			BYTE* pMedalData = (BYTE*)M_ALLOC(lAttrBufSize);
			pBaseEntity->GetBufAttr(strBuf, pMedalData, lAttrBufSize);
			LONG lPos = 0;
			pPlayer->m_MedalContainer.Decode(pMedalData, lPos);
			M_FREE(pMedalData,lAttrBufSize);
		}
	}
}
void CEntityManager::DetailDeOccuCPlayerToCDBPlayer	(CPlayer* pPlayer, CEntityGroup* dbPlayer)
{
	if(!pPlayer || !dbPlayer) return;
	vector<BYTE> doPBA;
	pPlayer->AddDOccuDataToByteArray(&doPBA);
	dbPlayer->SetBufAttr(string("DOccuData"), &doPBA[0], (long)doPBA.size());
}
void CEntityManager::DetailIncGoodsCPlayerToCDBPlayer	(CPlayer* pPlayer, CEntityGroup* dbPlayer)
{
	if(!pPlayer || !dbPlayer) return;
	//-------------------------------------------------------------------------------
	// 最近购买的10种商品列表. by Fox.		2008-02-25
	long lSize = pPlayer->GetIncShopCur10().size();
	if(lSize)
	{
		CPlayer::IncShopCur10Iter diamondIt=pPlayer->GetIncShopCur10().find(CIncrementShopList::TM_YUANBAO);
		assert(diamondIt!=pPlayer->GetIncShopCur10().end());
		CPlayer::IncShopCur10Iter weimianIt=pPlayer->GetIncShopCur10().find(CIncrementShopList::TM_WEIMIAN);
		assert(weimianIt!=pPlayer->GetIncShopCur10().end());
		CPlayer::IncShopCur10Iter ticketIt=pPlayer->GetIncShopCur10().find(CIncrementShopList::TM_TICKET);
		assert(ticketIt!=pPlayer->GetIncShopCur10().end());

		size_t nDiamondSize=diamondIt->second.size();
		size_t nWeimianSize=weimianIt->second.size();
		size_t nTicketSize=ticketIt->second.size();

		size_t bufSize=3*sizeof(DWORD) + nDiamondSize*sizeof(DWORD) + nWeimianSize*sizeof(DWORD) + nTicketSize*sizeof(DWORD);

		////
		BYTE* incBuf = (BYTE*)M_ALLOC(bufSize);
		DWORD* p_dwDataBuf=(DWORD*)incBuf;
		*p_dwDataBuf++=diamondIt->second.size();
		list<DWORD>::iterator idIt=diamondIt->second.begin();
		for(;idIt!=diamondIt->second.end();idIt++)
			*p_dwDataBuf++=*idIt;
		*p_dwDataBuf++=weimianIt->second.size();
		idIt=weimianIt->second.begin();
		for(;idIt!=weimianIt->second.end();idIt++)
			*p_dwDataBuf++=*idIt;
		*p_dwDataBuf++=ticketIt->second.size();
		idIt=ticketIt->second.begin();
		for(;idIt!=ticketIt->second.end();idIt++)
			*p_dwDataBuf++=*idIt;
		dbPlayer->SetBufAttr(string("IncShopCur10"), incBuf,bufSize);
		M_FREE(incBuf,bufSize);
	}
	else
	{
		DWORD tmpValue = 0;
		dbPlayer->SetBufAttr(string("IncShopCur10"), (BYTE*)&tmpValue, sizeof(DWORD));
	}
}
void CEntityManager::DetailAreaCreditCPlayerToCDBPlayer	(CPlayer* pPlayer, CEntityGroup* dbPlayer)
{
	map<DWORD,DWORD>::iterator creditIt=pPlayer->GetCreditMap()->begin();
	long creditNum = pPlayer->GetCreditMap()->size();
	long bufSize = creditNum*sizeof(DWORD)*2+sizeof(long);
	if(bufSize)
	{
		BYTE* buf =(BYTE*)M_ALLOC(bufSize);
		memcpy((BYTE*)buf, &creditNum, sizeof(long));
		long count = 0;
		for(;creditIt!=pPlayer->GetCreditMap()->end();creditIt++)
		{
			long value1 = creditIt->first;
			long value2 = creditIt->second;
			memcpy(&(((BYTE*)buf)[sizeof(long)+sizeof(DWORD)*2*count]), &value1, sizeof(DWORD));
			memcpy(&(((BYTE*)buf)[sizeof(long)+sizeof(DWORD)*2*count+sizeof(DWORD)]), &value2, sizeof(DWORD));
			count++;
		}
		dbPlayer->SetBufAttr(string("AreaCredit"), buf, bufSize);
		M_FREE(buf,bufSize);
	}
}

void CEntityManager::DetailQuestIndexCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* dbPlayer)
{
	map<long,CQuestIndexXml::tagQuestIndexForServer>::iterator creditIt=pPlayer->GetMapQuestIndex()->begin();
	long creditNum = pPlayer->GetMapQuestIndex()->size();
	vector<BYTE> vRecBuf;
	_AddToByteArray( &vRecBuf, creditNum );

	for ( ; creditIt != pPlayer->GetMapQuestIndex()->end(); ++creditIt )
	{
		CQuestIndexXml::tagQuestIndexForServer QuestIndex=creditIt->second;
		_AddToByteArray( &vRecBuf, (DWORD)QuestIndex.lQuestID );
		_AddToByteArray( &vRecBuf, (DWORD)QuestIndex.lNumParam );
		_AddToByteArray( &vRecBuf, (DWORD)QuestIndex.iState );
		_AddToByteArray( &vRecBuf, (DWORD)QuestIndex.lReSetTime );
	}
	DWORD dwBufSize = vRecBuf.size();
	if ( dwBufSize )
	{
		BYTE* buf=(BYTE*)M_ALLOC(dwBufSize);
		for ( int i = 0; i < dwBufSize; ++i )
		{
			buf[i] = vRecBuf[i];
		}
		dbPlayer->SetBufAttr(string("QuestIndex"),buf,dwBufSize);
		M_FREE(buf,dwBufSize);
	}
}

void CEntityManager::DetailLotteryCPlayerToCDBPlayer(CPlayer* pPlayer, CEntityGroup* dbPlayer)
{

	vector<tagLottery>::iterator creditIt=pPlayer->GetVecLottery()->begin();
	long creditNum = pPlayer->GetVecLottery()->size();
	vector<BYTE> vRecBuf;
	_AddToByteArray( &vRecBuf, creditNum );

	for ( ; creditIt != pPlayer->GetVecLottery()->end(); ++creditIt )
	{
		_AddToByteArray( &vRecBuf, (DWORD)creditIt->iFieldID);
		_AddToByteArray( &vRecBuf, (DWORD)creditIt->dwGoodsIndex);
		_AddToByteArray( &vRecBuf, (DWORD)creditIt->lConstel);
	}
	DWORD dwBufSize = vRecBuf.size();
	if ( dwBufSize )
	{
		BYTE* buf=(BYTE*)M_ALLOC(dwBufSize);
		for ( int i = 0; i < dwBufSize; ++i )
		{
			buf[i] = vRecBuf[i];
		}
		dbPlayer->SetBufAttr(string("Lottery"),buf,dwBufSize);
		M_FREE(buf,dwBufSize);
	}
}

void CEntityManager::DetailLimitGoodsRecordCPlayerToCDBPlayer(CPlayer* pPlayer,CEntityGroup* dbPlayer)
{
	if(!pPlayer)
		return;
	map<DWORD,DWORD>::iterator recordIt=pPlayer->GetLimitGoodsMap().begin();
	DWORD dwRecordNum=pPlayer->GetLimitGoodsMap().size();
	DWORD dwBufSize=dwRecordNum*sizeof(DWORD)*2 + sizeof(DWORD);
	if(dwBufSize)
	{
		BYTE* buf=(BYTE*)M_ALLOC(dwBufSize);
		memcpy((BYTE*)buf,&dwRecordNum,sizeof(DWORD));
		int nCount=0;
		for(;recordIt!=pPlayer->GetLimitGoodsMap().end();recordIt++)
		{
			DWORD dwGoodsId=recordIt->first;
			DWORD dwGoodsNum=recordIt->second;
			memcpy(&(((BYTE*)buf)[sizeof(long)+sizeof(DWORD)*2*nCount]), &dwGoodsId, sizeof(DWORD));
			memcpy(&(((BYTE*)buf)[sizeof(long)+sizeof(DWORD)*2*nCount+sizeof(DWORD)]), &dwGoodsNum, sizeof(DWORD));
			nCount++;
		}
		dbPlayer->SetBufAttr(string("LimitGoodsRecord"),buf,dwBufSize);
		M_FREE(buf,dwBufSize);
	}
}

void CEntityManager::DetailIncTradeRecordCPlayerToCDBPlayer(CPlayer* pPlayer,CEntityGroup* dbPlayer)
{
	if(!pPlayer)
		return;
	CIncrementShopList::VEC_BUYLIST& vecBuyList = pPlayer->GetIncTradeList();
	DWORD dwTradeRecNum = vecBuyList.size();
	CIncrementShopList::ITR_BUYLIST it = vecBuyList.begin();
	vector<BYTE> vRecBuf;
	_AddToByteArray( &vRecBuf, dwTradeRecNum );
	for ( ; it != vecBuyList.end(); ++it )
	{
		_AddToByteArray( &vRecBuf, it->strBuyTime.c_str() );
		_AddToByteArray( &vRecBuf, it->dwPriceNum );
		_AddToByteArray( &vRecBuf, it->dwPriceType );
		_AddToByteArray( &vRecBuf, it->dwGoodsIdx );
		_AddToByteArray( &vRecBuf, it->dwBuyNum );
	}
	DWORD dwBufSize = vRecBuf.size();
	if ( dwBufSize )
	{
		BYTE* buf=(BYTE*)M_ALLOC(dwBufSize);
		for ( int i = 0; i < dwBufSize; ++i )
		{
			buf[i] = vRecBuf[i];
		}
		dbPlayer->SetBufAttr(string("IncShopBuyList"),buf,dwBufSize);
		M_FREE(buf,dwBufSize);
	}
}

void CEntityManager::DetailGoodsCooldownCPlayerToCDBPlayer	(CPlayer* player, CEntityGroup* dbPlayer)
{
	DWORD dwClassIdNum = player->GetClassIdCooldownMap().size();
	DWORD dwGoodsIdNum=player->GetGoodsIdCooldownMap().size();
	DWORD dwCooldownId=0,dwCooldownTime=0;
	long bufSize = dwClassIdNum*sizeof(DWORD)*2+dwGoodsIdNum*sizeof(DWORD)*2 + sizeof(DWORD)*2;
	long loffset=0;
	if(bufSize)
	{
		BYTE* buf = (BYTE*)M_ALLOC(bufSize);
		memcpy((BYTE*)buf,&dwClassIdNum,sizeof(DWORD));
		loffset+=sizeof(DWORD);
		map<DWORD,DWORD>::iterator cooldownIter=player->GetClassIdCooldownMap().begin();
		for(;cooldownIter!=player->GetClassIdCooldownMap().end();cooldownIter++)
		{
			dwCooldownId=cooldownIter->first;
			dwCooldownTime=cooldownIter->second;
			memcpy(&(((BYTE*)buf)[loffset]),&dwCooldownId,sizeof(DWORD));
			loffset+=sizeof(DWORD);
			memcpy(&(((BYTE*)buf)[loffset]),&dwCooldownTime,sizeof(DWORD));
			loffset+=sizeof(DWORD);
		}		
		memcpy(&(((BYTE*)buf)[loffset]),&dwGoodsIdNum,sizeof(DWORD));
		loffset+=sizeof(DWORD);
		cooldownIter=player->GetGoodsIdCooldownMap().begin();
		for(;cooldownIter!=player->GetGoodsIdCooldownMap().end();cooldownIter++)
		{
			dwCooldownId=cooldownIter->first;
			dwCooldownTime=cooldownIter->second;
			memcpy(&(((BYTE*)buf)[loffset]),&dwCooldownId,sizeof(DWORD));
			loffset+=sizeof(DWORD);
			memcpy(&(((BYTE*)buf)[loffset]),&dwCooldownTime,sizeof(DWORD));
			loffset+=sizeof(DWORD);
		}
		dbPlayer->SetBufAttr(string("GoodsCooldown"), buf, bufSize);
		M_FREE(buf,bufSize);
	}
}

void CEntityManager::DetailMedalCPlayerToCDBPlayer(CPlayer* player, CEntityGroup* pBaseEntity)
{
	if(pBaseEntity && player)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		vector<BYTE> vMedalData;
		player->m_MedalContainer.AddToByteArray(vMedalData);
		if(vMedalData.size())
		{
			dbPlayer->SetBufAttr(string("MedalData"), (BYTE*)&vMedalData[0], (long)vMedalData.size());
		}
	}
}

void CEntityManager::DetailOrignalDepotCDBPlayerToCPlayer	(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;
		CGUID goodsGroupID = dbPlayer->QueryChildGuidByComFlag(string("[goodsgroup]"));
		CEntityGroup* pGoodsGroup = (CEntityGroup*)dbPlayer->GetChild(goodsGroupID);
		if(!pGoodsGroup) return;
		// 背包内物品
		string containGoodsComFlag = GetComFlagByExtendID((PLAYER_EXTEND_ID)PEI_DCT_Primary);
		CGUID containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
		CEntityGroup* pContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(containerGoodsGroupID);
		if(pContainerGoodsGroup)
		{
			// 创建子背包容器对象
			map<CGUID, CBaseEntity*>::iterator itr = pContainerGoodsGroup->GetEntityGroupMap().begin();
			for(; itr != pContainerGoodsGroup->GetEntityGroupMap().end(); itr++)
			{
				CGoods* tGoods = NULL;
				CEntityGroup* dbGoods = (CEntityGroup*)itr->second;
				if(dbGoods)
				{
					long GPlace = dbGoods->GetLongAttr(string("place"));
					long GPos = dbGoods->GetLongAttr(string("position"));
					// original goods
					if(PEI_DCT_Primary == (PLAYER_EXTEND_ID)GPlace)
					{
#ifdef __MEMORY_LEAK_CHECK__
						CBaseObject::SetConstructFlag(47);
#endif
						tGoods=CGoodsFactory::CreateGoods(dbGoods->GetLongAttr(string("goodsIndex")),9);
						if(tGoods != NULL)
						{
							tGoods->SetExID(const_cast<CGUID&>(dbGoods->GetGUID()));
						}
						
						BOOL isAddOkFlag = pPlayer->m_PlayerDepot.FindContainer(PEI_DCT_Primary)->AddFromDB(tGoods, GPos);
						if(isAddOkFlag == FALSE)
							CGoodsFactory::GarbageCollect(&tGoods);

						if(NULL == tGoods)
						{
							char szGuid[128];
							pPlayer->GetExID().tostring(szGuid);
							char outStr[1024];
							sprintf(outStr, "CDBPlayerToCPlayer() ID[%s] depot CGoods指针为空", szGuid);
							PutStringToFile("Login_WS_Info",outStr);
						}
						
							CDBGoodToCGoods(dbGoods, tGoods, dbPlayer->GetGUID());
					}
				}
			}
		}
	}
}
void CEntityManager::DetailSubDepotCDBPlayerToCPlayer		(CEntityGroup* pBaseEntity, CPlayer* pPlayer, long subPackPos)
{
	if(pPlayer && pBaseEntity)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;
		CGUID goodsGroupID = dbPlayer->QueryChildGuidByComFlag(string("[goodsgroup]"));
		CEntityGroup* pGoodsGroup = (CEntityGroup*)dbPlayer->GetChild(goodsGroupID);
		if(!pGoodsGroup) return;
		
		// 创建仓库子背包容器对象
		string containGoodsComFlag = GetComFlagByExtendID((PLAYER_EXTEND_ID)eDCT_Secondary);
		CGUID containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
		CEntityGroup* pContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(containerGoodsGroupID);
		if(pContainerGoodsGroup)
		{
			// 创建子背包容器对象
			map<CGUID, CBaseEntity*>::iterator itr = pContainerGoodsGroup->GetEntityGroupMap().begin();
			for(; itr != pContainerGoodsGroup->GetEntityGroupMap().end(); itr++)
			{
				CGoods* tGoods = NULL;
				CEntityGroup* dbGoods = (CEntityGroup*)itr->second;
				if(dbGoods)
				{
					long GPlace = dbGoods->GetLongAttr(string("place"));
					long GPos = dbGoods->GetLongAttr(string("position"));
					//! 仓库子背包
					if(eDCT_Secondary == GPlace)
					{
						if((subPackPos + PEI_PACK1) == GPos ) //是子背包
						{
#ifdef __MEMORY_LEAK_CHECK__
							CBaseObject::SetConstructFlag(45);
#endif
							tGoods=CGoodsFactory::CreateGoods(dbGoods->GetLongAttr(string("goodsIndex")),10);
							if(tGoods != NULL)
							{
								tGoods->SetExID(const_cast<CGUID&>(dbGoods->GetGUID()));
							}
							
							BOOL isAddOkFlag = pPlayer->m_PlayerDepot.FindContainer(eDCT_Secondary)->AddFromDB(tGoods, GPos);
							if(isAddOkFlag == FALSE)
								CGoodsFactory::GarbageCollect(&tGoods);

							if(NULL == tGoods)
							{
								char szGuid[128];
								pPlayer->GetExID().tostring(szGuid);
								char outStr[1024];
								sprintf(outStr, "CDBPlayerToCPlayer() ID[%s] subdepotCon CGoods指针为空", szGuid);
								PutStringToFile("Login_WS_Info",outStr);
							}

							CDBGoodToCGoods(dbGoods, tGoods, dbPlayer->GetGUID());
						}
					}
				}
			}
		}

		// 创建子背包内物品对象
		string subContainGoodsComFlag = GetComFlagByExtendID((PLAYER_EXTEND_ID)(PEI_DCT_Secondary1+subPackPos));
		CGUID subContainerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(subContainGoodsComFlag);
		CEntityGroup* pSubContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(subContainerGoodsGroupID);
		if(pSubContainerGoodsGroup)
		{
			// 创建物品对象
			map<CGUID, CBaseEntity*>::iterator itr = pSubContainerGoodsGroup->GetEntityGroupMap().begin();
			for(; itr != pSubContainerGoodsGroup->GetEntityGroupMap().end(); itr++)
			{
				CEntityGroup* dbGoods = (CEntityGroup*)itr->second;
				CGoods* tGoods = NULL;
				if(dbGoods)
				{
					long GPlace = dbGoods->GetLongAttr(string("place"));
					long GPos = dbGoods->GetLongAttr(string("position"));


					{
#ifdef __MEMORY_LEAK_CHECK__
						CBaseObject::SetConstructFlag(48);
#endif
						tGoods=CGoodsFactory::CreateGoods(dbGoods->GetLongAttr(string("goodsIndex")),11);
						if(tGoods != NULL)
						{
							tGoods->SetExID(const_cast<CGUID&>(dbGoods->GetGUID()));
						}
						
						BOOL isAddOkFlag = pPlayer->m_PlayerDepot.FindContainer(PEI_DCT_Secondary1+subPackPos)->AddFromDB(tGoods, GPos);
						if(isAddOkFlag == FALSE)
							CGoodsFactory::GarbageCollect(&tGoods);

						if(NULL == tGoods)
						{
							char szGuid[128];
							pPlayer->GetExID().tostring(szGuid);
							char outStr[1024];
							sprintf(outStr, "CDBPlayerToCPlayer() ID[%s] insubdepot CGoods指针为空", szGuid);
							PutStringToFile("Login_WS_Info",outStr);
						}

						CDBGoodToCGoods(dbGoods, tGoods, dbPlayer->GetGUID());
					}
				}
			}
		}
	}
}
void CEntityManager::DetailDepotWalletCDBPlayerToCPlayer	(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pPlayer && pBaseEntity)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		CGUID goodsGroupID = dbPlayer->QueryChildGuidByComFlag(string("[goodsgroup]"));
		CEntityGroup* pGoodsGroup = (CEntityGroup*)dbPlayer->GetChild(goodsGroupID);
		if(!pGoodsGroup) return;
		string containGoodsComFlag = GetComFlagByExtendID(PEI_DCT_Gold);
		CGUID containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
		CEntityGroup* pContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(containerGoodsGroupID);
		if(pContainerGoodsGroup)
		{
			// 创建子背包容器对象
			map<CGUID, CBaseEntity*>::iterator itr = pContainerGoodsGroup->GetEntityGroupMap().begin();
			for(; itr != pContainerGoodsGroup->GetEntityGroupMap().end(); itr++)
			{
				CEntityGroup* dbGoods = (CEntityGroup*)itr->second;
				CGoods* tGoods = NULL;
				if(dbGoods)
				{
					long GPlace = dbGoods->GetLongAttr(string("place"));
					long GPos = dbGoods->GetLongAttr(string("position"));

					if(PEI_DCT_Gold == GPlace)
					{
#ifdef __MEMORY_LEAK_CHECK__
						CBaseObject::SetConstructFlag(49);
#endif
						tGoods=CGoodsFactory::CreateGoods(dbGoods->GetLongAttr(string("goodsIndex")),12);
						if(tGoods != NULL)
						{
							tGoods->SetExID(const_cast<CGUID&>(dbGoods->GetGUID()));
						}
						
						BOOL isAddOkFlag = pPlayer->m_PlayerDepot.FindContainer(eDCT_Gold)->AddFromDB(tGoods);
						if(isAddOkFlag == FALSE)
							CGoodsFactory::GarbageCollect(&tGoods);

						if(NULL == tGoods)
						{
							char szGuid[128];
							pPlayer->GetExID().tostring(szGuid);
							char outStr[1024];
							sprintf(outStr, "CDBPlayerToCPlayer() ID[%s] depotwallet CGoods指针为空", szGuid);
							PutStringToFile("Login_WS_Info",outStr);
						}

						CDBGoodToCGoods(dbGoods, tGoods, dbPlayer->GetGUID());
					}
				}
			}
		}
	}
}
void CEntityManager::DetailDepotSilverCDBPlayerToCPlayer	(CEntityGroup* pBaseEntity, CPlayer* pPlayer)
{
	if(pPlayer && pBaseEntity)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		CGUID goodsGroupID = dbPlayer->QueryChildGuidByComFlag(string("[goodsgroup]"));
		CEntityGroup* pGoodsGroup = (CEntityGroup*)dbPlayer->GetChild(goodsGroupID);
		if(!pGoodsGroup) return;
		string containGoodsComFlag = GetComFlagByExtendID(PEI_DCT_Silver);
		CGUID containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
		CEntityGroup* pContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(containerGoodsGroupID);
		if(pContainerGoodsGroup)
		{
			// 创建子背包容器对象
			map<CGUID, CBaseEntity*>::iterator itr = pContainerGoodsGroup->GetEntityGroupMap().begin();
			for(; itr != pContainerGoodsGroup->GetEntityGroupMap().end(); itr++)
			{
				CEntityGroup* dbGoods = (CEntityGroup*)itr->second;
				CGoods* tGoods = NULL;
				if(dbGoods)
				{
					long GPlace = dbGoods->GetLongAttr(string("place"));
					long GPos = dbGoods->GetLongAttr(string("position"));

					if(PEI_DCT_Silver == GPlace)
					{
#ifdef __MEMORY_LEAK_CHECK__
						CBaseObject::SetConstructFlag(49);
#endif
						tGoods=CGoodsFactory::CreateGoods(dbGoods->GetLongAttr(string("goodsIndex")),13);
						if(tGoods != NULL)
						{
							tGoods->SetExID(const_cast<CGUID&>(dbGoods->GetGUID()));
						}
						
						BOOL isAddOkFlag = pPlayer->m_PlayerDepot.FindContainer(eDCT_Silver)->AddFromDB(tGoods);
						if(isAddOkFlag == FALSE)
							CGoodsFactory::GarbageCollect(&tGoods);

						if(NULL == tGoods)
						{
							char szGuid[128];
							pPlayer->GetExID().tostring(szGuid);
							char outStr[1024];
							sprintf(outStr, "CDBPlayerToCPlayer() ID[%s] depotsilverwallet CGoods指针为空", szGuid);
							PutStringToFile("Login_WS_Info",outStr);
						}
						
						CDBGoodToCGoods(dbGoods, tGoods, dbPlayer->GetGUID());
					}
				}
			}
		}
	}
}

// 玩家的设置的功能映射热键
void CEntityManager::DetailFuncHotKeyCPlayerToCDBPlayer( CPlayer *pPlayer, CEntityGroup *pBaseEntity )
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;
		MAPFKEY& mapkey = pPlayer->GetCurMapKey();
		long lkeyNum = mapkey.size();
		long buffsize = lkeyNum*(sizeof(MAPKEY)+sizeof(long)) + sizeof(long);
		BYTE* buf = (BYTE*)M_ALLOC(buffsize);	

#ifdef _RUNSTACKINFO_
		char info[256];
		sprintf(info, "Detail FuncHotKey[size:%d,num:%d] CPlayer To CDBPlayer Start.", buffsize, lkeyNum);
		CMessage::AsyWriteFile(GetGame()->GetStatckFileName(), info);
#endif
		vector<BYTE> vMapBuf;
		_AddToByteArray(&vMapBuf,(long)(lkeyNum));
		MAPFKEYITER itr = mapkey.begin();

		for(; itr != mapkey.end(); itr++)
		{
			_AddToByteArray(&vMapBuf,(long)itr->first);
			_AddToByteArray(&vMapBuf,(void*)&(itr->second),sizeof(MAPKEY));				
		}
		for (int i=0;i<buffsize;++i)
		{
			buf[i] = vMapBuf[i];
		}
		dbPlayer->SetBufAttr(string("HotKeyVector"), buf, (long)buffsize);
		M_FREE(buf,buffsize);
#ifdef _RUNSTACKINFO_
		sprintf(info, "Detail FuncHotKey CPlayer To CDBPlayer vMapBuf[size:%d] End.", vMapBuf.size());
		CMessage::AsyWriteFile(GetGame()->GetStatckFileName(), info);
#endif
	}
}

void CEntityManager::DetailFuncHotKeyCDBPlayerToCPlayer( CEntityGroup *pBaseEntity, CPlayer *pPlayer )
{
	if(pBaseEntity && pPlayer)
	{
		CEntityGroup* dbPlayer = (CEntityGroup*)pBaseEntity;

		string strBuf = "HotKeyVector";
		long lAttrBufSize = dbPlayer->GetBufSize(strBuf);

#ifdef _RUNSTACKINFO_
		char info[128];
		sprintf(info, "Detail FuncHotKey[size:%d] CDBPlayer To CPlayer Start.", lAttrBufSize);
		CMessage::AsyWriteFile(GetGame()->GetStatckFileName(), info);
#endif
		if(lAttrBufSize > 0)
		{
			BYTE* buf = (BYTE*)M_ALLOC(lAttrBufSize);
			dbPlayer->GetBufAttr(strBuf, buf, lAttrBufSize);
			MAPFKEY& mapkey = pPlayer->GetCurMapKey();
			mapkey.clear();
			long pos = 0;
			long lNum = _GetLongFromByteArray(buf,pos); 
			for(int i=0; i<lNum; i++)
			{
				MAPKEY key;
				long index = _GetLongFromByteArray(buf,pos);
				_GetBufferFromByteArray(buf,pos,&key,sizeof(MAPKEY));
				CFuncKeyMapSystem::InsertKeyToCurMap(mapkey,(FKMS)index,key);
			}
			M_FREE(buf,lAttrBufSize);
		}
#ifdef _RUNSTACKINFO_
		CMessage::AsyWriteFile(GetGame()->GetStatckFileName(),"Detail FuncHotKey CDBPlayer To CPlayer End.");
#endif
	}
}

void CEntityManager::DetailBusinessPackCPlayerToCDBPlayer( CPlayer *player, CEntityGroup *pDBPlayer )
{
	if( player != NULL && pDBPlayer != NULL )
	{
		// 清除商业背包内物品节点
		ClearPlayerContainerMap( pDBPlayer, PEI_BUSINESSPACK );	
		// 清楚商业背包栏位所有节点（只有1个节点）
		ClearPlayerContainerMap( pDBPlayer, PEI_BUSINESSPACKFIELD );

		// 编码跑商背包自身（物品）
		CGoods *pSelfGoods = player->GetBusinessPackage().m_pGoods;
		if( pSelfGoods != NULL )
		{
			// 跑商背包栏位只有一个位置0 
			AddGoodsToPlayer( pDBPlayer, pSelfGoods, 0, PEI_BUSINESSPACKFIELD );
		}

		// 编码物品列表
		typedef stdext::hash_map<CGUID, CGoods*, hash_guid_compare> GoodsListType;
		GoodsListType *goods_list = player->GetBusinessPackage().m_pContainer->GetGoodsMap();
		for( GoodsListType::iterator it = goods_list->begin(); it != goods_list->end(); ++ it )
		{
			DWORD dwPos = 0;
			player->GetBusinessPackage().m_pContainer->QueryGoodsPosition( it->second, dwPos );
			// 编码物品到entity group
			AddGoodsToPlayer( pDBPlayer, it->second, dwPos, PEI_BUSINESSPACK );
		}
	}
}

void CEntityManager::DetailBusinessPackCDBPlayerToCPlayer( CEntityGroup *pDBPlayer, CPlayer *player )
{
	if( pDBPlayer != NULL && player != NULL )
	{
		// 获取商业背包父节点
		const CGUID &goodsGroupID = pDBPlayer->QueryChildGuidByComFlag( "[goodsgroup]" );
		CEntityGroup* pGoodsGroup = (CEntityGroup*)pDBPlayer->GetChild( goodsGroupID );
		if( pGoodsGroup == NULL )
		{
			// failed
			return;
		}
		
		typedef std::map<CGUID, CBaseEntity*> EntityGroupTable;

		// 获取商业背包栏位节点
		const CGUID &field_id = pGoodsGroup->QueryChildGuidByComFlag( "[gp_businesspack_field]" );
		CEntityGroup *pField = (CEntityGroup*)pGoodsGroup->GetChild( field_id );
		if( pField != NULL )
		{
			EntityGroupTable &pack_list =pField->GetEntityGroupMap();
			for( EntityGroupTable::iterator it = pack_list.begin(); it != pack_list.end(); ++ it )
			{
				CBaseEntity *entity = it->second;
				if( entity == NULL )
				{
					continue;
				}

				long lPlace = entity->GetLongAttr( "place" );
				long lPos = entity->GetLongAttr( "position" );
				if( lPlace == PEI_BUSINESSPACKFIELD && lPos == 0 )
				{
					// 商业背包物品自身
					CGoods *pGoods = CGoodsFactory::CreateGoods( 
						entity->GetLongAttr( "goodsIndex" ),14 );
					if( pGoods == NULL )
					{
						BUSINESS_LOG_WARNING( Business::FMT_STR( "Create business package goods [%d] failed",
							entity->GetLongAttr( "goodsIndex" ) ) );
						continue;
					}
					pGoods->SetExID( const_cast<CGUID&>( entity->GetGUID() ) );
					CDBGoodToCGoods( entity, pGoods, pDBPlayer->GetGUID() );

					// TEMP: 更新缓存数据
					// TODO: 回收已有物品
					if( player->GetBusinessPackage().m_pGoods != NULL )
					{
						BUSINESS_LOG_WARNING( Business::FMT_STR( "Player [%s] business package goods does exist already!",
								player->GetName() ) );
					}
					player->GetBusinessPackage().DecodeFromDB( pGoods );
				}
			}
		}

		// 获取商业背包数据节点
		const CGUID &businessContainerID = pGoodsGroup->QueryChildGuidByComFlag( "[gp_businesspack]" );
		CEntityGroup* pBusinessContainerGroup = (CEntityGroup*)pGoodsGroup->GetChild( businessContainerID );

		//
		if( pBusinessContainerGroup != NULL )
		{
			typedef std::map<CGUID, CBaseEntity*> EntityGroupTable;
			EntityGroupTable &goods_list = pBusinessContainerGroup->GetEntityGroupMap();
			for( EntityGroupTable::iterator it = goods_list.begin(); it != goods_list.end(); ++ it )
			{
				CBaseEntity *entity_goods = it->second;
				if( entity_goods != NULL )
				{
					long lPlace = entity_goods->GetLongAttr( "place" );
					long lPos = entity_goods->GetLongAttr( "position" );
					
					if( lPlace == PEI_BUSINESSPACK )
					{
						// 放置于商业背包内的物品
						CGoods *pGoods = CGoodsFactory::CreateGoods(
							entity_goods->GetLongAttr( "goodsIndex" ),15 );
						if( pGoods == NULL )
						{
							BUSINESS_LOG_WARNING( Business::FMT_STR( "Create goods [%d] failed loaded from db",
								entity_goods->GetLongAttr( "goodsIndex" ) ) );
							continue;
						}
						pGoods->SetExID( const_cast<CGUID&>( entity_goods->GetGUID() ) );
						// 添加进背包
						
						BOOL isAddOkFlag = player->GetBusinessPackage().m_pContainer->AddFromDB( pGoods, lPos );
						if(isAddOkFlag == FALSE)
							CGoodsFactory::GarbageCollect(&pGoods);

						if(NULL == pGoods)
						{
							char szGuid[128];
							player->GetExID().tostring(szGuid);
							char outStr[1024];
							sprintf(outStr, "CDBPlayerToCPlayer() ID[%s] businesspack CGoods指针为空", szGuid);
							PutStringToFile("Login_WS_Info",outStr);
						}

						// 解码物品详细数据
						CDBGoodToCGoods( entity_goods, pGoods, pDBPlayer->GetGUID() );
					}
				}
			} // for
		}// if
	}// if
}

// 加载玩家宠物列表
bool CEntityManager::LoadPlayerPet(CEntityGroup* pDBPlayer, CPlayer* pPlayer)
{
	if( 0 == GetInst(CPetCtrl).GetFuncSwitch() )
		return true;

	CEntityGroup* pPetList = (CEntityGroup*)FindLeafByComFlag(pDBPlayer, string("[playerpetlist]"));
	if( pPetList != NULL && pPlayer != NULL )
	{
		return GetInst(CPetCtrl).DecodeFromDataBlock(pPetList, pPlayer);
	}

	return true;
}

// 存储玩家宠物列表
bool CEntityManager::SavePlayerPet(CPlayer* pPlayer, CEntityGroup* pDBPlayer)
{
	if( 0 == GetInst(CPetCtrl).GetFuncSwitch() )
		return true;

	// 保存宠物数据
	CEntityGroup* pPetList = (CEntityGroup*)GetGame()->GetEntityManager()->FindLeafByComFlag(pDBPlayer, "[playerpetlist]");
	if( NULL != pPetList )
	{
		pPetList->SetCurDbOperFlag(1);
		pPetList->SetCurDbOperType(DB_OPER_DELETE_INSERT);
		pPetList->ReleaseChilds();
		return GetInst(CPetCtrl).CodeToDataBlock(pPlayer, pPetList);
	}
	return true;
}

// 根据PLAYER_EXTEND_ID取得Entity的类型值
string CEntityManager::GetComFlagByExtendID(PLAYER_EXTEND_ID containerID)
{
	switch(containerID)
	{
	case PEI_PACKET:
		return string("[gp_packet]");
	case PEI_EQUIPMENT:
		return string("[gp_equip]");
	case PEI_WALLET:
		return string("[gp_gold]");
	case PEI_SILVERWALLET:
		return string("[gp_silver]");
	case PEI_MEDAL_CONTAINER:
		return string("[gp_packet]");
	case PEI_PACK:
		return string("[gp_pack]");
	case PEI_PACK1:
		return string("[gp_subpackone]");
	case PEI_PACK2:
		return string("[gp_subpacktwo]");
	case PEI_PACK3:
		return string("[gp_subpackthree]");
	case PEI_PACK4:
		return string("[gp_subpackfour]");
	case PEI_PACK5:
		return string("[gp_subpackfive]");
	case PEI_DCT_Gold:
		return string("[gp_depotgold]");
	case PEI_DCT_Silver:
		return string("[gp_depotsilver]");
	case PEI_DCT_Primary:
		return string("[gp_depot]");
	case PEI_DCT_Secondary:
		return string("[gp_depotsecondary]");
	case PEI_DCT_Secondary1:
		return string("[gp_subdepotone]");
	case PEI_DCT_Secondary2:
		return string("[gp_subdepottwo]");
	case PEI_DCT_Secondary3:
		return string("[gp_subdepotthree]");
	case PEI_DCT_Secondary4:
		return string("[gp_subdepotfour]");
	case PEI_BUSINESSPACK:
		return string("[gp_businesspack]" );
	case PEI_BUSINESSPACKFIELD:
		return string( "[gp_businesspack_field]" );
	}
	return string("NULL");
}
// 清空装备物品map
void CEntityManager::ClearPlayerContainerMap(CEntityGroup* pEntity, PLAYER_EXTEND_ID containerID)
{
	if(!pEntity) return;
	const CGUID& goodsGroupID = pEntity->QueryChildGuidByComFlag(string("[goodsgroup]"));
	CEntityGroup* pGoodsGroup = (CEntityGroup*)pEntity->GetChild(goodsGroupID);
	if(!pGoodsGroup) return;
	string containGoodsComFlag = GetComFlagByExtendID(containerID);
	const CGUID& containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
	pGoodsGroup->ClearLeafChilds(containerGoodsGroupID);
}

// 清除子背包对象
void CEntityManager::ClearPlayerSubpackSelfMap(CEntityGroup* pEntity, long subPos)
{
	if(!pEntity) return;
	const CGUID& goodsGroupID = pEntity->QueryChildGuidByComFlag(string("[goodsgroup]"));
	CEntityGroup* pGoodsGroup = (CEntityGroup*)pEntity->GetChild(goodsGroupID);
	if(!pGoodsGroup) return;
	string containGoodsComFlag = GetComFlagByExtendID(PEI_PACK);
	const CGUID& containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
	CEntityGroup* pContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(containerGoodsGroupID);
	if(!pContainerGoodsGroup) return;

	map<CGUID, CBaseEntity*>::iterator goodsItr = pContainerGoodsGroup->GetEntityGroupMap().begin();
	while(goodsItr != pContainerGoodsGroup->GetEntityGroupMap().end())
	{
		CEntityGroup* packGoods = (CEntityGroup*)goodsItr->second;
		if(packGoods)
		{
			long pos = packGoods->GetLongAttr(string("position"));
			if(pos == PEI_PACK1+subPos)
			{
				DelBaseEntity((CBaseEntity*)goodsItr->second);
				goodsItr = pContainerGoodsGroup->GetEntityGroupMap().erase(goodsItr);
				return;
			}
			goodsItr++;
		}
	}
}
void CEntityManager::ClearPlayerSubDepotSelfMap(CEntityGroup* pEntity, long subPos)
{
	if(!pEntity) return;
	const CGUID& goodsGroupID = pEntity->QueryChildGuidByComFlag(string("[goodsgroup]"));
	CEntityGroup* pGoodsGroup = (CEntityGroup*)pEntity->GetChild(goodsGroupID);
	if(!pGoodsGroup) return;
	string containGoodsComFlag = GetComFlagByExtendID(PEI_DCT_Secondary);
	const CGUID& containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
	CEntityGroup* pContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(containerGoodsGroupID);
	if(!pContainerGoodsGroup) return;

	map<CGUID, CBaseEntity*>::iterator goodsItr = pContainerGoodsGroup->GetEntityGroupMap().begin();
	while(goodsItr != pContainerGoodsGroup->GetEntityGroupMap().end())
	{
		CEntityGroup* packGoods = (CEntityGroup*)goodsItr->second;
		if(packGoods)
		{
			long pos = packGoods->GetLongAttr(string("position"));
			if(pos == PEI_PACK1+subPos)
			{
				DelBaseEntity((CBaseEntity*)goodsItr->second);
				goodsItr = pContainerGoodsGroup->GetEntityGroupMap().erase(goodsItr);
				return;
			}
			goodsItr++;
		}
	}
}
// 添加一个物品到玩家容器
void CEntityManager::AddGoodsToPlayer(CEntityGroup* pEntity, CGoods* pGoods, long pos, PLAYER_EXTEND_ID containerID)
{
	if(!pEntity || !pGoods) return;
	const CGUID& goodsGroupID = pEntity->QueryChildGuidByComFlag(string("[goodsgroup]"));
	CEntityGroup* pGoodsGroup = (CEntityGroup*)pEntity->GetChild(goodsGroupID);
	if(!pGoodsGroup) return;
	
	pGoodsGroup->SetCurDbOperFlag(1);
	string containGoodsComFlag = GetComFlagByExtendID(containerID);
	const CGUID& containerGoodsGroupID = pGoodsGroup->QueryChildGuidByComFlag(containGoodsComFlag);
	if(containerGoodsGroupID == NULL_GUID)
	{
		AddLogText("AddGoodsToPlayer:容器ID为NULL!请重新检查代码!");
		return;
	}
	CEntityGroup* pContainerGoodsGroup = (CEntityGroup*)pGoodsGroup->GetChild(containerGoodsGroupID);
	if(!pContainerGoodsGroup)
	{
		pContainerGoodsGroup->SetCurDbOperFlag(1);
		pContainerGoodsGroup = (CEntityGroup*)GetGame()->GetEntityManager()->NewBaseEntity(containGoodsComFlag, containerGoodsGroupID);
		pGoodsGroup->AddChild(pContainerGoodsGroup);
	}

	CEntity* tGoods = (CEntity*)GetGame()->GetEntityManager()->NewBaseEntity(string("[goods]"), pGoods->GetExID());
	tGoods->SetLongAttr(string("place"), (long)containerID);
	tGoods->SetLongAttr(string("position"), (long)pos);
	CGoodsToCDBGood(pGoods, tGoods, pEntity->GetGUID());
	pContainerGoodsGroup->AddChild(tGoods);
}
// 计算LoginPlayer的删除时间
long CEntityManager::ComputeLoginPlayerDelTime(CEntityGroup* pBaseEntity)
{
	if(pBaseEntity)
	{
		long m_DelTime = -1;
		// 计算删除时间
		long delTime[6];
		pBaseEntity->GetTimeAttr(string("DelDate"), delTime, sizeof(long)*6);

		if(delTime[0] == 0 && delTime[1] == 0 && delTime[2] == 0
			&& delTime[3] == 0 && delTime[4] == 0 && delTime[5] == 0)
		{
			return m_DelTime = -1;
		}

		time_t tCurTime = time(0);
		tm *pCurTime = localtime(&tCurTime);

		tm tt;
		memset(&tt,0, sizeof(tm));
		tt.tm_year= delTime[0]-1900; tt.tm_mon = delTime[1]-1; tt.tm_mday = delTime[2];
		DWORD res = mktime(&tt);
		if(res == -1)
		{
			return m_DelTime = -1;
		}

		double dDiffSecond = difftime(tCurTime, res);
		short days = -short(dDiffSecond/86400.0);
		m_DelTime = days+1;
		if(m_DelTime < 0) m_DelTime = 0; 
	}
	return -1;
}
// 设置玩家节点上某些子节点的数据库操作标志位
void CEntityManager::SetEntityLeavesDbOperFlag(CBaseEntity* pBaseEntity, const string& leafComFlag, bool operFlag)
{
	if(!pBaseEntity) return;
	if(pBaseEntity->GetCompositeType() != COM_COMPOSITE) return;

	CEntityGroup* pGoodsGroup = NULL;
	map<string, CGUID>::iterator guidItr = ((CEntityGroup*)pBaseEntity)->GetGuidByComFlagMap().find(leafComFlag);
	if(guidItr != ((CEntityGroup*)pBaseEntity)->GetGuidByComFlagMap().end())
	{
		map<CGUID, CBaseEntity*>::iterator gpItr = ((CEntityGroup*)pBaseEntity)->GetEntityGroupMap().find(guidItr->second);
		if(gpItr != ((CEntityGroup*)pBaseEntity)->GetEntityGroupMap().end())
			gpItr->second->SetCurDbOperFlag(operFlag);
	}
}
// 将玩家节点上所有子节点数据库操作标志位清零
void CEntityManager::ResetEntityAllDbOperFlag(CBaseEntity* pBaseEntity, bool flag)
{
	if(!pBaseEntity) return;
	
	pBaseEntity->SetCurDbOperFlag(flag);

	// 不是叶子节点
	if(pBaseEntity->GetCompositeType() != COM_COMPOSITE) return;

	CEntityGroup* pGroup = (CEntityGroup*)pBaseEntity;
	pGroup->SetCurDbOperFlag(flag);
	map<CGUID, CBaseEntity*>::iterator gpItr = pGroup->GetEntityGroupMap().begin();
	for(; gpItr != pGroup->GetEntityGroupMap().end(); gpItr++)
	{
		ResetEntityAllDbOperFlag(gpItr->second, flag);
	}
}
// 设置从根结点开始的所有节点数据库操作标志
void CEntityManager::SetEntityAllDbOperType(CBaseEntity* pBaseEntity, DB_OPERATION_TYPE opType)
{
	if(!pBaseEntity) return;

	pBaseEntity->SetCurDbOperType(opType);

	// 不是叶子节点
	if(pBaseEntity->GetCompositeType() != COM_COMPOSITE) return;

	pBaseEntity->SetCurDbOperType(opType);
	map<CGUID, CBaseEntity*>::iterator gpItr = ((CEntityGroup*)pBaseEntity)->GetEntityGroupMap().begin();
	for(; gpItr != ((CEntityGroup*)pBaseEntity)->GetEntityGroupMap().end(); gpItr++)
	{
		SetEntityAllDbOperType(gpItr->second, opType);
	}
}

// 根据叶子类型查找其父节点
CEntityGroup* CEntityManager::FindLeafFatherByComFlag(CEntityGroup* pRoot, const string& leafComFlag)
{
	if(pRoot)
	{
		if( pRoot->GetLeafComFlag() == leafComFlag ) 
			return pRoot;

		if(pRoot->GetCompositeType() == COM_COMPOSITE)
		{
			map<CGUID, CBaseEntity*>::iterator itr = pRoot->GetEntityGroupMap().begin();
			for(; itr != pRoot->GetEntityGroupMap().end(); itr++)
			{
				if(((CEntityGroup*)itr->second)->GetLeafComFlag() == leafComFlag)
					return (CEntityGroup*)itr->second;

				if(itr->second->GetCompositeType() == COM_COMPOSITE)
				{
					CEntityGroup* retEntity = (CEntityGroup*)FindLeafFatherByComFlag((CEntityGroup*)itr->second, leafComFlag);
					if(retEntity)
						return retEntity;
				}
			}
		}
	}
	return NULL;
}
// 根据叶子类型在其根结点下查找其节点
CBaseEntity* CEntityManager::FindLeafByComFlag(CEntityGroup* pRoot, const string& leafComFlag)
{
	if(pRoot)
	{
		if(pRoot->GetCompositeType() == COM_COMPOSITE)
		{
			map<CGUID, CBaseEntity*>::iterator itr = pRoot->GetEntityGroupMap().begin();
			for(; itr != pRoot->GetEntityGroupMap().end(); itr++)
			{
				if((itr->second)->GetCompositeFlag() == leafComFlag)
				{
					return itr->second;
				}

				if(itr->second->GetCompositeType() == COM_COMPOSITE)
				{
					CBaseEntity* retEntity = FindLeafByComFlag((CEntityGroup*)itr->second, leafComFlag);
					if(retEntity)
						return retEntity;
				}
			}
		}
	}
	return NULL;
}
// 映射属性字符串名和枚举值名
void CEntityManager::AddAttrEnumAndStr(const string& strName, long lEnum)
{
	// trick : 记录枚举值的最大值
	if(m_lAttrEnumValue <= lEnum)
		m_lAttrEnumValue = lEnum;

	m_mapAttrEnumToStr[lEnum] = strName;
}
long CEntityManager::GetAttrEnumByStr(const string& strComFlag, const string& strName)
{
	AttrDefMapItr itr = m_mapObjAttrDef.find(strComFlag);
	if(itr != m_mapObjAttrDef.end())
	{
		map<string, tagEntityAttrInfo>::iterator attrItr = itr->second->pDataMgr.find(strName);
		if(attrItr != itr->second->pDataMgr.end())
			return (long)attrItr->second.lAttrTypeEnum;
	}
	return -1;
}
const string& CEntityManager::GetAttrStrByEnum(long lEnum)
{
	map<long, string>::iterator itr = m_mapAttrEnumToStr.find(lEnum);
	if(itr != m_mapAttrEnumToStr.end())
		return itr->second;
	return NULL_STRING;
}
// 取得配置文件的数据信息
tagEntityBuildInfo* CEntityManager::GetEntityBuildInfo(const string& strComFlag)
{
	AttrDefMapItr itr = m_mapObjAttrDef.find(strComFlag);
	if(itr != m_mapObjAttrDef.end())
	{
		return itr->second;
	}
	return NULL;
}
// DATA_OBJECT_TYPE
long CEntityManager::GetDataObjectType(const string& strComFlag, const string& strName)
{
	AttrDefMapItr itr = m_mapObjAttrDef.find(strComFlag);
	if(itr != m_mapObjAttrDef.end())
	{
		map<string, tagEntityAttrInfo>::iterator attrItr = itr->second->pDataMgr.find(strName);
		if(attrItr != itr->second->pDataMgr.end())
			return attrItr->second.eNumDataType;
	}
	return (long)DATA_OBJECT_UNKNOWN;
}
long CEntityManager::GetDataObjectType(const string& strComFlag, long lEnum)
{
	return GetDataObjectType(strComFlag, GetAttrStrByEnum(lEnum));
}
// 数据库属性标志位: 1|0: 1(数据库自动操作) 0程序操作(默认)
BYTE CEntityManager::GetDbUseageFlag(const string& strComFlag, const string& strName)
{
	AttrDefMapItr itr = m_mapObjAttrDef.find(strComFlag);
	if(itr != m_mapObjAttrDef.end())
	{
		map<string, tagEntityAttrInfo>::iterator attrItr = itr->second->pDataMgr.find(strName);
		if(attrItr != itr->second->pDataMgr.end())
			return attrItr->second.lUseage;
	}
	return 0;
}
BYTE CEntityManager::GetDbUseageFlag(const string& strComFlag, long lEnum)
{
	return GetDbUseageFlag(strComFlag, GetAttrStrByEnum(lEnum));
}
// 数据块最大长度
long CEntityManager::GetBufMaxSize(const string& strComFlag, const string& strName)
{
	AttrDefMapItr itr = m_mapObjAttrDef.find(strComFlag);
	if(itr != m_mapObjAttrDef.end())
	{
		map<string, tagEntityAttrInfo>::iterator attrItr = itr->second->pDataMgr.find(strName);
		if(attrItr != itr->second->pDataMgr.end())
			return attrItr->second.lMaxSize;
	}
	return 0;
}
long CEntityManager::GetBufMaxSize(const string& strComFlag, long lEnum)
{
	return GetBufMaxSize(strComFlag, GetAttrStrByEnum(lEnum));
}
CBaseEntity* CEntityManager::NewBaseEntity(const string& strComFlag, const CGUID& guid)
{
	CBaseEntity* pEntity = NULL;
	tagEntityBuildInfo* pInfo = GetEntityBuildInfo(strComFlag);
	if(pInfo)
	{
		if(pInfo->lComType == COM_COMPOSITE)
			pEntity = MP_NEW CEntityGroup(strComFlag, guid);
		else
			pEntity = MP_NEW CEntity(strComFlag, guid);
	}
	m_lNewDbPlayerNum++;
	return pEntity;
}
void CEntityManager::DelBaseEntity(CBaseEntity* pEntity)
{
	m_lNewDbPlayerNum--;
	MP_DELETE(pEntity);
}

//! 输出对象类型及个数
void CEntityManager::OutEntityInfo(VOID)
{
	char szInfo[1024 * 16] = {0};

	char szNum[32] = {0};
	strcpy(szInfo, "DBEntity Info:\r\n");
	DBEntityComponentMapItr enMapItr = GetGame()->GetEntityManager()->GetBaseEntityMap().begin();
	for(; enMapItr != GetGame()->GetEntityManager()->GetBaseEntityMap().end(); enMapItr++)
	{
		strcat(szInfo, enMapItr->first.c_str());
		strcat(szInfo, " Num=");
		map<CGUID,CEntityGroup*>::iterator tmpitr = enMapItr->second.begin();
		if(tmpitr != enMapItr->second.end())
		{
			itoa(tmpitr->second->GetEntityGroupMap().size(), szNum, 10);
			strcat(szInfo, szNum);
			strcat(szInfo, "\r\n");
		}
		else
{
			itoa(0, szNum, 10);
			strcat(szInfo, szNum);
			strcat(szInfo, "\r\n");
		}
	}

	strcat(szInfo, "Account Num=");
	itoa(GetGame()->GetEntityManager()->GetAccountMap().size(), szNum, 10);
	strcat(szInfo, szNum);
	strcat(szInfo, "\r\n");

	strcat(szInfo, "New Entity Num=");
	itoa(m_lNewDbPlayerNum, szNum, 10);
	strcat(szInfo, szNum);
	strcat(szInfo, "\r\n");

	PutStringToFile("MemAllocInfo", szInfo);
}

void CEntityManager::CreateChangeNameProcSession(const CGUID& guid, const char* szName, long gsscoketid)
{
	// 检查玩家是否处于会话中
	if(!szName || guid == NULL_GUID) return;

	//##创建会话
	CWorldServerSession* pSession			= (CWorldServerSession*)GetGame()->GetSessionFactoryInst()->CreateSession(1800000, ST_WS_SESSION);
	if( pSession && pSession->Start() )
	{
		pSession->AddOneSessionStep(CWorldServerSession::WORLD_SESSION_CHANGE_NAME);
		pSession->SendChangeNameProc(guid, szName, gsscoketid);
	}
}
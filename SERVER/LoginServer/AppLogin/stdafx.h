// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���ǳ��õ��������ĵ���Ŀ�ض��İ����ļ�
//

#pragma once
#pragma warning(disable:4018)
#pragma warning(disable:4996)
#pragma warning(disable:4311)
#pragma warning(disable:4312)
#pragma warning(disable:4244)

#define WIN32_LEAN_AND_MEAN		// �� Windows ͷ���ų�����ʹ�õ�����

// TODO: �ڴ˴����ó���Ҫ��ĸ���ͷ�ļ�
//#define DEBUG_NEW new
#define _WIN32_DCOM
#define _WIN32_WINNT 0x0501

#define _RUNSTACKINFO_

#include <Windows.h>
#include <objbase.h>
#include <assert.h>
#include <time.h>
#include <MMSystem.h>

// ��׼ģ���
#include <list>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <deque>
#include <set>

#include "..\public\public.h"
#include "..\public\tools.h"
#include "..\public\GUID.h"
#include "..\public\debugnew.h"
#include "..\public\MsgType.h"
#include "LoginServer\game.h"

#include "..\public\ClientResource.h"
#include "..\public\RFile.h"

using namespace std;
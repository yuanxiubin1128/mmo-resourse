#include "StdAfx.h"
#include "..\..\frostengine\utility.h"
#include "..\..\frostengine\render.h"
#include "..\..\frostengine\input.h"
#include "..\..\frostengine\ui.h"
#include "..\..\gameengine\gamemodel.h"
#include "..\..\gameengine\gamemodelmanager.h"
#include "..\..\..\public\tools.h"
#include "light.h"
#include "wndviewer.h"

class CGameModelManager;

SunLight::SunLight(void)
{
	m_bDrag = FALSE;
	m_pGameModel = CGameModelManager::GetInstance()->CreateDisplayModel("ui/arrow/model");
	m_pGameModel->ShowAllGroup();

	m_pAnimInfo = new AnimInfo;
	ZeroMemory(m_pAnimInfo->GetActionInfo(),sizeof(AnimInfo::tagActionInfo));
	m_pAnimInfo->SetAnimTime(GetCurTickCount());
	m_pAnimInfo->GetActionInfo()->dwCurAction = MAKEFOURCC('S','T','N','D');
	m_pAnimInfo->GetActionInfo()->dwCurActionStartTime = GetCurTickCount();

	D3DXQuaternionIdentity( &m_qOld);
	D3DXQuaternionIdentity( &m_qNow );
	D3DXMatrixIdentity(&m_mRotSnapshot);
	D3DXMatrixIdentity(&m_mRot);

	m_vOldVector = D3DXVECTOR3(0,0,0);
	m_vCurVector = D3DXVECTOR3(0,0,0);
	m_vDefDirection = D3DXVECTOR3(0.2f,-0.8f,-0.4f);
	m_vCurDirection = D3DXVECTOR3(0.2f,-0.8f,-0.4f);

	m_fRadius = 5.0f;
}

SunLight::~SunLight(void)
{
	//SAFEDESTROY(m_pGameModel);
	CGameModelManager::GetInstance()->ReleaseDisplayModel(m_pGameModel);
	CGameModelManager::GetInstance()->ReleaseGameModel(m_pGameModel->GetGameModelID());
	m_pGameModel = NULL;
	SAFEDELETE(m_pAnimInfo);
}

void SunLight::HandleMessage(const D3DXMATRIX *pViewMatrix)
{
	render::Interface *pInterface = render::Interface::GetInstance();
	float fW = (float)pInterface->GetWndWidth();
	float fH = (float)pInterface->GetWndHeight();

	ui::Manager::_tagINPUTINFO *pInputInfo = ui::Manager::GetInstance()->GetInputInfoBuffer();

	switch(pInputInfo->eType)
	{
	case UIMT_MS_BTNDOWN:
		if (pInputInfo->dwData == MK_RBUTTON && (pInputInfo->byKeyBuffer[DIK_LALT] & 0x80))
		{
			m_bDrag = TRUE;
			m_qOld = m_qNow;
			ScreenToVector(pInputInfo->ptMouse.x,pInputInfo->ptMouse.y,(int)fW,(int)fH,1.0f,&m_vOldVector);
		}
		break;
	case UIMT_MS_MOVE:
		if (m_bDrag)
		{
			ScreenToVector(pInputInfo->ptMouse.x,pInputInfo->ptMouse.y,(int)fW,(int)fH,1.0f,&m_vCurVector);
			m_qNow = m_qOld * QuatFromBallPoints( m_vOldVector, m_vCurVector );	

			D3DXMATRIX mInvView;
			D3DXMatrixInverse(&mInvView, NULL, pViewMatrix);
			mInvView._41 = mInvView._42 = mInvView._43 = 0;

			D3DXMATRIX mLastRotInv;
			D3DXMatrixInverse(&mLastRotInv, NULL, &m_mRotSnapshot);

			D3DXMATRIX mRot;
			D3DXMatrixRotationQuaternion(&mRot, &m_qNow);
			m_mRotSnapshot = mRot;

			// Accumulate the delta of the arcball's rotation in view space.
			// Note that per-frame delta rotations could be problematic over long periods of time.
			m_mRot *= *pViewMatrix * mLastRotInv * mRot * mInvView;

			// Since we're accumulating delta rotations, we need to orthonormalize 
			// the matrix to prevent eventual matrix skew
			D3DXVECTOR3* pXBasis = (D3DXVECTOR3*) &m_mRot._11;
			D3DXVECTOR3* pYBasis = (D3DXVECTOR3*) &m_mRot._21;
			D3DXVECTOR3* pZBasis = (D3DXVECTOR3*) &m_mRot._31;
			D3DXVec3Normalize( pXBasis, pXBasis );
			D3DXVec3Cross( pYBasis, pZBasis, pXBasis );
			D3DXVec3Normalize( pYBasis, pYBasis );
			D3DXVec3Cross( pZBasis, pXBasis, pYBasis );

			//Transform the default direction vector by the light's rotation matrix
			D3DXVec3TransformNormal( &m_vCurDirection, &m_vDefDirection, &m_mRot );
		}
		break;
	}
	if (!(pInputInfo->dwMouseButtonState & MK_RBUTTON))
	{
		m_bDrag = FALSE;	
	}
}

void SunLight::ScreenToVector( int iScreenPtX, int iScreenPtY ,int iScreenWidth,int iScreenHeight,float fRadius,D3DXVECTOR3 *pVector)
{
	// Scale to screen

	float x = (float)iScreenWidth/2;
	x = -((float)iScreenPtX - x)  / (fRadius*x);
	float y =  (float)iScreenHeight/2;
	y = ((float)iScreenPtY - y) / (fRadius*y);

	float z   = x*x + y*y;

	if( z > 1.0f )
	{
		FLOAT scale = 1.0f/sqrtf(z);
		x *= scale;
		y *= scale;
		z = 0.0f;
	}
	else
		z = sqrtf( 1.0f - z );

	// Return vector
	pVector->x = x;
	pVector->y = y;
	pVector->z = z;
}

void SunLight::Render()
{
	render::Interface *pInterface = render::Interface::GetInstance();

	// Rotate arrow model to point towards origin
	D3DXMATRIX mRotateA, mRotateB,mRotate,mScale,mTrans;
	D3DXVECTOR3 vAt = D3DXVECTOR3(0,0,0);
	D3DXVECTOR3 vUp = D3DXVECTOR3(0,1,0);
	D3DXMatrixRotationX( &mRotateB, D3DX_PI );
	D3DXMatrixLookAtLH( &mRotateA, &vAt,&m_vCurDirection, &vUp );
	D3DXMatrixInverse( &mRotateA, NULL, &mRotateA );
	mRotate = mRotateB * mRotateA;

	D3DXVec3Normalize(&m_vCurDirection,&m_vCurDirection);
	D3DXVECTOR3 vL =  - m_vCurDirection * m_fRadius * 1.0f;
	D3DXMatrixTranslation( &mTrans, vL.x, vL.y, vL.z );
	D3DXMatrixScaling( &mScale, 1.0f, 1.0f, 1.0f );

	D3DXMATRIX mWorld = mRotate * mScale * mTrans;

	render::Camera* pcamera = ((WndViewer*)ui::Manager::GetInstance()->GetMainWnd())->GetCamera();

	m_pAnimInfo->SetCurrentTime(timeGetTime());
	m_pGameModel->SetDirLightEnable(false);
	m_pAnimInfo->SetWorldMatrix(&mWorld);
	m_pAnimInfo->SetViewMatrix(pcamera->GetViewMatrix());
	m_pAnimInfo->SetProjectedMatrix(pcamera->GetProjectionMatrix());
	m_pGameModel->ProcessAnimJoint(m_pAnimInfo);
//	m_pGameModel->SetModelColor(0XFF888888);
	//m_pGameModel->ProcessPointLight(&D3DXVECTOR3(-1.0f,-1.0f,1.0f),1.0f,0xFFFFFFaa);

	m_pGameModel->Display(m_pAnimInfo);
}

D3DXQUATERNION SunLight::QuatFromBallPoints(const D3DXVECTOR3 &vFrom, const D3DXVECTOR3 &vTo)
{
	D3DXVECTOR3 vPart;
	float fDot = D3DXVec3Dot(&vFrom, &vTo);
	D3DXVec3Cross(&vPart, &vFrom, &vTo);

	return D3DXQUATERNION(vPart.x, vPart.y, vPart.z, fDot);
}

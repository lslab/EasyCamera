/*
	Copyright (c) 2013-2015 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/
#include "EasyMediaSource.h"
#include "QTSServerInterface.h"
#include "QTSServer.h"

static unsigned int sLastVPTS = 0;
static unsigned int sLastAPTS = 0;

//���������Ƶ���ݻص�
HI_S32 NETSDK_APICALL OnStreamCallback(	HI_U32 u32Handle,		/* ��� */
										HI_U32 u32DataType,     /* �������ͣ���Ƶ����Ƶ���ݻ�����Ƶ�������� */
										HI_U8*  pu8Buffer,      /* ���ݰ���֡ͷ */
										HI_U32 u32Length,		/* ���ݳ��� */
										HI_VOID* pUserData		/* �û�����*/
									)						
{
	HI_S_AVFrame* pstruAV = HI_NULL;
	HI_S_SysHeader* pstruSys = HI_NULL;
	EasyMediaSource* pThis = (EasyMediaSource*)pUserData;

	if (u32DataType == HI_NET_DEV_AV_DATA)
	{
		pstruAV = (HI_S_AVFrame*)pu8Buffer;

		if (pstruAV->u32AVFrameFlag == HI_NET_DEV_VIDEO_FRAME_FLAG)
		{
			//qtss_printf("   PTS:%d increase:%d --\n",pstruAV->u32AVFramePTS, pstruAV->u32AVFramePTS - sLastVPTS);
			
			////qtss_printf("u32Length %u PTS: %u Len: %u \n", u32Length, pstruAV->u32AVFramePTS,pstruAV->u32AVFrameLen);
			//��Ƶ֡����
			//fwrite(pu8Buffer+sizeof(HI_S_AVFrame),1,pstruAV->u32AVFrameLen,pThis->_f_264);
			//���͸�MediaSink���ж���/����/����

			//ǿ��Ҫ���һ֡ΪI�ؼ�֡
			if(pThis->m_bForceIFrame)
			{
				if(pstruAV->u32VFrameType == HI_NET_DEV_VIDEO_FRAME_I)
					pThis->m_bForceIFrame = false;
				else
					return HI_SUCCESS;
			}

			unsigned int vInter = pstruAV->u32AVFramePTS - sLastVPTS;

			sLastVPTS = pstruAV->u32AVFramePTS;
			//pThis->GetMediaSink()->PushPacket((char*)pu8Buffer, u32Length);
		}
		else if (pstruAV->u32AVFrameFlag == HI_NET_DEV_AUDIO_FRAME_FLAG)
		{
			//pThis->GetMediaSink()->PushPacket((char*)pu8Buffer, u32Length);
		}
	}	

	return HI_SUCCESS;
}

	
HI_S32 NETSDK_APICALL OnEventCallback(	HI_U32 u32Handle,	/* ��� */
										HI_U32 u32Event,	/* �¼� */
										HI_VOID* pUserData  /* �û�����*/
                                )
{
	//if(HI_NET_DEV_NORMAL_DISCONNECTED == u32Event)
	//	pSamnetlibDlg->AlartData();
	//qtss_printf("Event Callback\n");
	return HI_SUCCESS;
}

HI_S32 NETSDK_APICALL OnDataCallback(	HI_U32 u32Handle,		/* ��� */
										HI_U32 u32DataType,		/* ��������*/
										HI_U8*  pu8Buffer,      /* ���� */
										HI_U32 u32Length,		/* ���ݳ��� */
										HI_VOID* pUserData		/* �û�����*/
                                )
{
	//CSamnetlibDlg *pSamnetlibDlg = (CSamnetlibDlg*)pUserData;
	//qtss_printf("Data Callback\n");
	//pSamnetlibDlg->AlartData();
	return HI_SUCCESS;
}


EasyMediaSource::EasyMediaSource()
:	Task(),
	m_u32Handle(0),
	fCameraLogin(false),//�Ƿ��ѵ�¼��ʶ
	m_bStreamFlag(false),//�Ƿ�������ý���ʶ
	m_bForceIFrame(true)
{
	//SDK��ʼ����ȫ�ֵ���һ��
	HI_NET_DEV_Init();
	this->Signal(Task::kStartEvent);
}

EasyMediaSource::~EasyMediaSource(void)
{
	qtss_printf("~EasyMediaSource\n");
	
	//��ֹͣStream���ڲ����Ƿ���Stream���ж�
	NetDevStopStream();

	if(fCameraLogin)
		HI_NET_DEV_Logout(m_u32Handle);

	//SDK�ͷţ�ȫ�ֵ���һ��
	HI_NET_DEV_DeInit();
}

bool EasyMediaSource::CameraLogin()
{
	//����ѵ�¼������true
	if(fCameraLogin) return true;
	//��¼�������
	HI_S32 s32Ret = HI_SUCCESS;
	s32Ret = HI_NET_DEV_Login(	&m_u32Handle,
		QTSServerInterface::GetServer()->GetPrefs()->GetRunUserName(),
		QTSServerInterface::GetServer()->GetPrefs()->GetRunPassword(),
		QTSServerInterface::GetServer()->GetPrefs()->GetLocalCameraAddress(),
		80);

	if (s32Ret != HI_SUCCESS)
	{
		qtss_printf("HI_NET_DEV_Login Fail\n");
		m_u32Handle = 0;
		return false;
	}
	else
	{
		HI_NET_DEV_SetReconnect(m_u32Handle, 5000);
		fCameraLogin = true;
	}

	return true;
}

QTSS_Error EasyMediaSource::NetDevStartStream()
{
	//���δ��¼,����ʧ��
	if(!CameraLogin()) return QTSS_RequestFailed;
	
	//�Ѿ����������У�����Easy_AttrNameExists
	if(m_bStreamFlag) return QTSS_AttrNameExists;

	OSMutexLocker locker(this->GetMutex());

	QTSS_Error theErr = QTSS_NoErr;
	HI_S32 s32Ret = HI_SUCCESS;
	HI_S_STREAM_INFO struStreamInfo;

	HI_NET_DEV_SetEventCallBack(m_u32Handle, (HI_ON_EVENT_CALLBACK)OnEventCallback, this);
	HI_NET_DEV_SetStreamCallBack(m_u32Handle, (HI_ON_STREAM_CALLBACK)OnStreamCallback, this);
	HI_NET_DEV_SetDataCallBack(m_u32Handle, (HI_ON_DATA_CALLBACK)OnDataCallback, this);

	struStreamInfo.u32Channel = HI_NET_DEV_CHANNEL_1;
	struStreamInfo.blFlag = HI_FALSE;
	struStreamInfo.u32Mode = HI_NET_DEV_STREAM_MODE_TCP;
	struStreamInfo.u8Type = HI_NET_DEV_STREAM_ALL;
	s32Ret = HI_NET_DEV_StartStream(m_u32Handle, &struStreamInfo);
	if (s32Ret != HI_SUCCESS)
	{
		qtss_printf("HI_NET_DEV_StartStream Fail\n");
		return QTSS_RequestFailed;
	}

	m_bStreamFlag = true;
	m_bForceIFrame = true;
	qtss_printf("HI_NET_DEV_StartStream SUCCESS\n");

	return QTSS_NoErr;
}

void EasyMediaSource::NetDevStopStream()
{
	if( m_bStreamFlag )
	{
		qtss_printf("HI_NET_DEV_StopStream\n");
		HI_NET_DEV_StopStream(m_u32Handle);
		m_bStreamFlag = false;
		//m_bForceIFrame = false;
	}
}

void EasyMediaSource::handleClosure(void* clientData) 
{
	if(clientData != NULL)
	{
		EasyMediaSource* source = (EasyMediaSource*)clientData;
		source->handleClosure();
	}
}

void EasyMediaSource::handleClosure() 
{
	if (fOnCloseFunc != NULL) 
	{
		(*fOnCloseFunc)(fOnCloseClientData);
	}
}

void EasyMediaSource::stopGettingFrames() 
{
	OSMutexLocker locker(this->GetMutex());
	fOnCloseFunc = NULL;
	doStopGettingFrames();
}

void EasyMediaSource::doStopGettingFrames() 
{
	qtss_printf("doStopGettingFrames()\n");
	NetDevStopStream();
}

bool EasyMediaSource::GetSnapData(unsigned char* pBuf, UInt32 uBufLen, int* uSnapLen)
{
	//��������δ��¼������false
	if(!CameraLogin()) return false;

	//����SDK��ȡ����
	HI_S32 s32Ret = HI_FAILURE; 
	s32Ret = HI_NET_DEV_SnapJpeg(m_u32Handle, (HI_U8*)pBuf, uBufLen, uSnapLen);
	if(s32Ret == HI_SUCCESS)
	{
		return true;
	}

	return false;
}

SInt64 EasyMediaSource::Run()
{
	printf("EasyMediaSource::Run Test\n");

	QTSS_Error nRet = QTSS_NoErr;

	do{
		//���豸��ȡ��������
		unsigned char *sData = (unsigned char*)malloc(EASY_SNAP_BUFFER_SIZE);
		int snapBufLen = 0;

		//�����ȡ��������������ݣ�Base64����/����
		if(!GetSnapData(sData, EASY_SNAP_BUFFER_SIZE, &snapBufLen))
		{
			//δ��ȡ������
			qtss_printf("EasyDeviceCenter::UpdateDeviceSnap => Get Snap Data Fail \n");
			nRet = QTSS_ValueNotFound;
			break;
		}

		QTSServer* svr = (QTSServer*)QTSServerInterface::GetServer();
		svr->GetCMSApi()->UpdateSnap((const char*)sData, snapBufLen);

		free((void*)sData);
		sData = NULL;

	}while(0);

	return 10*1000;
}
	
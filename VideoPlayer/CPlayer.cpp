#include "pch.h"
#include "CPlayer.h"


void CPlayer::AudiopInitCoreAudio() noexcept
{
	m_pAudioDeviceEnum.CreateInstance(CLSID_MMDeviceEnumerator);
	m_pAudioDeviceEnum->GetDefaultAudioEndpoint(
		eRender, eMultimedia, m_pAudioDevice.AddrOfClear());
	m_pAudioDevice->Activate(IID_IAudioClient, CLSCTX_ALL,
		nullptr, (void**)m_pAudioClient.AddrOfClear());

	WAVEFORMATEX* pwfx;
	m_pAudioClient->GetMixFormat(&pwfx);

	pwfx->nSamplesPerSec = m_pAudioCodecCtx->sample_rate;
	pwfx->nChannels = m_cAudioChannels;
	pwfx->nAvgBytesPerSec = pwfx->nSamplesPerSec *
		pwfx->nChannels * pwfx->wBitsPerSample / 8;
	pwfx->wFormatTag = WAVE_FORMAT_EXTENSIBLE;

	m_pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
		1000 * 10000/* 1秒 */, 0, pwfx, nullptr);
	m_pAudioClient->GetBufferSize(&m_cAudioBuffer);
	m_pAudioClient->GetService(IID_IAudioRenderClient,
		(void**)m_pAudioRenderClient.AddrOfClear());

	CoTaskMemFree(pwfx);
}

float* CPlayer::AudiopGetBuffer(_Inout_ UINT32& cRequested) noexcept
{
	UINT32 cPadding;
	m_pAudioClient->GetBufferSize(&m_cAudioBuffer);
	m_pAudioClient->GetCurrentPadding(&cPadding);
	if (cRequested > m_cAudioBuffer - cPadding)
	{
		m_pAudioClient->Stop();
		m_pAudioClient->Reset();
		m_pAudioClient->Start();
		if (cRequested > m_cAudioBuffer)
			cRequested = m_cAudioBuffer;
	}
	BYTE* pBuffer;
	m_pAudioRenderClient->GetBuffer(cRequested, &pBuffer);
	return (float*)pBuffer;
}

void CPlayer::AudiopReleaseBuffer(UINT32 cWritten) noexcept
{
	m_pAudioRenderClient->ReleaseBuffer(cWritten, 0);
}

CPlayer::~CPlayer()
{
	if (m_pFmtCtx)
		avformat_close_input(&m_pFmtCtx);
	if (m_pVideoCodecCtx)
		avcodec_free_context(&m_pVideoCodecCtx);
	if (m_pAudioCodecCtx)
		avcodec_free_context(&m_pAudioCodecCtx);
	if (m_pHwDeviceCtx)
		av_buffer_unref(&m_pHwDeviceCtx);
}

int CPlayer::OpenFile(PCSTR pszPathU8) noexcept
{
	int r;
	if (r = avformat_open_input(&m_pFmtCtx, pszPathU8, nullptr, nullptr))
		return r;
	if (r = avformat_find_stream_info(m_pFmtCtx, nullptr))
		return r;
	m_pVideoStream = nullptr;
	m_pAudioStream = nullptr;
	EckCounter(m_pFmtCtx->nb_streams, i)
	{
		const auto pStream = m_pFmtCtx->streams[i];
		const AVCodec* pCodec;
		switch (pStream->codecpar->codec_type)
		{
		case AVMEDIA_TYPE_VIDEO:
		{
			if (m_pVideoStream)
				break;
			m_pVideoStream = pStream;
			pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
			if (m_pVideoCodecCtx)
				avcodec_free_context(&m_pVideoCodecCtx);
			m_pVideoCodecCtx = avcodec_alloc_context3(pCodec);
			if (!m_pVideoCodecCtx)
				return AVERROR(ENOMEM);
			avcodec_parameters_to_context(m_pVideoCodecCtx, pStream->codecpar);
			if (!m_pHwDeviceCtx)
			{
				AVBufferRef* pHwCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
				if (!pHwCtx)
					return AVERROR(ENOMEM);
				const auto pCtxD3D = (AVD3D11VADeviceContext*)
					((AVHWDeviceContext*)pHwCtx->data)->hwctx;
				pCtxD3D->device = m_pD3DDevice.Get();
				pCtxD3D->device_context = m_pD3DContext.Get();
				pCtxD3D->lock = nullptr;
				pCtxD3D->unlock = nullptr;
				m_pD3DDevice->AddRef();
				m_pD3DContext->AddRef();

				if (r = av_hwdevice_ctx_init(pHwCtx))
				{
					av_buffer_unref(&pHwCtx);
					return r;
				}
				m_pHwDeviceCtx = pHwCtx;
			}
			m_pVideoCodecCtx->hw_device_ctx = av_buffer_ref(m_pHwDeviceCtx);
			m_pVideoCodecCtx->get_format = [](AVCodecContext*, const AVPixelFormat* pPixFmts)
				{
					for (auto p = pPixFmts; *p != AV_PIX_FMT_NONE; ++p)
						if (*p == AV_PIX_FMT_D3D11)
							return *p;
					return pPixFmts[0];
				};
			if (r = avcodec_open2(m_pVideoCodecCtx, pCodec, nullptr))
				return r;
		}
		break;
		case AVMEDIA_TYPE_AUDIO:
		{
			if (m_pAudioStream)
				break;
			m_pAudioStream = pStream;
			pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
			// 创建解码上下文
			if (m_pAudioCodecCtx)
				avcodec_free_context(&m_pAudioCodecCtx);
			m_pAudioCodecCtx = avcodec_alloc_context3(pCodec);
			if (!m_pAudioCodecCtx)
				return AVERROR(ENOMEM);
			avcodec_parameters_to_context(m_pAudioCodecCtx, pStream->codecpar);
			// 打开解码器
			if (r = avcodec_open2(m_pAudioCodecCtx, pCodec, nullptr))
				return r;
		}
		break;
		}
	}
	AudiopInitCoreAudio();
	return 0;
}

int CPlayer::ReadFrame(_Inout_ RfData& d) noexcept
{
	if (!d.pPacket)
		d.pPacket = av_packet_alloc();
	if (!d.pFrame)
		d.pFrame = av_frame_alloc();
	int r;
	EckLoop()
	{
		r = avcodec_receive_frame(m_pVideoCodecCtx, d.pFrame);
		if (r == 0)
		{
			d.eType = AVMEDIA_TYPE_VIDEO;
			av_packet_unref(d.pPacket);
			return 0;// 解码视频成功
		}
		r = avcodec_receive_frame(m_pAudioCodecCtx, d.pFrame);
		if (r == 0)
		{
			d.eType = AVMEDIA_TYPE_AUDIO;
			av_packet_unref(d.pPacket);
			return 0;// 解码音频成功
		}
		r = av_read_frame(m_pFmtCtx, d.pPacket);
		if (r != 0)
		{
			avcodec_send_packet(m_pVideoCodecCtx, nullptr);
			avcodec_send_packet(m_pAudioCodecCtx, nullptr);
			continue;
		}

		AVCodecContext* pCurrCodecCtx;
		if (d.pPacket->stream_index == m_pVideoStream->index)
			pCurrCodecCtx = m_pVideoCodecCtx;
		else if (d.pPacket->stream_index == m_pAudioStream->index)
			pCurrCodecCtx = m_pAudioCodecCtx;
		else
		{
			av_packet_unref(d.pPacket);
			continue;
		}

		r = avcodec_send_packet(pCurrCodecCtx, d.pPacket);
		av_packet_unref(d.pPacket);

		if (r != 0)
			return r;
	}
	return AVERROR_UNKNOWN;
}

int CPlayer::Seek(INT64 pos) noexcept
{
	av_seek_frame(m_pFmtCtx, -1, pos, AVSEEK_FLAG_BACKWARD);
	avcodec_flush_buffers(m_pVideoCodecCtx);
	return 0;
}

INT64 CPlayer::GetCurrentPos() const noexcept
{
	return av_seek_frame(m_pFmtCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
}

float CPlayer::GetFrameRate() const noexcept
{
	if (m_pVideoStream->avg_frame_rate.num > 0.f)
		return (float)m_pVideoStream->avg_frame_rate.num /
		(float)m_pVideoStream->avg_frame_rate.den;
	else if (m_pVideoCodecCtx->framerate.num > 0.f)
		return (float)m_pVideoCodecCtx->framerate.num /
		(float)m_pVideoCodecCtx->framerate.den;
	else
		return 0.f;
}

void CPlayer::AudioWriteFrame(const AVFrame* pFrame) noexcept
{
	UINT32 cRequested = pFrame->nb_samples;
	auto pBuf = AudiopGetBuffer(cRequested);
	switch (pFrame->format)
	{
	case AV_SAMPLE_FMT_U8:
	{
		auto p = (BYTE*)pFrame->data[0];
		const auto pEnd = p + (cRequested * m_cAudioChannels);
		for (; p < pEnd; ++p)
			*pBuf++ = (float(*p) - 128.f) / 128.f;
	}
	break;
	case AV_SAMPLE_FMT_S16:
	{
		auto p = (short*)pFrame->data[0];
		const auto pEnd = p + (cRequested * m_cAudioChannels);
		for (; p < pEnd; ++p)
			*pBuf++ = (float(*p) / 32768.f);
	}
	break;
	case AV_SAMPLE_FMT_S32:
	{
		auto p = (int*)pFrame->data[0];
		const auto pEnd = p + (cRequested * m_cAudioChannels);
		for (; p < pEnd; ++p)
			*pBuf++ = (float(*p) / 2147483648.f);
	}
	break;
	case AV_SAMPLE_FMT_FLT:
		memcpy(pBuf, pFrame->data[0], cRequested * m_cAudioChannels * sizeof(float));
		break;
	case AV_SAMPLE_FMT_DBL:
	{
		auto p = (double*)pFrame->data[0];
		const auto pEnd = p + (cRequested * m_cAudioChannels);
		for (; p < pEnd; ++p)
			*pBuf++ = float(*p);
	}
	break;
	case AV_SAMPLE_FMT_U8P:
	{
		const auto p = (BYTE**)pFrame->data;
		for (UINT32 i = 0; i < cRequested; ++i)
			for (UINT32 j = 0; j < m_cAudioChannels; ++j)
				*pBuf++ = (float(p[j][i]) - 128.f) / 128.f;
	}
	break;
	case AV_SAMPLE_FMT_S16P:
	{
		const auto p = (short**)pFrame->data;
		for (UINT32 i = 0; i < cRequested; ++i)
			for (UINT32 j = 0; j < m_cAudioChannels; ++j)
				*pBuf++ = (float(p[j][i]) / 32768.f);
	}
	break;
	case AV_SAMPLE_FMT_S32P:
	{
		const auto p = (int**)pFrame->data;
		for (UINT32 i = 0; i < cRequested; ++i)
			for (UINT32 j = 0; j < m_cAudioChannels; ++j)
				*pBuf++ = (float(p[j][i]) / 2147483648.f);
	}
	break;
	case AV_SAMPLE_FMT_FLTP:
	{
		const auto p = (float**)pFrame->data;
		for (UINT32 i = 0; i < cRequested; ++i)
			for (UINT32 j = 0; j < m_cAudioChannels; ++j)
				*pBuf++ = p[j][i];
	}
	break;
	case AV_SAMPLE_FMT_DBLP:
	{
		const auto p = (double**)pFrame->data;
		for (UINT32 i = 0; i < cRequested; ++i)
			for (UINT32 j = 0; j < m_cAudioChannels; ++j)
				*pBuf++ = float(p[j][i]);
	}
	break;
	default:
		break;
	}
	AudiopReleaseBuffer(cRequested);
}
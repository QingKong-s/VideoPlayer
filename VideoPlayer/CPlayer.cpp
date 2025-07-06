#include "pch.h"
#include "CPlayer.h"


int CPlayer::OpenFile(PCSTR pszPathU8) noexcept
{
	int r;
	if (r = avformat_open_input(&m_pFmtCtx, pszPathU8, nullptr, nullptr))
		return r;
	if (r = avformat_find_stream_info(m_pFmtCtx, nullptr))
		return r;
	m_pVideoStream = nullptr;
	int cAudio{};
	EckCounter(m_pFmtCtx->nb_streams, i)
	{
		const auto pStream = m_pFmtCtx->streams[i];
		const AVCodec* pCodec;
		switch (pStream->codecpar->codec_type)
		{
		case AVMEDIA_TYPE_VIDEO:
		{
			m_pVideoStream = pStream;
			pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
			if (m_pVideoCodecCtx)
				avcodec_free_context(&m_pVideoCodecCtx);
			m_pVideoCodecCtx = avcodec_alloc_context3(pCodec);
			if (!m_pVideoCodecCtx)
				return AVERROR(ENOMEM);
			avcodec_parameters_to_context(m_pVideoCodecCtx, pStream->codecpar);
			if (!m_pHwDeviceCtx)
				av_hwdevice_ctx_create(&m_pHwDeviceCtx,
					AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
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
			if (cAudio == MaxAudioCount)
				break;
			const auto j = cAudio++;
			m_pAudioStream[j] = pStream;
			pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
			// 创建解码上下文
			if (m_pAudioCodecCtx[j])
				avcodec_free_context(&m_pAudioCodecCtx[j]);
			m_pAudioCodecCtx[j] = avcodec_alloc_context3(pCodec);
			if (!m_pAudioCodecCtx[j])
				return AVERROR(ENOMEM);
			avcodec_parameters_to_context(m_pAudioCodecCtx[j], pStream->codecpar);
			// 打开解码器
			if (r = avcodec_open2(m_pAudioCodecCtx[j], pCodec, nullptr))
				return r;
		}
		break;
		}
	}
	return 0;
}

int CPlayer::ReadFrame(_Inout_ RfData& d) noexcept
{
	if (!d.pPacket)
		d.pPacket = av_packet_alloc();
	if (!d.pFrame)
		d.pFrame = av_frame_alloc();

	int ret = 0;

	while (true)
	{
		// 尝试从解码器取 frame
		ret = avcodec_receive_frame(m_pVideoCodecCtx, d.pFrame);
		if (ret == 0) {
			d.eType = AVMEDIA_TYPE_VIDEO;
			av_packet_unref(d.pPacket);  // 清理 packet
			return 0;  // 解码成功
		}
		else if (ret != AVERROR(EAGAIN)) {
			av_packet_unref(d.pPacket);  // 解码器出错
			return ret;
		}

		// receive_frame 没有 frame，读 packet
		ret = av_read_frame(m_pFmtCtx, d.pPacket);
		if (ret != 0) {
			// 文件读完时，发送 NULL packet flush 解码器
			avcodec_send_packet(m_pVideoCodecCtx, nullptr);
			continue;  // 再次尝试 receive_frame
		}

		if (d.pPacket->stream_index != m_pVideoStream->index) {
			// 其他流直接忽略
			av_packet_unref(d.pPacket);
			continue;
		}

		// 送 packet 给解码器
		ret = avcodec_send_packet(m_pVideoCodecCtx, d.pPacket);
		av_packet_unref(d.pPacket);  // 送完就可释放

		if (ret != 0) {
			return ret;  // 送 packet 失败
		}

		// 继续 while，回去尝试 receive_frame
	}

	return AVERROR_UNKNOWN;  // 理论上不可能走到这里
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

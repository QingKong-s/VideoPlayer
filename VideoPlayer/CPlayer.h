#pragma once
class CPlayer final
{
public:
	constexpr static size_t MaxAudioCount = 4;

	struct RfData
	{
		AVPacket* pPacket{};
		AVMediaType eType{};// AVMEDIA_TYPE_*
		size_t idx{};		// 每个类型的子索引，只支持1个视频流，支持MaxAudioCount个音频流

		AVFrame* pFrame{};

		~RfData()
		{
			if (pPacket)
			{
				av_packet_unref(pPacket);
				av_packet_free(&pPacket);
			}
		}

		void UnRef()
		{
			av_frame_unref(pFrame);
			av_packet_unref(pPacket);
		}
	};
private:
	AVFormatContext* m_pFmtCtx{};

	AVStream* m_pVideoStream{};
	AVCodecContext* m_pVideoCodecCtx{};
	AVBufferRef* m_pHwDeviceCtx{};

	AVStream* m_pAudioStream[MaxAudioCount]{};
	AVCodecContext* m_pAudioCodecCtx[MaxAudioCount]{};

public:
	int OpenFile(PCSTR pszPathU8) noexcept;

	int ReadFrame(_Inout_ RfData& d) noexcept;

	int Seek(INT64 pos) noexcept;

	INT64 GetCurrentPos() const noexcept;

	float GetFrameRate() const noexcept;

	EckInlineNdCe auto GetFmtCtx() const noexcept { return m_pFmtCtx; }
	EckInlineNdCe auto GetVideoStream() const noexcept { return m_pVideoStream; }
	EckInlineNdCe auto GetVideoCodecCtx() const noexcept { return m_pVideoCodecCtx; }
	EckInlineNdCe auto GetHwDeviceCtx() const noexcept { return m_pHwDeviceCtx; }

	float GetPts(const RfData& d)
	{
		double framePts = 0;
		if (d.pFrame->pts != AV_NOPTS_VALUE)
			return float(d.pFrame->pts * av_q2d(m_pVideoStream->time_base));
		if (d.pPacket->pts!= AV_NOPTS_VALUE)
			return float(d.pPacket->pts * av_q2d(m_pVideoStream->time_base));
		return 0.f;
	}
};
#pragma once
class CPlayer final
{
public:
	enum class MdType
	{
		Invalid,
		Video,
		Audio,
	};

	struct RfData
	{
		AVPacket* pPacket{};
		AVMediaType eType{};// AVMEDIA_TYPE_*
		size_t idx{};		// 每个类型的子索引，只支持1个视频流，支持MaxAudioCount个音频流
		MdType eMdType{};

		AVFrame* pFrame{};

		~RfData()
		{
			if (pPacket)
			{
				av_packet_unref(pPacket);
				av_packet_free(&pPacket);
			}
			if (pFrame)
			{
				av_frame_unref(pFrame);
				av_frame_free(&pFrame);
			}
		}

		void UnRef()
		{
			av_frame_unref(pFrame);
			av_packet_unref(pPacket);
		}
	};
private:
	ComPtr<ID3D11Device> m_pD3DDevice{};
	ComPtr<ID3D11DeviceContext> m_pD3DContext{};

	AVFormatContext* m_pFmtCtx{};

	AVStream* m_pVideoStream{};
	AVCodecContext* m_pVideoCodecCtx{};
	AVBufferRef* m_pHwDeviceCtx{};

	AVStream* m_pAudioStream{};
	AVCodecContext* m_pAudioCodecCtx{};

	ComPtr<IMMDeviceEnumerator> m_pAudioDeviceEnum{};
	ComPtr<IMMDevice> m_pAudioDevice{};
	ComPtr<IAudioClient> m_pAudioClient{};
	ComPtr<IAudioRenderClient> m_pAudioRenderClient{};

	UINT32 m_cAudioBuffer{};
	constexpr static int m_cAudioChannels = 2;

	void AudiopInitCoreAudio() noexcept;

	// 返回[-1, 1]归一化交错浮点缓冲区，返回时cRequested被修改为最大可写入长度
	float* AudiopGetBuffer(_Inout_ UINT32& cRequested) noexcept;

	void AudiopReleaseBuffer(UINT32 cWritten) noexcept;
public:
	ECK_DISABLE_COPY_MOVE_DEF_CONS(CPlayer);
	~CPlayer();

	void InitD3D(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
	{
		m_pD3DDevice = pDevice;
		m_pD3DContext = pContext;
		if (m_pHwDeviceCtx)
		{
			av_buffer_unref(&m_pHwDeviceCtx);
			m_pHwDeviceCtx = nullptr;
		}
	}

	int OpenFile(PCSTR pszPathU8) noexcept;

	int ReadFrame(_Inout_ RfData& d) noexcept;

	int Seek(INT64 pos) noexcept;

	INT64 GetCurrentPos() const noexcept;

	float GetFrameRate() const noexcept;

	float GetPts(const RfData& d) const noexcept
	{
		double framePts = 0;
		if (d.pFrame->pts != AV_NOPTS_VALUE)
			return float(d.pFrame->pts * av_q2d(m_pVideoStream->time_base));
		if (d.pPacket->pts != AV_NOPTS_VALUE)
			return float(d.pPacket->pts * av_q2d(m_pVideoStream->time_base));
		return 0.f;
	}

	EckInline void AudioStart() noexcept { m_pAudioClient->Start(); }
	EckInline void AudioStop() noexcept { m_pAudioClient->Stop(); }
	void AudioWriteFrame(const AVFrame* pFrame) noexcept;

	EckInlineNdCe auto GetFmtCtx() const noexcept { return m_pFmtCtx; }
	EckInlineNdCe auto GetVideoStream() const noexcept { return m_pVideoStream; }
	EckInlineNdCe auto GetVideoCodecCtx() const noexcept { return m_pVideoCodecCtx; }
	EckInlineNdCe auto GetHwDeviceCtx() const noexcept { return m_pHwDeviceCtx; }
	EckInlineNdCe auto GetAudioStream() const noexcept { return m_pAudioStream; }
	EckInlineNdCe auto GetAudioCodecCtx() const noexcept { return m_pAudioCodecCtx; }
};
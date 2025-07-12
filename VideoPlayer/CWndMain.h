#pragma once
#include "CApp.h"
#include "CPlayer.h"
#include "CVeRoot.h"
#include "CVePlayList.h"

class CWndMain : public Dui::CDuiWnd, public eck::CFixedTimeLine
{
	friend class CVePlayList;
private:
	struct PLAY_ITEM
	{
		eck::CRefStrW rsPath;
		eck::CRefStrA rsPathU8;
		eck::CRefStrW rsName;
	};

	struct alignas(16) VERTEX
	{
		Dx::XMFLOAT4 pos;
		Dx::XMFLOAT2 uv;
	};

	constexpr static VERTEX Vertices[]
	{
		{ { -1.f, 1.f, 0.f, 1.f },{ 0.f,0.f } },
		{ {  1.f, 1.f, 0.f, 1.f },{ 1.f,0.f } },
		{ { -1.f,-1.f, 0.f, 1.f },{ 0.f,1.f } },
		{ {  1.f,-1.f, 0.f, 1.f },{ 1.f,1.f } }
	};

	ComPtr<ID3D11Device> m_pDevice{};
	ComPtr<ID3D11DeviceContext> m_pContext{};
	EzDx::CTexture m_Texture{};
	//---不要改变顺序---//
	ComPtr<ID3D11SamplerState> m_pSamplerY{};
	ComPtr<ID3D11SamplerState> m_pSamplerUV{};
	EzDx::CShaderResourceView m_SrvY{};
	EzDx::CShaderResourceView m_SrvUV{};
	//---          ---//
	EzDx::CVSAndInputLayout m_VSAndIL{};
	EzDx::CShader<EzDx::PS_T> m_PS{};
	EzDx::CBuffer m_VbVideoTex{};

	float m_fFrameRate{};
	int m_msCount{};
	D2D1_RECT_F m_rcVideo{};

	CPlayer m_Player{};
	CPlayer::RfData m_RfData{};

	std::vector<PLAY_ITEM> m_vPlayList{};
	int m_idxCurrPlaying{ -1 };

	CVeRoot m_RootElem{};
	Dui::CTrackBar m_TB{};
	CVePlayList m_ListContainer{ *this };

	Dui::CCompositor2DAffineTransform* m_pCompAnList{};
	float m_msAnList{};
	BITBOOL m_bAnList : 1{};
	BITBOOL m_bListShow : 1{};

	ComPtr<IDWriteTextFormat> m_pTextFormat{};

	LRESULT OnCreate();

	void UpdateSampler();

	void UpdateTexture(const AVFrame* pFrame);

	void CopyTexture(ID3D11Texture2D* pTexSrc, UINT idxSubRes);

	void UpdateVideoMetrics();

	int PlAdd(std::wstring_view svPath, int idxInsert = -1);

	void Play(int idx);

	void Stop();

	void AnListBegin();

	void AnListEnd();

	void RePosList();
public:
	LRESULT OnMsg(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	LRESULT OnRenderEvent(UINT uMsg, Dui::RENDER_EVENT& e) override;

	LRESULT OnElemEvent(Dui::CElem* pElem, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	void Tick(int iMs);

	BOOL IsValid() override { return 1; }
};
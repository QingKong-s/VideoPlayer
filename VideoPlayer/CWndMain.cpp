#include "pch.h"
#include "CWndMain.h"

#include "Test.h"

enum : int
{
	CyTrackSize = 12,
	CyTrackBar = 26,
};

constexpr std::string_view VS
{ R"___(
struct VSOut
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

VSOut main(float4 pos : POSITION, float2 tex : TEXCOORD0)
{
	VSOut r;
	r.pos = pos;
	r.tex = tex;
	return r;
}
)___" };

constexpr std::string_view PSBlt
{ R"___(
Texture2D<float> gTexY : register(t0);
Texture2D<float2> gTexUV : register(t1);
SamplerState gSamplerY : register(s0);
SamplerState gSamplerUV : register(s1);

static const float3x3 MatYuvToRgb =
{
	1.164383f, 1.164383f, 1.164383f,
	0.000000f, -0.391762f, 2.017232f,
	1.596027f, -0.812968f, 0.000000f
};

struct PSIn
{
	float4 pos : SV_POSITION;
	float2 uvPos : TEXCOORD0;
};

float3 YuvToRgb(float3 yuv)
{
	yuv -= float3(0.062745f, 0.501960f, 0.501960f);
	yuv = mul(yuv, MatYuvToRgb);

	return saturate(yuv);
}

float4 main(PSIn input) : SV_TARGET
{
	float y = gTexY.Sample(gSamplerY, input.uvPos);
	float2 uv = gTexUV.Sample(gSamplerUV, input.uvPos);
	return float4(YuvToRgb(float3(y, uv)), 1);
}
)___" };


LRESULT CWndMain::OnCreate()
{
	RegisterTimeLine(this);

	eck::g_pD3d11Device->QueryInterface(IID_PPV_ARGS(&m_pDevice));
	m_pDevice->GetImmediateContext(&m_pContext);

	m_Player.InitD3D(m_pDevice.Get(), m_pContext.Get());
	// 编译着色器
	constexpr D3D11_INPUT_ELEMENT_DESC Layout[]
	{
		{ "POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT },
		{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,16 }
	};
	m_VSAndIL.Create(VS.data(), (int)VS.size(), "main", Layout, ARRAYSIZE(Layout));
	m_PS.Create(PSBlt.data(), (int)PSBlt.size(), "main");
	// 顶点缓冲
	m_VbVideoTex.Create(sizeof(Vertices), D3D11_BIND_VERTEX_BUFFER,
		D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

	UpdateSampler();

	eck::GetThreadCtx()->UpdateDefColor();

	for (auto e : Test)
		PlAdd(e);
	Play(8);

	m_RootElem.Create(nullptr, Dui::DES_VISIBLE, 0,
		0, 0, 0, 0, nullptr, this);
	const auto pRoot = &m_RootElem;
	m_TB.Create(nullptr, Dui::DES_VISIBLE | Dui::DES_NOTIFY_TO_WND, 0,
		0, 0, 0, 0, pRoot);
	m_TB.SetTrackSize(CyTrackSize);
	return 0;
}

void CWndMain::UpdateSampler()
{
	D3D11_SAMPLER_DESC Desc{};
	Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	Desc.AddressU = Desc.AddressV = Desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	Desc.MaxAnisotropy = 1;
	Desc.MaxLOD = FLT_MAX;
	Desc.MinLOD = -FLT_MAX;
	Desc.BorderColor[0] = 0.0627f;
	m_pDevice->CreateSamplerState(&Desc, &m_pSamplerY);
	Desc.BorderColor[0] = Desc.BorderColor[1] = 0.5020f;
	m_pDevice->CreateSamplerState(&Desc, &m_pSamplerUV);
}

void CWndMain::UpdateTexture(const AVFrame* pFrame)
{
	D3D11_TEXTURE2D_DESC Desc{};
	((ID3D11Texture2D*)pFrame->data[0])->GetDesc(&Desc);

	m_Texture.Create(Desc.Width, Desc.Height, Desc.Format,
		D3D11_BIND_SHADER_RESOURCE);
	constexpr DXGI_FORMAT YFormat[]
	{
		DXGI_FORMAT_R8_UNORM,DXGI_FORMAT_R16_UNORM,DXGI_FORMAT_R16_UNORM
	};
	constexpr DXGI_FORMAT UVFormat[]
	{
		DXGI_FORMAT_R8G8_UNORM,DXGI_FORMAT_R16G16_UNORM,DXGI_FORMAT_R16_UNORM
	};
	size_t idxFmt;
	switch (m_Player.GetVideoCodecCtx()->sw_pix_fmt)
	{
	case AV_PIX_FMT_YUV420P:		idxFmt = 0; break;
	case AV_PIX_FMT_YUV420P10LE:	idxFmt = 1; break;
	case AV_PIX_FMT_YUV444P10LE:	idxFmt = 2; break;
	default: return;
	}
	m_SrvY.Create(m_Texture.Get(), YFormat[idxFmt]);
	m_SrvUV.Create(m_Texture.Get(), UVFormat[idxFmt]);
}

void CWndMain::CopyTexture(ID3D11Texture2D* pTexSrc, UINT idxSubRes)
{
	ComPtr<ID3D11Device> pDevice;
	pTexSrc->GetDevice(&pDevice);
	ComPtr<ID3D11DeviceContext> pContext;
	pDevice->GetImmediateContext(&pContext);
	pContext->CopySubresourceRegion(m_Texture.Get(), 0, 0, 0, 0,
		pTexSrc, idxSubRes, nullptr);
	pContext->Flush();
}

void CWndMain::UpdateVideoMetrics()
{
	const auto cxVideo = m_Player.GetVideoCodecCtx()->width;
	const auto cyVideo = m_Player.GetVideoCodecCtx()->height;

	m_rcVideo = { 0.f,0.f,(float)cxVideo,(float)cyVideo };
	const D2D1_RECT_F rcClient{ 0.f,0.f,(float)GetClientWidth(),(float)GetClientHeight() };
	eck::AdjustRectToFitAnother(m_rcVideo, rcClient);
}

int CWndMain::PlAdd(std::wstring_view svPath, int idxInsert)
{
	auto& e = (idxInsert < 0 ? m_vPlayList.emplace_back(svPath) :
		*m_vPlayList.emplace(m_vPlayList.begin() + idxInsert, svPath));
	e.rsPathU8 = eck::StrW2U8(e.rsPath);
	const auto posFileSpec = e.rsPath.PazFindFileSpec();
	if (posFileSpec >= 0)
	{
		e.rsName.DupString(e.rsPath.Data() + posFileSpec,
			e.rsPath.Size() - posFileSpec);
		e.rsName.PazRemoveExtension();
	}
	else
		e.rsName = e.rsPath;
	return (idxInsert < 0 ? (int)m_vPlayList.size() - 1 : idxInsert);
}

void CWndMain::Play(int idx)
{
	Stop();
	m_Player.OpenFile(m_vPlayList[idx].rsPathU8.Data());
	m_Player.AudioStart();
	//m_Player.Seek(100 * AV_TIME_BASE);
	//m_Player.Seek(0);

	const auto* const pfc = m_Player.GetFmtCtx();
	m_fFrameRate = m_Player.GetFrameRate();

	m_TB.SetRange(0.f, pfc->duration / (float)AV_TIME_BASE);
	m_TB.SetTrackPos(0.f);
	m_msCount = 0;
}

void CWndMain::Stop()
{
}

LRESULT CWndMain::OnMsg(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SIZE:
	{
		const auto lResult = __super::OnMsg(hWnd, uMsg, wParam, lParam);
		UpdateVideoMetrics();
		const auto cx = GetClientWidthLog();
		const auto cy = GetClientHeightLog();
		m_RootElem.SetSize(cx, cy);
		m_TB.SetRect({ 0,cy - CyTrackBar,cx,cy });
		return lResult;
	}
	break;

	case WM_CREATE:
	{
		const auto lResult = __super::OnMsg(hWnd, uMsg, wParam, lParam);
		StUpdateColorizationColor();
		OnCreate();
		return lResult;
	}
	break;

	case WM_DESTROY:
	{
		__super::OnMsg(hWnd, uMsg, wParam, lParam);
		PostQuitMessage(0);
	}
	return 0;

	case WM_SYSCOLORCHANGE:
		eck::MsgOnSysColorChangeMainWnd(hWnd, wParam, lParam);
		break;
	case WM_SETTINGCHANGE:
		eck::MsgOnSettingChangeMainWnd(hWnd, wParam, lParam);
		break;
	}
	return __super::OnMsg(hWnd, uMsg, wParam, lParam);
}

LRESULT CWndMain::OnRenderEvent(UINT uMsg, Dui::RENDER_EVENT& e)
{
	switch (uMsg)
	{
	case Dui::RE_PRERENDER:
	{
		RECT rcClient;
		const RECT* prcDirty;
		if (e.PreRender.prcDirtyPhy)
			prcDirty = e.PreRender.prcDirtyPhy;
		else
		{
			rcClient = { 0,0,GetClientWidth(),GetClientHeight() };
			prcDirty = &rcClient;
		}

		const auto& f = e.PreRender;
		D3D11_VIEWPORT Viewport;
		Viewport.TopLeftX = (float)f.ptOffsetPhy.x;
		Viewport.TopLeftY = (float)f.ptOffsetPhy.y;
		Viewport.Width = float(prcDirty->right - prcDirty->left);
		Viewport.Height = float(prcDirty->bottom - prcDirty->top);
		Viewport.MinDepth = 0.f;
		Viewport.MaxDepth = 1.f;

		ComPtr<ID3D11Texture2D> pDstTex;
		f.pSfcFinalDst->QueryInterface(&pDstTex);
		EzDx::CRenderTargetView Rtv{};
		Rtv.Create(pDstTex.Get());

		m_pContext->OMSetRenderTargets(1, &Rtv.pRtv, nullptr);
		m_pContext->RSSetViewports(1, &Viewport);

		m_pContext->IASetInputLayout(m_VSAndIL.GetInputLayout());
		m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		D3D11_MAPPED_SUBRESOURCE MappedRes{};
		m_pContext->Map(m_VbVideoTex.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedRes);
		const auto pVertices = (VERTEX*)MappedRes.pData;
		memcpy(pVertices, Vertices, sizeof(Vertices));
		{
			const auto lD = prcDirty->left;
			const auto tD = prcDirty->top;
			const auto rD = prcDirty->right;
			const auto bD = prcDirty->bottom;
			const auto xV = m_rcVideo.left;
			const auto yV = m_rcVideo.top;
			const auto wV = m_rcVideo.right - m_rcVideo.left;
			const auto hV = m_rcVideo.bottom - m_rcVideo.top;
			pVertices[0].uv = { (lD - xV) / wV,(tD - yV) / hV };// 左上
			pVertices[1].uv = { (rD - xV) / wV,(tD - yV) / hV };// 右上
			pVertices[2].uv = { (lD - xV) / wV,(bD - yV) / hV };// 左下
			pVertices[3].uv = { (rD - xV) / wV,(bD - yV) / hV };// 右下
		}
		m_pContext->Unmap(m_VbVideoTex.Get(), 0);

		constexpr UINT Stride = sizeof(VERTEX);
		constexpr UINT Offset = 0;
		m_pContext->IASetVertexBuffers(0, 1, &m_VbVideoTex.pBuffer, &Stride, &Offset);

		m_pContext->VSSetShader(m_VSAndIL.GetShader(), nullptr, 0);

		m_pContext->PSSetShader(m_PS.Get(), nullptr, 0);
		m_pContext->PSSetSamplers(0, 2, &m_pSamplerY);
		ID3D11ShaderResourceView* const pSrv[]{ m_SrvY.Get(), m_SrvUV.Get() };
		m_pContext->PSSetShaderResources(0, ARRAYSIZE(pSrv), pSrv);

		m_pContext->Draw(4, 0);
	}
	return Dui::RER_NO_ERASE;
	}
	return 0;
}

LRESULT CWndMain::OnElemEvent(Dui::CElem* pElem, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (pElem == &m_TB)
		switch (uMsg)
		{
		case Dui::TBE_POSCHANGED:
		{
			ECK_DUILOCKWND;
			m_Player.Seek(INT64(m_TB.GetTrackPos() * AV_TIME_BASE));
		}
		break;
		}
	return 0;
}

void CWndMain::Tick(int iMs)
{
	if (!iMs)
		return;
	const float fFrameInterval = 1000.f / m_fFrameRate; // 每帧间隔 ms
	m_msCount += iMs;

	while (m_msCount >= fFrameInterval)
	{
		const int r = m_Player.ReadFrame(m_RfData);
#ifdef _DEBUG
		char szErrStr[128];
		if (r < 0)
		{
			av_strerror(r, szErrStr, sizeof(szErrStr));
			OutputDebugStringA(szErrStr);
			OutputDebugStringW(L"\n");
			DebugBreak();
		}
#endif// _DEBUG
		m_TB.SetTrackPos(m_Player.GetPts(m_RfData));
		switch (m_RfData.eType)
		{
		case AVMEDIA_TYPE_VIDEO:
		{
			m_msCount -= (int)fFrameInterval;
			const auto pTex = (ID3D11Texture2D*)m_RfData.pFrame->data[0];
			if (!m_Texture.Get())
			{
				UpdateTexture(m_RfData.pFrame);
				UpdateSampler();
			}
			CopyTexture(pTex, PtrToUint(m_RfData.pFrame->data[1]));
			Redraw();
		}
		break;
		case AVMEDIA_TYPE_AUDIO:
			m_Player.AudioWriteFrame(m_RfData.pFrame);
			break;
		}
		m_RfData.UnRef();
	}
}
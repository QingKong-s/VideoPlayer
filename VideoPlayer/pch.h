#pragma once
#include "eck\PchInc.h"
#include "eck\SystemHelper.h"
#include "eck\CCommDlg.h"
#include "eck\EzDx.h"
#include "eck\MathHelper.h"
#include "eck\CDWriteFontFactory.h"
#include "eck\CDuiTrackBar.h"
#include "eck\CDuiTabList.h"
#include "eck\CDuiButton.h"
#include "eck\DuiStdCompositor.h"

EXTERN_C_START
#pragma warning(push)
#pragma warning(disable: 4819)// 该文件包含不能在当前代码页(936)中表示的字符
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext_d3d11va.h>
#pragma warning(pop)
EXTERN_C_END

#include <mmdeviceapi.h>
#include <Audioclient.h>

const inline IID IID_IAudioClient = __uuidof(IAudioClient);
const inline IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
const inline CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);

using eck::PCVOID;
using eck::PCBYTE;
using eck::SafeRelease;
using eck::ComPtr;

namespace Dui = eck::Dui;
namespace EzDx = eck::EzDx;
namespace Dx = DirectX;
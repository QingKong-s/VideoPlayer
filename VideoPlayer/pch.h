#pragma once
#include "eck\PchInc.h"
#include "eck\SystemHelper.h"
#include "eck\CCommDlg.h"
#include "eck\CDuiTrackBar.h"
#include "eck\EzDx.h"
#include "eck\MathHelper.h"

#include "d3d11_1.h"

EXTERN_C_START
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
EXTERN_C_END

using eck::PCVOID;
using eck::PCBYTE;
using eck::SafeRelease;
using eck::ComPtr;

namespace Dui = eck::Dui;
namespace EzDx = eck::EzDx;
namespace Dx = DirectX;
#include "frameconverter.h"
#include <QDebug>

#include <libswscale/swscale.h>   // 必须包含

FrameConverter::FrameConverter() {}

FrameConverter::~FrameConverter()
{
    close();
}

bool FrameConverter::open(int srcWidth, int srcHeight, int srcFormat,
                          int dstWidth, int dstHeight, int dstFormat)
{
    if (m_opened) close();

    m_swsCtx = sws_getContext(srcWidth, srcHeight, (AVPixelFormat)srcFormat,  //启动转换器，生成上下文
                              dstWidth, dstHeight, (AVPixelFormat)dstFormat,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsCtx) {
        qWarning() << "Failed to create SwsContext";
        return false;
    }
    m_dstWidth = dstWidth;
    m_dstHeight = dstHeight;
    m_dstFormat = dstFormat;
    m_opened = true;
    return true;
}

void FrameConverter::close()
{
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    m_opened = false;
}

bool FrameConverter::isOpened() const
{
    return m_opened;
}

AVFrame* FrameConverter::convert(const AVFrame *srcFrame)
{
    if (!m_opened || !srcFrame) return nullptr;

    AVFrame *dstFrame = av_frame_alloc();  //新建一个帧，然后输入参数
    dstFrame->format = m_dstFormat;
    dstFrame->width = m_dstWidth;
    dstFrame->height = m_dstHeight;
    av_frame_get_buffer(dstFrame, 0);


    // // ---- 关键修正：设置色彩范围 ----
    // // 源帧（桌面采集）通常为全范围（JPEG / PC 范围）
    // // 但 const 限定不能直接修改，用临时变量或强制转换（每帧用完即弃，无副作用）
    // const_cast<AVFrame*>(srcFrame)->color_range = AVCOL_RANGE_JPEG;
    // // 目标帧（给编码器）需要限制范围（MPEG / TV 范围）
    // dstFrame->color_range = AVCOL_RANGE_MPEG;


    sws_scale(m_swsCtx,
              srcFrame->data, srcFrame->linesize, 0, srcFrame->height,
              dstFrame->data, dstFrame->linesize);



    return dstFrame;
}

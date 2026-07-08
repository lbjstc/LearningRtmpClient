#ifndef FLVRTMPOUTPUT_H
#define FLVRTMPOUTPUT_H

#include "irtmpoutput.h"
#include <QMutex>

extern "C" {
#include <libavformat/avformat.h>
}

//为什么这个类也存在多线程的场景使用？这个不就是推流吗？
//因为UI线程可能调用isOpened 或者close;;编码/网络线程可能调用writePacket;;控制线程调用writeHeader/writeTrailer
//AVFormatContext也不是线程安全的，多线程同时写包会关闭导致崩溃


class FlvRtmpOutput : public IRtmpOutput
{
public:
    FlvRtmpOutput();
    ~FlvRtmpOutput() override;

    bool open(const QString &url,
              const AVCodecParameters *videoParams,
              AVRational encTimeBase) override;
    void close() override;
    bool writePacket(const EncodedPacket &pkt) override;
    bool writeHeader() override;
    bool writeTrailer() override;
    bool isOpened() const override;

private:
    AVFormatContext *m_outCtx = nullptr;   //最核心的推流的上下文，信息都在这里面
    AVStream *m_videoStream = nullptr;       //推流需要一个流信息
    AVRational m_encTimeBase;  // 视频流时间基；//看起来时间基很重要，要单独存储？
    bool m_headerWritten = false;  //标识符，用来给发送rtmp的头和尾坐标记得 ，我有点想不起来了flv发送的时候，想需要先发送一个头metadata和关键帧，然后结尾的时候也需要发送一个结束标识符对吗？
    mutable QMutex m_mutex;
};

#endif // FLVRTMPOUTPUT_H

#ifndef H264ENCODER_H
#define H264ENCODER_H

#include "ivideoencoder.h"
#include <QMutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
//你贴出的编码器封装代码，核心抽象在于将 FFmpeg 的异步编码模型包装为同步的 send/receive 接口，并通过互斥锁支持多线程并发调用。

//怎么理解多线程的应用场景？上一个屏幕采集就默认只有一个线程调用。但是这里的encoder是需要锁的，设计的时候认为会有多线程调用？甚至是open都会有多线程调用，如何理解这个设计思路？

// 采集器在目前设计里是单一使用场景：由一个线程打开，同一个线程运行 start() 循环，停止后才会关闭。start() 本身是阻塞的，所以逻辑上它始终在单个线程中执行，无需加锁。

//编码器则不同：它是可能被多个线程共享的模块。例如在推流器中，采集线程拿到一帧后调用 sendFrame()，推送线程会定时调用 receivePackets() 取走编码好的包，同时主线程可能通
//过 codecParameters() 获取参数。这些操作可能并发进行，因此编码器必须保证线程安全。



class H264Encoder : public IVideoEncoder
{
public:
    H264Encoder();
    ~H264Encoder() override;

    bool open(const QSize &resolution, int inputPixelFormat,   //初始化解码器
              int fps, int bitrate = 0) override;
    void close() override;
    bool sendFrame(AVFrame *frame) override;   //就是把frame发送给解码器呗
    bool receivePackets(EncodedPacketCallback cb) override;
    int64_t timeBaseNum() const override;
    int64_t timeBaseDen() const override;
    AVRational timeBase() const  override;
    const AVCodecParameters* codecParameters() const override;

private:
    AVCodecContext *m_codecCtx = nullptr;  //最核心的上下文，保存全部信息。这个是动态的
    AVRational m_timeBase;   //为什么时间基需要单独保存；
    AVCodecParameters *m_codecPar = nullptr; // 复制保存编码器的参数？为了给外部传出去，作为关键信息，本质上也是传静态参数
    mutable QMutex m_mutex;   //这里需要线程安全了？
};

#endif // H264ENCODER_H

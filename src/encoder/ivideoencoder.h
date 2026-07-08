// ivideoencoder.h
#ifndef IVIDEOENCODER_H
#define IVIDEOENCODER_H

#include <QSize>
#include <functional>
#include <cstdint>
extern "C"
{
#include <libavutil/frame.h>
#include <libavcodec/codec_par.h>
};


//提供一个编码器的接口，open，close是最基础的设置编码器。然后发送编码器的包。提供暴露编码器参数的接口。

//设计意图：这是一个典型的薄封装，将 FFmpeg 的数据结构转换为项目内自定义的类型，便于后续替换底层库或在不同模块间传递。
struct EncodedPacket {   //自定义编码包，为什么这么设计？目的是隐藏ffmpeg的细节，降低耦合。
    uint8_t *data;
    int size;
    int64_t pts;
    int64_t dts;
};

using EncodedPacketCallback = std::function<void(const EncodedPacket &pkt)>;   //要同步回调传递自定义packet

class IVideoEncoder
{
public:
    virtual ~IVideoEncoder() = default;

    // 打开编码器，指定输入尺寸、像素格式、帧率，可选码率 (0 为默认)
    virtual bool open(const QSize &resolution, int inputPixelFormat,
                      int fps, int bitrate = 0) = 0;
    virtual void close() = 0;

    // 发送一帧原始数据 (YUV420P 或与 open 指定的格式匹配)
    virtual bool sendFrame(AVFrame *frame) = 0;
    // 取出所有可用的编码包，通过回调传递
    virtual bool receivePackets(EncodedPacketCallback cb) = 0;

    // 获取编码器参数，供输出流初始化
    virtual int64_t timeBaseNum() const = 0;
    virtual int64_t timeBaseDen() const = 0;
    virtual AVRational timeBase() const  = 0;
    virtual const AVCodecParameters* codecParameters() const = 0;
};

#endif // IVIDEOENCODER_H

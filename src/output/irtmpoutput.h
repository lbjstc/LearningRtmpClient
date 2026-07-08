#ifndef IRTMPOUTPUT_H
#define IRTMPOUTPUT_H

#include <QString>

struct EncodedPacket; // 前向声明，与编码器中的定义一致


//推流器，同样先是延迟初始化，open和close。核心就是把包写入到推流的format中

class IRtmpOutput
{
public:
    virtual ~IRtmpOutput() = default;

    // 打开输出 URL，并传入视频编码参数和时间基//为什么需要这么多参数？
    virtual bool open(const QString &url,
              const struct AVCodecParameters *videoParams,
                      struct AVRational encTimeBase) = 0;
    virtual void close() = 0;

    // 写入编码后的包
    virtual bool writePacket(const EncodedPacket &pkt) = 0;

    // 写文件头/尾（内部通常自动调用，也可手动）
    virtual bool writeHeader() = 0;
    virtual bool writeTrailer() = 0;

    // 是否已打开
    virtual bool isOpened() const = 0;
};

#endif // IRTMPOUTPUT_H

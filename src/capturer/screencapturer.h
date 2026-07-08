#ifndef SCREENCAPTURER_H
#define SCREENCAPTURER_H

#include "ivideocapturer.h"
#include <QSize>
#include <atomic>
#include <functional>

//这个类在做什么？在做屏幕的采集。继承纯虚类，定义接口
//用ffmpeg的gdi采集的时候，需要使用的成员变量，一个是采集到的属性，index，format，尺寸
//AVFormatContext是ffmpeg里最重要的数据载体
//线程安全的控制start和stop需要一个原子变量标识符。但是这里也有问题，其他的比如open，close和获取参数就是线程安全的了吗？没有做现线程同步。

//问题三：为什么 rawFrame 在循环内创建和销毁，不放在外面？没有说服我
//你的意思是，每次循环传出去的都是一个新的rawFrame，这是一个异步的过程，会传出去很多个rawFrame？但是我传递的时候是局部变量AVFrame *rawFrame，下一次循环直接销毁了，所以按理说这应该是一个同步的过程
//外面通过回调直接取用了frame的数据，然后下一次循环重新赋值，没啥问题啊？而且这里传递应该用shared_ptr吧？保证frame*是存活的？
//也可以这样优化，但是一定要保证没有残留，否则会有不利影响
//需要很注意的是，frame只是借用了packet里面的数据，数据还是属于packet的。
//在这个场景下，回调者不能存储frame


extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#include <QObject>

class ScreenCapturer : public IVideoCapturer,public QObject
{
public:
    explicit ScreenCapturer(QObject *parent = nullptr);
    ~ScreenCapturer() override;

    bool open(const QString &source) override;
    void close() override;
    QSize resolution() const override;
    int pixelFormat() const override;
    bool isOpened() const override;
    bool start(CapturedFrameCallback cb) override;
    void stop() override;
private:
    static int interruptCallback(void *opaque);
private:
    AVFormatContext *m_inCtx = nullptr;
    AVCodecContext *m_decCtx = nullptr;
    int m_videoStreamIndex = -1;
    QSize m_resolution;
    int m_pixelFormat = -1;
    std::atomic<bool> m_running{false};   //为什么这个需要用原子变量？
};

#endif // SCREENCAPTURER_H

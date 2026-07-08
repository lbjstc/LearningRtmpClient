// ivideocapturer.h
#ifndef IVIDEOCAPTURER_H
#define IVIDEOCAPTURER_H

#include <QSize>
#include <functional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

//定义一个抽象类，把视频采集的接口都定义好。open打开信息源，对应close关闭。然后提取各类采集信息。最后运行，开启运行循环start，对应stop关闭

// 1. 为什么 start 的回调不作为成员变量存储？
//     virtual bool start(CapturedFrameCallback cb) = 0;
//     这是一个同步阻塞式采集的设计：start() 一旦调用就进入内部循环，直到采集结束才返回。回调函数仅在这个循环的生命周期内有效，不需要在类中持久保存。
//     如果采集是异步的（比如启动一个线程，start() 立即返回，后续通过回调通知），那么才需要把回调存为成员变量，因为调用结束不代表采集结束。
//     你当前的实现是同步的，所以参数传递就足够了。
//     重新理解这个同步异步的问题，作为参数就是立刻执行这个函数了，所以里面的cb就会立刻执行。但是如果注册为成员变量，那就什么时候调用函数就不一定了，直到调用对应的参数，回调才会执行，这就是一种异步。

//问题一：同步的话没必要用回调函数采集把，直接返回具体的值不就行了？返回AVFrame*作为函数参数。同时这里还有个问题，AVFrame为什么不用shared_ptr进行管理？
//因为这是在循环里进行的，每读取到一帧都推送出来，如果每次都返回，那就无法在循环里进行了，循环就要在外面，由调用者拉取。


//问题二：目前的设计中，其实只有stop是线程安全的，其他的函数都是默认在一个线程中串行使用对吧？
//对的

// 回调: 每采集到一帧原始数据 (未压缩的 AVFrame, 需调用者释放)
using CapturedFrameCallback = std::function<void(AVFrame *frame)>;

class IVideoCapturer
{
public:
    virtual ~IVideoCapturer() {}
    virtual bool open(const QString &source) = 0;  // "desktop" 或窗口名
    virtual void close() = 0;
    virtual QSize resolution() const = 0;            // 采集分辨率
    virtual int pixelFormat() const = 0;             // AV_PIX_FMT_xxx
    virtual bool isOpened() const = 0;

    // 开始采集，回调在内部循环中调用，返回 false 表示结束或出错
    virtual bool start(CapturedFrameCallback cb) = 0;   //为什么这里回调函数不作为成员变量去使用了呢？
    // 停止采集（异步安全）
    virtual void stop() = 0;
};

#endif // IVIDEOCAPTURER_H

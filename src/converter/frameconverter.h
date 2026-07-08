#ifndef FRAMECONVERTER_H
#define FRAMECONVERTER_H

#include <QSize>

extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

//这个类为什么又不用关心多线程了？意思是转换是不需要多线程场景的？
//基于此我有一个大的问题，就是在设计的时候，如何考虑多线程是否使用的问题？就是单纯的脑子里思考，是否有多线程利用的场景吗？

//这里为什么不用纯虚函数？因为转换只有一种方式对吗？

class FrameConverter
{
public:
    FrameConverter();
    ~FrameConverter();
    //同样延迟启动
    bool open(int srcWidth, int srcHeight, int srcFormat,   //主要转换的参数就是宽高和格式
              int dstWidth, int dstHeight, int dstFormat);
    void close();   //
    bool isOpened() const;  //用标识符控制启动暂停

    // 转换一帧，返回新分配的 AVFrame（YUV420P），调用者需 av_frame_free 释放
    AVFrame* convert(const AVFrame *srcFrame);  //输入一个转换前的帧，为什么要用返回值的形式，直接用新增一个形参也是一样的吧？

private:
    SwsContext *m_swsCtx = nullptr;  //转换的核心上下文
    int m_dstWidth = 0;   //相应的参数，宽、高、格式。但是这里存的是输出的还是输入的？为什么要存输出的？纯粹就是为了类内跨函数使用。其实这么理解又是一个封装的大问题，成员变量的作用就是类内跨函数使用，相当于一个class内部的全局变量
    int m_dstHeight = 0;
    int m_dstFormat = -1;
    bool m_opened = false;  //其实这东西本质上也是状态机，对吧？就是两种状态而已。
};

#endif // FRAMECONVERTER_H

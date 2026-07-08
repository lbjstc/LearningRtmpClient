#include "screencapturer.h"
#include <QDebug>
#include <QMutexLocker>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

ScreenCapturer::ScreenCapturer(QObject *parent)
    : QObject(parent) // 如果不需要信号，可不继承 QObject，但为了一致性保留
{
    // 注册设备（在整个程序生命周期只需一次，但放在这里保证调用）
    avdevice_register_all();
}

ScreenCapturer::~ScreenCapturer()
{
    close();
}

bool ScreenCapturer::open(const QString &source)
{
    close(); // 关闭已存在的

    const AVInputFormat *ifmt = av_find_input_format("gdigrab");  //正常流程，新建inputformat作为信息源的格式
    if (!ifmt) {
        qWarning() << "gdigrab input format not found";
        return false;
    }

    AVDictionary *options = nullptr;
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "draw_mouse", "1", 0); // 显示鼠标
    av_dict_set(&options, "pixel_format", "bgr0", 0);   // 去掉 Alpha 通道


    // 还可以传入区域，如 source = "desktop" 或 "title=Calculator"
    m_inCtx = avformat_alloc_context();   //最核心的数据类型AVFormatContext，储存拉流的信息
    if (avformat_open_input(&m_inCtx, source.toUtf8().constData(), ifmt, &options) != 0) {   //根据格式、设置，做拉流处理，拉流源头是source，然后拉取信息到AVFormatContext
        qWarning() << "Cannot open screen capture source:" << source;
        av_dict_free(&options);  //如果出错记得释放
        avformat_free_context(m_inCtx);
        m_inCtx = nullptr;
        return false;
    }


    av_dict_free(&options);  //用过了就是释放了

    if (avformat_find_stream_info(m_inCtx, nullptr) < 0) {   //找到对应的流是否存在，从AVFormatContext里面找
        qWarning() << "Cannot find stream information";
        close();
        return false;
    }

    // 查找视频流
    m_videoStreamIndex = -1;
    for (unsigned i = 0; i < m_inCtx->nb_streams; i++) {   //还是从AVFormatContext里面找到视频流
        if (m_inCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            break;
        }
    }
    if (m_videoStreamIndex < 0) {   //每一步都要做错误处理
        qWarning() << "No video stream found";
        close();
        return false;
    }

    AVCodecParameters *par = m_inCtx->streams[m_videoStreamIndex]->codecpar;    //从对应的流里面找到AVCodecParameters，这个是视频的参数集合
    m_resolution = QSize(par->width, par->height);   //定义采集到的视频的参数
    m_pixelFormat = par->format;

    qDebug() << "Screen capture opened:" << m_resolution
             << "pixel format:" << av_get_pix_fmt_name((AVPixelFormat)m_pixelFormat);

    // 创建原始视频解码器
    const AVCodec *decoder = avcodec_find_decoder(par->codec_id);
    if (!decoder) {
        qWarning() << "rawvideo decoder not found";
        close();
        return false;
    }
    m_decCtx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(m_decCtx, par);
    if (avcodec_open2(m_decCtx, decoder, nullptr) < 0) {
        qWarning() << "Cannot open rawvideo decoder";
        close();
        return false;
    }

    return true;
}

void ScreenCapturer::close()
{
    if (m_inCtx) {   //为什么关闭的里面，只有它需要释放？可能是因为别的成员变量都是参数，没有上下文。所以其实AVFormatContext是采集这个场景里面最关键的。
        m_inCtx->interrupt_callback = {nullptr, nullptr};  // 清空回调
        avformat_close_input(&m_inCtx);
        m_inCtx = nullptr;
    }
    if (m_decCtx) {
        avcodec_free_context(&m_decCtx);
        m_decCtx = nullptr;
    }
    m_videoStreamIndex = -1;
}

QSize ScreenCapturer::resolution() const  //这往下的三个函数都是线程安全的吗？
{
    return m_resolution;
}

int ScreenCapturer::pixelFormat() const
{
    return m_pixelFormat;
}

bool ScreenCapturer::isOpened() const
{
    return m_inCtx != nullptr;
}

bool ScreenCapturer::start(CapturedFrameCallback cb)  //return true作为启动函数，需要吸收这个设计思路
{
    if (!m_inCtx) {   //先做安全检查
        qWarning() << "Capturer not opened";
        return false;
    }

    m_running = true;

    // ********** 在这里设置中断回调 **********
    m_inCtx->interrupt_callback.callback = interruptCallback;
    m_inCtx->interrupt_callback.opaque = this;

    AVPacket *packet = av_packet_alloc();  //循环外声明，避免重复创建

    while (m_running) {
        int ret = av_read_frame(m_inCtx, packet);   //采集的核心，从AVFormatContext里面读取packet
        if (ret < 0) {  //如果读取出问题
            if (ret == AVERROR_EOF || !m_running)  //什么意思，这应该是读完的标记
                break;
            char errbuf[128];   //如果没有读完，要从ffmpeg读取错误信息
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "av_read_frame error:" << errbuf;
            break;
        }

        // 只处理视频流
        if (packet->stream_index != m_videoStreamIndex) {  //根据前面对视频流的标记读取视频流。但是就一定要把所有packet从context里面读出来吗？不能仅从视频流读取？
            av_packet_unref(packet);
            continue;           //跳过
        }

        // //测试
        // // 放这里
        // AVCodecParameters *par = m_inCtx->streams[m_videoStreamIndex]->codecpar;
        // qDebug() << "codec:" << avcodec_get_name(par->codec_id)
        //          << "format:" << av_get_pix_fmt_name((AVPixelFormat)par->format)
        //          << "packet size:" << packet->size
        //          << "expected:" << m_resolution.width() * m_resolution.height() * 4;

        // // 构造一个 AVFrame 包装原始数据（不拷贝，直接引用 packet 数据）
        // AVFrame *rawFrame = av_frame_alloc();   //分配Frame，为什么不在循环外做这个事？每个包都需要新建发送销毁？因为这里做的是一个浅拷贝，
        // rawFrame->format = m_pixelFormat;
        // rawFrame->width = m_resolution.width();
        // rawFrame->height = m_resolution.height();

        // // 填充 data 指针和 linesize（对于 packed 格式如 BGRA）
        // av_image_fill_arrays(rawFrame->data, rawFrame->linesize,   //linesize到底是什么意思？data应该是具体的帧数据吧。是每一行占用的字节数（步长），可能大于等于图像宽度乘以每像素字节数，因为内存对齐的缘故。
        //                      packet->data, (AVPixelFormat)m_pixelFormat,
        //                      m_resolution.width(), m_resolution.height(), 1);


        // // 回调通知用户（用户负责释放 frame）
        // cb(rawFrame);   //发送

        // av_frame_free(&rawFrame);  //直接销毁frame，打算需要先释放，所以在回调函数里面别忘了释放unref，但是为什么释放不在自己这里做呢？这个命令相当于已经做了。


        // 发送原始 packet 到解码器
        if (avcodec_send_packet(m_decCtx, packet) == 0) {
            AVFrame *frame = av_frame_alloc();
            if (avcodec_receive_frame(m_decCtx, frame) == 0) {
                // frame 的 data 和 linesize 已正确设置，无需自己填充
                cb(frame);  // 回调传递，由外部释放 frame
                av_frame_free(&frame);
            }
        }

        av_packet_unref(packet);   //释放packet里面的内容的意思吧
    }

    av_packet_free(&packet);
    m_running = false;
    return true;
}

void ScreenCapturer::stop()
{
    m_running = false;
}


int ScreenCapturer::interruptCallback(void *opaque)
{
    auto *self = static_cast<ScreenCapturer*>(opaque);
    return !self->m_running.load();
}

#include "streampusher.h"
#include <QDebug>
#include <QThread>
#include <memory>

StreamPusher::StreamPusher(QObject *parent)
    : QObject(parent) {}

StreamPusher::~StreamPusher()
{
    stop();
}

void StreamPusher::setCapturer(IVideoCapturer *capturer) { m_capturer = capturer; }
void StreamPusher::setEncoder(IVideoEncoder *encoder) { m_encoder = encoder; }
void StreamPusher::setOutput(IRtmpOutput *output) { m_output = output; }
void StreamPusher::setConverter(FrameConverter *converter) { m_converter = converter; }
void StreamPusher::setUrl(const QString &url) { m_url = url; }
void StreamPusher::setCaptureSource(const QString &source) { m_captureSource = source; }

void StreamPusher::process()
{
    if (!m_capturer || !m_encoder || !m_output) {   //为什么convert这么特殊？不用检查？
        emit error("Missing capturer, encoder or output");
        return;
    }

    // 1. 打开采集器
    if (!m_capturer->open(m_captureSource)) {
        emit error("Failed to open capturer");
        return;
    }

    //采集器获得的参数，用来初始化其他的组件
    QSize resolution = m_capturer->resolution();
    int srcFormat = m_capturer->pixelFormat();

    // 2. 初始化编码器（目标格式 YUV420P，30fps）
    if (!m_encoder->open(resolution, AV_PIX_FMT_YUV420P, 30)) {
        emit error("Failed to open encoder");
        m_capturer->close();
        return;
    }

    // 3. 初始化输出
    if (!m_output->open(m_url, m_encoder->codecParameters(),
                        m_encoder->timeBase())) {
        emit error("Failed to open output");
        m_encoder->close();
        m_capturer->close();
        return;
    }


    // 4. 初始化转换器（如果采集格式不是 YUV420P，且未外部提供则内部创建）
    if (!m_converter) {
        m_converter = new FrameConverter; // 注意所有权，可改为智能指针。这里是为什么？如果不传入进来，就新建一个？为什么这个这么特殊？那这里肯定要做内存指针管理的，用unique_ptr？要怎么改？
                                        //我改成m_converter = std::make_unique<FrameConverter>(); 应该是不对的，这样的话，m_converter就要改成智能指针，但是传入的是裸指针啊？要如何修改？
    }
    if (srcFormat != AV_PIX_FMT_YUV420P) {
        if (!m_converter->isOpened())
            m_converter->open(resolution.width(), resolution.height(), srcFormat,
                              resolution.width(), resolution.height(), AV_PIX_FMT_YUV420P);
    }

    //所有组件初始化结束，开始推流
    // 5. 写输出头
    if (!m_output->writeHeader()) {
        emit error("Failed to write header");
        m_output->close();
        m_encoder->close();
        m_capturer->close();
        return;
    }

    m_running = true;
    m_frameIndex = 0;
    emit started();


    // // 写头成功后
    // m_running = true;
    // m_frameIndex = 0;
    // emit started();

    // // 持续发送红色帧
    // while (m_running) {
    //     AVFrame *testFrame = av_frame_alloc();
    //     testFrame->format = AV_PIX_FMT_YUV420P;
    //     testFrame->width  = 1920;
    //     testFrame->height = 1080;
    //     av_frame_get_buffer(testFrame, 0);
    //     // 白色 (TV range: Y=235, U=128, V=128)
    //     memset(testFrame->data[0], 235, testFrame->width * testFrame->height);
    //     memset(testFrame->data[1], 128, (testFrame->width/2) * (testFrame->height/2));
    //     memset(testFrame->data[2], 128, (testFrame->width/2) * (testFrame->height/2));
    //     testFrame->pts = m_frameIndex++;

    //     if (!m_encoder->sendFrame(testFrame)) {
    //         qWarning() << "sendFrame failed";
    //         av_frame_free(&testFrame);
    //         continue;
    //     }
    //     av_frame_free(&testFrame);

    //     // 立即取出并写入
    //     m_encoder->receivePackets([this](const EncodedPacket &pkt) {
    //         if (!m_output->writePacket(pkt)) {
    //             qWarning() << "writePacket failed";
    //         }
    //     });

    //     // 控制帧率（~30fps）
    //     QThread::msleep(33);

    //     if (m_frameIndex % 30 == 0)
    //         qDebug() << "Pushed" << m_frameIndex << "red frames";
    // }


    // 6. 采集-转换-编码-输出循环
    m_capturer->start([this](AVFrame *rawFrame) {   //这里注入回调函数，同步调用，告诉m_capturer获取到avframe之后怎么用
        if (!m_running) return;

        AVFrame *frameToEncode = rawFrame;   //准备去编码的帧，先取原始帧。
        AVFrame *convertedFrame = nullptr;   //转换后的avframe
        // 颜色空间转换（如果需要）
        if (rawFrame->format != AV_PIX_FMT_YUV420P) {   //如果格式不对
            convertedFrame = m_converter->convert(rawFrame);  //转换器来进行转换
            if (!convertedFrame) {   //转换失败了
                qWarning() << "Frame conversion failed";
                return;
            }
            frameToEncode = convertedFrame;
        }

        frameToEncode->pts = m_frameIndex++;

        if (!m_encoder->sendFrame(frameToEncode))
        {
            qWarning() << "sendFrame failed";
        } else
        {
            m_encoder->receivePackets([this](const EncodedPacket &pkt) {   //还是通过回调函数读取到转换好的帧
                if (!m_running) return;
                m_output->writePacket(pkt);
            });
        }

        if (convertedFrame)
            av_frame_free(&convertedFrame);
    });

    // 7. 冲刷编码器
    m_encoder->sendFrame(nullptr);  //一个经验问题，需要冲刷编码器，把剩下来的帧都读出来
    m_encoder->receivePackets([this](const EncodedPacket &pkt) {  //每次sendFrame都要运行这个函数，告诉encoder解码出来的帧要怎么做
        m_output->writePacket(pkt);
    });

    // 8. 收尾
    m_output->writeTrailer();
    m_output->close();
    m_encoder->close();
    m_capturer->close();
    m_running = false;

    emit stopped();
}

void StreamPusher::stop()
{
    if (m_running) {
        m_running = false;
        if (m_capturer)   //为什么别组成部分不需要close()?
            m_capturer->stop(); // 触发中断回调，av_read_frame 退出
    }
}

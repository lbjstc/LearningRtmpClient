#include "h264encoder.h"
#include <QDebug>

extern "C" {
#include <libavutil/opt.h>
}

H264Encoder::H264Encoder()   //为什么不把open放在构造函数里？而是单独手动open，这个设计思路如何理解？因为open可以返回bool，判断是否成功（构造函数里只能抛异常）。延迟初始化。可以重复使用，更换参数再次工作。
{
}

H264Encoder::~H264Encoder()
{
    close();
}

bool H264Encoder::open(const QSize &resolution, int inputPixelFormat,
                       int fps, int bitrate)
{
    QMutexLocker locker(&m_mutex);   //锁，保证线程安全
    if (m_codecCtx) {  //要保证首次启动
        qWarning() << "Encoder already opened";
        return false;
    }

    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);  //找到解码器
    if (!codec) {
        qWarning() << "H.264 encoder not found";
        return false;
    }
    else
    {
        qDebug() << "Using encoder:" << codec->name;
    }

    m_codecCtx = avcodec_alloc_context3(codec);   //给解码器分配上下文
    if (!m_codecCtx) {
        qWarning() << "Cannot allocate encoder context";
        return false;
    }
    //给解码器上下文设置各种参数包括：宽高、帧率、时间基、帧的格式，gop，b帧间隔，
    m_codecCtx->width = resolution.width();
    m_codecCtx->height = resolution.height();
    m_timeBase = {1, fps};
    m_codecCtx->time_base = m_timeBase;
    m_codecCtx->framerate = {fps, 1};
    m_codecCtx->pix_fmt = AV_PIX_FMT_YUV420P; // 编码器内部要求 YUV420P
    m_codecCtx->gop_size = fps * 2;  // 每2秒一个关键帧
    m_codecCtx->max_b_frames = 0;    // 零延迟
    if (bitrate > 0)
        m_codecCtx->bit_rate = bitrate;
    m_codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;  //这个是为了输出sps pps
    //低延迟参数
    av_opt_set(m_codecCtx->priv_data, "preset", "fast", 0);
    av_opt_set(m_codecCtx->priv_data, "tune", "zerolatency", 0);

    // //测试用

    // AVCodecContext *ctx = m_codecCtx;
    // ctx->width = resolution.width();
    // ctx->height = resolution.height();
    // ctx->time_base = AVRational{1, fps};        // 时间基 {1, 30}
    // ctx->framerate = AVRational{fps, 1};        // 帧率 {30, 1} 必须设置！
    // ctx->pix_fmt = AV_PIX_FMT_YUV420P;          // 输入格式
    // ctx->max_b_frames = 0;                      // 无 B 帧，低延迟


    int ret = avcodec_open2(m_codecCtx, codec, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "Cannot open encoder:" << errbuf;
        return false;
    }
    if(ret == 0)
    {
        qDebug()<<"success open encoder";
    }


    // 保存 codec parameters 以便外部获取
    m_codecPar = avcodec_parameters_alloc();
    avcodec_parameters_from_context(m_codecPar, m_codecCtx);

    qDebug() << "H264Encoder opened:" << resolution << "fps:" << fps;
    return true;
}

void H264Encoder::close()
{
    QMutexLocker locker(&m_mutex);
    if (m_codecCtx) {   //得把成员变量给释放
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }
    if (m_codecPar) {  //另一个成员变量
        avcodec_parameters_free(&m_codecPar);
        m_codecPar = nullptr;
    }
}

bool H264Encoder::sendFrame(AVFrame *frame)
{
    QMutexLocker locker(&m_mutex);
    if (!m_codecCtx) return false;
    int ret = avcodec_send_frame(m_codecCtx, frame);
    if (ret < 0) {   //如果失败了，要打印错误，用ffmpeg的方式
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        qWarning() << "avcodec_send_frame error:" << errbuf;
        return false;
    }
    return true;
}

bool H264Encoder::receivePackets(EncodedPacketCallback cb)
{
    QMutexLocker locker(&m_mutex);
    if (!m_codecCtx) return false;
    AVPacket *pkt = av_packet_alloc();
    while (true) {
        int ret = avcodec_receive_packet(m_codecCtx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            qWarning() << "avcodec_receive_packet error:" << errbuf;
            break;
        }
        EncodedPacket encPkt;  //意思是通过自定义编码包，把关心的数据复制出来，再拿出来用。这样的好处是什么？为什么不直接用avpacket，太臃肿了？
        encPkt.data = pkt->data;  //传递的是指针，还是浅拷贝。
        encPkt.size = pkt->size;
        encPkt.pts = pkt->pts;
        encPkt.dts = pkt->dts;
        cb(encPkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    return true;
}

int64_t H264Encoder::timeBaseNum() const
{
    QMutexLocker locker(&m_mutex);
    return m_timeBase.num;
}

int64_t H264Encoder::timeBaseDen() const
{
    QMutexLocker locker(&m_mutex);
    return m_timeBase.den;
}

AVRational H264Encoder::timeBase() const
{
    QMutexLocker locker(&m_mutex);
    return m_timeBase;
}

const AVCodecParameters* H264Encoder::codecParameters() const
{
    QMutexLocker locker(&m_mutex);
    return m_codecPar;
}

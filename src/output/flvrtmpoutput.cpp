#include "flvrtmpoutput.h"
#include "ivideoencoder.h"   // for EncodedPacket definition
#include <QDebug>

extern "C" {
#include <libavutil/avutil.h>
}

FlvRtmpOutput::FlvRtmpOutput()
{
    avformat_network_init(); // 确保网络初始化，可放在全局，但多次调用无副作用
}

FlvRtmpOutput::~FlvRtmpOutput()
{
    close();
}

bool FlvRtmpOutput::open(const QString &url,
                         const AVCodecParameters *videoParams,
                         AVRational encTimeBase)
{
    QMutexLocker locker(&m_mutex);
    if (m_outCtx) {
        qWarning() << "Output already opened";
        return false;
    }

    // 1. 分配输出上下文
    int ret = avformat_alloc_output_context2(&m_outCtx, nullptr, "flv", url.toUtf8().constData());  //又是最重要的上下文AVFormatContext，存储全部信息，根据格式和推流的协议创建
    if (ret < 0 || !m_outCtx) {
        qWarning() << "Cannot create output context for" << url;
        return false;
    }

    // 2. 添加视频流
    m_videoStream = avformat_new_stream(m_outCtx, nullptr);  //这个怎么理解，推流的话，需要先创建流。我可不可以理解为，这部分的设置其实是为rtmp的握手、publish服务的?错
    //这个操作是在输出上下文里创建一条新的流，这条流代表了你要推送的那个视频轨。它不是直接做 RTMP 握手或 publish 命令，而是为封装层预分配一个流对象...你可以理解为：创建流 = 注册一条轨道的元信息，真正网络通信的触发是在 avformat_write_header 和 avio_open 中。
    //需要注意，是在m_outCtx创建了一条流
    if (!m_videoStream) {
        qWarning() << "Cannot create video stream";
        avformat_free_context(m_outCtx);
        m_outCtx = nullptr;
        return false;
    }

    // 3. 拷贝编码参数到流
    ret = avcodec_parameters_copy(m_videoStream->codecpar, videoParams);   //把视频的参数拷贝到流里面
    if (ret < 0) {
        qWarning() << "Cannot copy codec parameters";
        avformat_free_context(m_outCtx);
        m_outCtx = nullptr;
        return false;
    }

    m_videoStream->time_base = encTimeBase;
    m_encTimeBase = encTimeBase; // 保存，供 writePacket 转换用

    // 4. 打开 IO
    if (!(m_outCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_outCtx->pb, m_outCtx->url, AVIO_FLAG_WRITE);  //为什么要打开IO？因为url需要，还有不需要的场景吗？
        if (ret < 0) {
            qWarning() << "Cannot open output IO:" << url;
            avformat_free_context(m_outCtx);
            m_outCtx = nullptr;
            return false;
        }
    }

    qDebug() << "FlvRtmpOutput opened:" << url;
    return true;
}

void FlvRtmpOutput::close()
{
    QMutexLocker locker(&m_mutex);
    if (m_outCtx) {
        if (m_headerWritten)
            writeTrailer();
        if (!(m_outCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&m_outCtx->pb);
        avformat_free_context(m_outCtx);
        m_outCtx = nullptr;
        m_videoStream = nullptr;
        m_headerWritten = false;
    }
}

bool FlvRtmpOutput::writeHeader()//avformat_write_header 还负责写入 脚本 tag（onMetaData） 以及 视频/音频的 sequence header（即 AVC sequence header 或 AAC sequence header），里面带有 SPS/PPS 等解码必需的全局参数。
{
    QMutexLocker locker(&m_mutex);
    if (!m_outCtx || m_headerWritten) return false;
    int ret = avformat_write_header(m_outCtx, nullptr);
    if (ret < 0) {
        qWarning() << "Error writing header";
        return false;
    }
    m_headerWritten = true;
    return true;
}

bool FlvRtmpOutput::writePacket(const EncodedPacket &pkt)
{
    QMutexLocker locker(&m_mutex);
    if (!m_outCtx || !m_videoStream) return false;

    AVPacket *avPkt = av_packet_alloc();  //构造一个新的avpacket用于发送
    avPkt->data = pkt.data;
    avPkt->size = pkt.size;
    avPkt->pts = pkt.pts;
    avPkt->dts = pkt.dts;
    avPkt->stream_index = m_videoStream->index;

    // 时间基转换：从编码器时间基 -> 流时间基
    av_packet_rescale_ts(avPkt, m_encTimeBase, m_videoStream->time_base); // 编码器时间基硬编码 30fps? 需要从外部传入。稍后修正。
    int ret = av_interleaved_write_frame(m_outCtx, avPkt);
    av_packet_free(&avPkt);
    return ret >= 0;
}

bool FlvRtmpOutput::writeTrailer()  //发送结尾 av_write_trailer 会更新 FLV 文件头的某些字段（如 duration），并可能写入结束标识。对于 RTMP 推流，它还会发送 FCUnpublish、deleteStream 等命令，优雅关闭流。
{
    QMutexLocker locker(&m_mutex);
    if (!m_outCtx || !m_headerWritten) return false;
    int ret = av_write_trailer(m_outCtx);
    m_headerWritten = false;
    return ret >= 0;
}

bool FlvRtmpOutput::isOpened() const
{
    QMutexLocker locker(&m_mutex);
    return m_outCtx != nullptr;
}

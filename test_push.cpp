// extern "C" {
// #include <libavcodec/avcodec.h>
// #include <libavformat/avformat.h>
// #include <libavdevice/avdevice.h>
// #include <libavutil/avutil.h>
// #include <libavutil/time.h>
// #include <libavutil/opt.h>
// #include <libavutil/imgutils.h>
// #include <libswscale/swscale.h>
// }

// #include <iostream>
// #include <cstdio>
// #include <signal.h>

// // 全局标志，用于优雅退出
// static volatile int keep_running = 1;  //为什么要用volatile？
// void int_handler(int) { keep_running = 0; }

// int test_push()
// {
//     // 注册信号处理，按 Ctrl+C 退出
//     signal(SIGINT, int_handler);  //什么意思？

//     // 1. 初始化
//     avdevice_register_all();
//     avformat_network_init();

//     // 2. 打开输入：屏幕采集 (gdigrab)
//     const AVInputFormat *ifmt = av_find_input_format("gdigrab");  //这是在干啥？  按名称查找输入格式。
//     if (!ifmt) {
//         std::cerr << "gdigrab not found" << std::endl;  //std::cerr相比std::cout有什么优势？
//         return -1;
//     }
//     AVDictionary *options = nullptr;  //为什么需要AVDictionary。是ffmpeg的键值对参数容器，用于传递设备或编码器的选项。
//     av_dict_set(&options, "framerate", "30", 0);
//     av_dict_set(&options, "draw_mouse", "1", 0); // 显示鼠标
//     // 采集整个桌面
//     AVFormatContext *inCtx = avformat_alloc_context();   //这个上下文又是什么作用？AVFormatContext封装的“大局”上下文，管理整个输入文件/设备的流信息。
//     //avformat_open_input：打开输入。"desktop" 是 gdigrab 的特殊 URL（代表整个桌面），使用指定格式 ifmt 打开，设备参数通过 options 传入。成功后 inCtx 里就有了屏幕捕获的“容器”。
//     if (avformat_open_input(&inCtx, "desktop", ifmt, &options) != 0) {  //这个的意思就是把AVInputFormat和AVDictionary打开desktop然后把信息放入inCtx？desktop是特殊的url

//         std::cerr << "Cannot open desktop" << std::endl;
//         return -1;
//     }
//     av_dict_free(&options);  //这就释放了？

//     if (avformat_find_stream_info(inCtx, nullptr) < 0) {  //然后从AVFormatContext查找流
//         std::cerr << "Cannot find stream info" << std::endl;
//         return -1;
//     }

//     int videoIndex = -1;
//     for (unsigned i = 0; i < inCtx->nb_streams; i++) {   //从AVFormatContext找视频流
//         if (inCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
//             videoIndex = i;
//             break;
//         }
//     }
//     if (videoIndex < 0) {     //再次确认视频流存在
//         std::cerr << "No video stream" << std::endl;
//         return -1;
//     }

//     AVCodecParameters *inCodecPar = inCtx->streams[videoIndex]->codecpar;  //流的参数提取出来。宽高、像素格式、编码ID等
//     std::cout << "Screen: " << inCodecPar->width << "x" << inCodecPar->height
//               << ", format: " << av_get_pix_fmt_name((AVPixelFormat)inCodecPar->format) << std::endl;

//     // 3. 输出：RTMP 推流
//     AVFormatContext *outCtx = nullptr;   //输出也需要一个AVFormatContext
//     // 注意：URL 中的 IP 和端口请改成你的服务器实际地址
//     const char* rtmp_url = "rtmp://10.0.0.100:1935/live/screen";
//     if (avformat_alloc_output_context2(&outCtx, nullptr, "flv", rtmp_url) < 0) {  //这里创建一个输出的outCtx。根据flv和url自动创建一个输出上下文，URL的协议会自动探测
//         std::cerr << "Cannot create output context" << std::endl;
//         return -1;
//     }

//     // 查找 H.264 编码器
//     const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_H264);  //创建编码器
//     if (!encoder) {
//         std::cerr << "H.264 encoder not found" << std::endl;
//         return -1;
//     }
//     AVCodecContext *encCtx = avcodec_alloc_context3(encoder);  //编码器上下文
//     encCtx->width = inCodecPar->width;    //从流的参数里提取数据
//     encCtx->height = inCodecPar->height;
//     encCtx->time_base = {1, 30};          // 30 fps
//     encCtx->framerate = {30, 1};
//     encCtx->pix_fmt = AV_PIX_FMT_YUV420P; // 编码器输入格式
//     encCtx->gop_size = 60;                // 每 2 秒一个关键帧
//     encCtx->max_b_frames = 0;             // 零延迟：不使用 B 帧
//     encCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; // 存储全局头到 extradata

//     // 编码器参数调整
//     av_opt_set(encCtx->priv_data, "preset", "fast", 0);   //提升编码速度
//     av_opt_set(encCtx->priv_data, "tune", "zerolatency", 0);  //关闭帧间延迟，适合实时推流

//     if (avcodec_open2(encCtx, encoder, nullptr) < 0) {  //打开编码器，关键函数
//         std::cerr << "Cannot open encoder" << std::endl;
//         return -1;
//     }

//     // 新建输出流并拷贝编码器参数
//     //avformat_new_stream：在输出上下文里新建一条流（视频流），它必须关联到一个 AVFormatContext，因为封装格式需要知道有哪些流。
//     AVStream *outStream = avformat_new_stream(outCtx, nullptr);  //为什么要用outCtx来新建？
//     if (!outStream) {
//         std::cerr << "Cannot create output stream" << std::endl;
//         return -1;
//     }
//     avcodec_parameters_from_context(outStream->codecpar, encCtx);  //流的参数从编码器拷贝出来,把编码器上下文的参数复制到流的 codecpar 里，这样 FLV 封装时才能正确写入流信息。
//     outStream->time_base = encCtx->time_base;   //时间基不在参数列表里？为什么要单独赋值？因为codecpar里面不包括时间基

//     // 打开输出 IO
//     if (!(outCtx->oformat->flags & AVFMT_NOFILE)) {   //这是在干啥？检查标志：如果输出格式不是“无文件”（即需要 I/O，如文件、网络），则打开 I/O 通道。RTMP 需要网络连接，所以会执行 avio_open，建立到 RTMP 服务器的连接。
//         if (avio_open(&outCtx->pb, outCtx->url, AVIO_FLAG_WRITE) < 0) {  //打开io的输出
//             std::cerr << "Cannot open output URL: " << outCtx->url << std::endl;
//             return -1;
//         }
//     }

//     // 写 FLV 头
//     // 写 FLV 文件头（包含 onMetaData、序列头等信息），对 RTMP 来说就是把 connect、createStream、publish 等协议交互完成，并发送元数据和编码器参数。这里有问题啊？问题一：还没有编解码呢，哪来的原信息？
//     if (avformat_write_header(outCtx, nullptr) < 0) {  //啥意思？写什么头呢？
//         std::cerr << "Error writing header" << std::endl;
//         return -1;
//     }

//     // 4. 初始化视频转换 (BGRA -> YUV420P)
//     SwsContext *swsCtx = sws_getContext(   //视频转换器
//         encCtx->width, encCtx->height, (AVPixelFormat)inCodecPar->format, // 输入格式（通常 bgra 或 bgr0）
//         encCtx->width, encCtx->height, AV_PIX_FMT_YUV420P,
//         SWS_BILINEAR, nullptr, nullptr, nullptr);//这一行代码在干啥
//     if (!swsCtx) {
//         std::cerr << "Cannot create SwsContext" << std::endl;
//         return -1;
//     }

//     // 分配帧
//     AVFrame *yuvFrame = av_frame_alloc();
//     yuvFrame->format = encCtx->pix_fmt;  //帧的格式
//     yuvFrame->width = encCtx->width;
//     yuvFrame->height = encCtx->height;
//     av_frame_get_buffer(yuvFrame, 0);  //什么意思？av_frame_get_buffer 根据设置的 format/width/height 真正分配图像数据缓冲区（对齐后的内存）。

//     AVPacket *inPkt = av_packet_alloc();
//     int frameCount = 0;
//     int64_t startTime = av_gettime_relative(); // 用于计算 PTS（相对时间，微秒）//问题二，这个不太懂在干啥？？

//     // 5. 主采集－编码－推流循环
//     while (keep_running) {
//         // 读一帧原始数据（gdigrab 直接给出完整的未压缩帧）
//         int ret = av_read_frame(inCtx, inPkt);
//         if (ret < 0) {
//             std::cerr << "Read frame error: " << ret << std::endl;
//             break;
//         }
//         if (inPkt->stream_index != videoIndex) {
//             av_packet_unref(inPkt);
//             continue;
//         }

//         // 构造输入 AVFrame（从原始 packet 数据）
//         AVFrame *rawFrame = av_frame_alloc();
//         rawFrame->format = inCodecPar->format;
//         rawFrame->width = inCodecPar->width;
//         rawFrame->height = inCodecPar->height;
//         av_image_fill_arrays(rawFrame->data, rawFrame->linesize,   //这是在干啥？这行将原始 packet 的数据指针赋给 rawFrame 的 data 数组和 linesize，无需拷贝。
//                              inPkt->data, (AVPixelFormat)inCodecPar->format,
//                              inCodecPar->width, inCodecPar->height, 1);
//         // 注意：inPkt->data 大小应为 rawFrame->linesize[0] * height （对于 packed 格式）
//         //问题三：datalinesize是什么意思？data可能是帧的具体数据

//         // 转换颜色空间 BGRA -> YUV420P
//         sws_scale(swsCtx, rawFrame->data, rawFrame->linesize, 0,    //这是在干啥啊？
//                   encCtx->height, yuvFrame->data, yuvFrame->linesize);
//         // 把 rawFrame 的 BGRA 数据缩放/转换为 yuvFrame 的 YUV420P。

//         av_frame_free(&rawFrame);  // 释放我们手动分配的临时 frame（不释放数据，因数据属于 packet）  问题四：数据属于packet是啥意思？因为前门
//         av_packet_unref(inPkt); // 归还 packet 内部引用，数据不再需要  问题五：为啥不需要了？

//         // 设置 PTS（基于帧序号和时间基）
//         // 时间基为 {1, 30}，则每帧增量 = 1，即 PTS = frameCount
//         yuvFrame->pts = frameCount;

//         // 编码
//         ret = avcodec_send_frame(encCtx, yuvFrame);  //把转换好颜色格式的frame，投入到编码器中，
//         if (ret < 0) {
//             std::cerr << "Error sending frame to encoder" << std::endl;
//             continue;
//         }

//         while (ret >= 0) {   //为什么又开始循环了？为了循环把一个frame读干净，都读到packet中，然后写入outCtx？应该是的
//             AVPacket *encPkt = av_packet_alloc();
//             ret = avcodec_receive_packet(encCtx, encPkt);  //从编码器上下文，receive？
//             if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
//                 av_packet_free(&encPkt);
//                 break;
//             } else if (ret < 0) {
//                 std::cerr << "Error encoding frame" << std::endl;
//                 av_packet_free(&encPkt);
//                 break;
//             }

//             // 将 packet 的时间基转换为输出流的时间基
//             av_packet_rescale_ts(encPkt, encCtx->time_base, outStream->time_base);
//             encPkt->stream_index = outStream->index;

//             // 写入输出
//             ret = av_interleaved_write_frame(outCtx, encPkt);  //把packet写入到输出流上下文，这个应该是核心目的吧。。。
//             //av_interleaved_write_frame：将编码后的数据包写入输出上下文，内部按时间戳交错写入以保证 FLV 的 tag 顺序正确，这是最终把 H.264 流推送到 RTMP 服务器的核心操作。
//             if (ret < 0) {
//                 std::cerr << "Error writing frame" << std::endl;
//             }
//             av_packet_free(&encPkt);
//         }

//         frameCount++;  //手动计数？

//         // 控制推流速率（以采集帧率自然控制，这里简单打印）
//         if (frameCount % 30 == 0) {
//             std::cout << "Pushed " << frameCount << " frames" << std::endl;
//         }
//     }

//     // 6. 清理
//     std::cout << "Flushing encoder..." << std::endl;
//     avcodec_send_frame(encCtx, nullptr); // 冲刷编码器.// 送空帧表示结束，让编码器输出剩余帧
//     AVPacket *flushPkt = av_packet_alloc();
//     while (avcodec_receive_packet(encCtx, flushPkt) == 0) {
//         // 继续写出残余的压缩包
//         av_packet_rescale_ts(flushPkt, encCtx->time_base, outStream->time_base);
//         flushPkt->stream_index = outStream->index;
//         av_interleaved_write_frame(outCtx, flushPkt);
//         av_packet_unref(flushPkt);  //unref和free有啥区别？递减内部引用计数，但不清空结构体
//     }
//     av_packet_free(&flushPkt);// 彻底释放 AVPacket 对象
//     //av_packet_unref vs av_packet_free：unref 清理包内分配的数据，将包重置为默认状态；free 是连 AVPacket 结构体本身也释放掉。

//     av_write_trailer(outCtx);  //啥意思？
//     //写 FLV 尾部信息，对 RTMP 来说就是发送 FCUnpublish、deleteStream 之类的命令，并关闭网络连接。

//     av_frame_free(&yuvFrame);
//     sws_freeContext(swsCtx);
//     avcodec_free_context(&encCtx);
//     avformat_close_input(&inCtx);
//     if (outCtx && !(outCtx->oformat->flags & AVFMT_NOFILE))
//         avio_closep(&outCtx->pb);
//     avformat_free_context(outCtx);

//     std::cout << "Push finished, total frames: " << frameCount << std::endl;
//     return 0;
// }



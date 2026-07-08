#ifndef STREAMPUSHER_H
#define STREAMPUSHER_H

#include <QObject>
#include <QString>
#include <atomic>
#include "ivideocapturer.h"
#include "ivideoencoder.h"
#include "irtmpoutput.h"
#include "frameconverter.h"

class StreamPusher : public QObject
{
    Q_OBJECT
public:
    explicit StreamPusher(QObject *parent = nullptr);
    ~StreamPusher();
    //把组件和参数注入进来
    void setCapturer(IVideoCapturer *capturer);
    void setEncoder(IVideoEncoder *encoder);
    void setOutput(IRtmpOutput *output);
    void setConverter(FrameConverter *converter);
    void setUrl(const QString &url);
    void setCaptureSource(const QString &source = "desktop");

public slots:   //这样设计槽的目的是什么？就是别的类控制我什么时候执行？怎么理解槽函数的设计？所以process就是用来给pthread用来响应started信号执行的？
    void process();  // 在工作线程中执行。。。。相当于等待命令，发起就开始执行，所有的主逻辑都在这里面呢
    void stop();     // 停止推流

signals:   //信号的发射，传递信息，传递命令。
    void started();
    void stopped();
    void error(const QString &msg);

private:
    //三个纯虚函数接口，为了实现松耦合，高层模块不依赖于底层模块，解耦合。但是也可能存在过度设计的问题。
    IVideoCapturer *m_capturer = nullptr;
    IVideoEncoder *m_encoder = nullptr;
    IRtmpOutput *m_output = nullptr;

    FrameConverter *m_converter = nullptr;
    //具体的推流信息，往哪里推？从哪里获取信息？
    QString m_url;
    QString m_captureSource;
    //多线程场景下的控制标识符
    std::atomic<bool> m_running{false};
    int m_frameIndex = 0; // 用于 PTS 递增；；；//这种参数的设计应该就是根据经验了，pts在推流的时候需要手动设置很重要呗
};

#endif // STREAMPUSHER_H

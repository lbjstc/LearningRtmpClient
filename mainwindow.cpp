#include "mainwindow.h"
#include "ui_mainwindow.h"


extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/avutil.h>
}

#include <flvrtmpoutput.h>
#include <h264encoder.h>
#include <QThread>
#include <screencapturer.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 注册设备与网络
    avdevice_register_all();
    avformat_network_init();

    qDebug() << "FFmpeg version:" << av_version_info();
    qDebug() << "Available input devices:";
    // 可简单列出设备（可选）
    //设置个默认值
    ui->urlInput->setText("rtmp://10.0.0.100:1935/live/screen");
}

MainWindow::~MainWindow()
{
    if (m_pushThread && m_pushThread->isRunning()) {
        if (m_pusher) m_pusher->stop();
        m_pushThread->quit();
        m_pushThread->wait(3000);
    }
    // 注意：如果线程没来得及退出，强制 wait 后可能还残留对象，但 Qt 会随主窗口销毁清理
    delete ui;
}

void MainWindow::on_startBtn_clicked()
{

    QString url = ui->urlInput->text().trimmed();
    if (url.isEmpty()) return;

    // 如果已有正在运行的推流，先停止
    if (m_pusher && m_pushThread && m_pushThread->isRunning()) {
        on_stopBtn_clicked();
    }

    // 创建各模块（推流结束后会自动删除，见下方 connect）
    auto *capturer = new ScreenCapturer;
    auto *encoder = new H264Encoder;
    auto *output = new FlvRtmpOutput;
    auto *converter = new FrameConverter; // 也可传 nullptr，让 StreamPusher 内部按需创建

    m_pusher = new StreamPusher;
    m_pusher->setCapturer(capturer);
    m_pusher->setEncoder(encoder);
    m_pusher->setOutput(output);
    m_pusher->setConverter(converter);
    m_pusher->setUrl(url);
    m_pusher->setCaptureSource("desktop");

    m_pushThread = new QThread(this);
    m_pusher->moveToThread(m_pushThread);

    // 线程启动后执行推流
    connect(m_pushThread, &QThread::started, m_pusher, &StreamPusher::process);

    // 推流停止或出错 -> 退出线程
    connect(m_pusher, &StreamPusher::stopped, m_pushThread, &QThread::quit);
    connect(m_pusher, &StreamPusher::error, m_pushThread, &QThread::quit);

    // 线程结束后清理所有对象
    connect(m_pushThread, &QThread::finished, this, [this, capturer, encoder, output, converter]() {
        // 安全删除所有模块（此时工作线程已结束，对象不再被使用）
        delete capturer;
        delete encoder;
        delete output;
        delete converter;
        delete m_pusher;
        m_pusher = nullptr;

        // 线程对象稍后由 deleteLater 清理（或直接 delete this）
        m_pushThread->deleteLater();
        m_pushThread = nullptr;

        ui->statusbar->showMessage("推流已停止");
        ui->startBtn->setEnabled(true);
        ui->stopBtn->setEnabled(false);
    });

    // 状态反馈
    connect(m_pusher, &StreamPusher::started, this, [this]() {
        ui->statusbar->showMessage("推流中…");
        ui->startBtn->setEnabled(false);
        ui->stopBtn->setEnabled(true);
    });
    connect(m_pusher, &StreamPusher::error, this, [this](const QString &msg) {
        ui->statusbar->showMessage("推流错误: " + msg);
    });

    m_pushThread->start();
}


void MainWindow::on_stopBtn_clicked()
{
    if (m_pusher) {
        m_pusher->stop();  // 通知采集循环退出（m_running = false）
        // 随后线程会自然退出，触发 finished 进行清理
    }
}


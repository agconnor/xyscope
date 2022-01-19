#include <QtCore>
#include <QtWidgets>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QAudioInput>
#include <QByteArray>
#include <QComboBox>
#include <QMainWindow>
#include <QObject>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>

#include <QWidget>
#include <QScopedPointer>
#include <QAudioInput>
#include <qendian.h>

#include "raster_view.hpp"
#include "hilbert_scatter_view.hpp"
#include "spectrum_view.hpp"



class AudioInfo : public QIODevice
{
    Q_OBJECT

public:
    AudioInfo(const QAudioFormat &format);

    void start();
    void stop();

    qint16 * dataFrame() const {return m_dataFrame;}
    quint64 dataLen() const {return m_dataLen;}

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    void setDataMutex(QMutex * dataMutex) { m_dataMutex = dataMutex;}

private:
    const QAudioFormat m_format;
    qint16 * m_dataFrame;
    quint64 m_dataLen;
    QMutex * m_dataMutex;

signals:
    void update();
};

AudioInfo::AudioInfo(const QAudioFormat &format)
    : m_format(format)
{
    m_dataLen = 0;
    m_dataFrame = new qint16[FRAME_SIZE];
}

void AudioInfo::start()
{
    open(QIODevice::WriteOnly);
}

void AudioInfo::stop()
{
    close();
}

qint64 AudioInfo::readData(char *data, qint64 maxlen)
{
    Q_UNUSED(data)
    Q_UNUSED(maxlen)

    return 0;
}

qint64 AudioInfo::writeData(const char *data, qint64 len)
{
    Q_ASSERT(m_format.sampleSize() % 8 == 0);
    const int channelBytes = m_format.sampleSize() / 8;
    const int sampleBytes = m_format.channelCount() * channelBytes;
    Q_ASSERT(len % sampleBytes == 0);
    const int numSamples = len / sampleBytes;

    if(m_dataMutex->tryLock()) {
        memcpy(m_dataFrame, data, numSamples * m_format.channelCount() * channelBytes);
        m_dataLen = numSamples * m_format.channelCount();
        m_dataMutex->unlock();
    }

    
    emit update();
    return len;
}






class Window : public QMainWindow
{
    Q_OBJECT
    
public:
    Window() : QMainWindow(),
    timerConnection(*(new QMetaObject::Connection)),
    audioUpdateConnection(*(new QMetaObject::Connection))
    {
        QWidget *window = new QWidget;
        m_layout = new QVBoxLayout;
        m_timer = new QTimer();
        
        unsigned int refresh_rate = 25; // refresh/S

        hilbert_canvas = new HilbertScatterView(this);
        spectrum_canvas = new SpectrumView(this);
        m_canvas = spectrum_canvas;
        m_layout->setMargin(0);
        m_layout->addWidget(m_canvas);

        sourcesMenu = menuBar()->addMenu(tr("&Source"));
        
        const QAudioDeviceInfo &defaultDeviceInfo = QAudioDeviceInfo::defaultInputDevice();
        for (auto &deviceInfo: QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
            QAction * srcAction = new QAction(deviceInfo.deviceName(), this);
            connect(srcAction, &QAction::triggered, this, [this, deviceInfo](){
                deviceChanged(deviceInfo);
            });
        
            sourcesMenu->addAction(srcAction);
        }
        
        viewsMenu = menuBar()->addMenu(tr("&View"));
        hilbertScanAction = new QAction(tr("Analytic Signal Scan"), this);
        connect(hilbertScanAction, &QAction::triggered, this, &Window::viewChanged);
        viewsMenu->addAction(hilbertScanAction);
        spectrumAction = new QAction(tr("Spectrum"), this);
        connect(spectrumAction, &QAction::triggered, this, &Window::viewChanged);
        viewsMenu->addAction(spectrumAction);
        //connect(m_deviceBox, QOverload<int>::of(&QComboBox::activated), this, &Window::deviceChanged);
        //layout->addWidget(m_deviceBox);

        m_volumeSlider = new QSlider(Qt::Horizontal, this);
        m_volumeSlider->setRange(0, 100);
        m_volumeSlider->setValue(100);
        connect(m_volumeSlider, &QSlider::valueChanged, this, &Window::sliderChanged);
        //layout->addWidget(m_volumeSlider);

        m_modeButton = new QPushButton(this);
        connect(m_modeButton, &QPushButton::clicked, this, &Window::toggleMode);
        //layout->addWidget(m_modeButton);

        m_suspendResumeButton = new QPushButton(this);
        connect(m_suspendResumeButton, &QPushButton::clicked, this, &Window::toggleSuspend);
        //layout->addWidget(m_suspendResumeButton);

        window->setLayout(m_layout);

        setCentralWidget(window);
        window->show();
        

        
        timerConnection = connect(m_timer, &QTimer::timeout, m_canvas,[this](){
            m_canvas->update();
        });
        m_timer->start(1000/refresh_rate);
        initializeAudio(defaultDeviceInfo);
    }
    
    void initializeAudio(const QAudioDeviceInfo &deviceInfo)
    {
        QAudioFormat format;
        format.setSampleRate(48000);
        format.setChannelCount(1);
        format.setSampleSize(16);
        format.setSampleType(QAudioFormat::SignedInt);
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setCodec("audio/pcm");

        if (!deviceInfo.isFormatSupported(format)) {
            qWarning() << "Default format not supported - trying to use nearest";
            format = deviceInfo.nearestFormat(format);
        }

        m_audioInfo.reset(new AudioInfo(format));
        m_audioInfo->setDataMutex(m_canvas->dataMutex());
        audioUpdateConnection = connect(m_audioInfo.data(), &AudioInfo::update, [this]() {
            m_canvas->setData(m_audioInfo->dataFrame(), m_audioInfo->dataLen());
        });

        m_audioInput.reset(new QAudioInput(deviceInfo, format));
        qreal initialVolume = QAudio::convertVolume(m_audioInput->volume(),
                                                    QAudio::LinearVolumeScale,
                                                    QAudio::LogarithmicVolumeScale);
        m_volumeSlider->setValue(qRound(initialVolume * 100));
        m_audioInfo->start();
        toggleMode();
    }
    
//    void resizeEvent(QResizeEvent *) {
//        m_audioInput->suspend();
//        m_canvas->doResize();
//        m_audioInput->resume();
//
//    }
    
    #ifndef QT_NO_CONTEXTMENU
//    void contextMenuEvent(QContextMenuEvent *event)
//    {
//        QMenu menu(this);
//        menu.addAction(srcAction);
//        menu.addAction(ppAction);
//        menu.addAction(volAction);
//        menu.exec(event->globalPos());
//    }
    #endif // QT_NO_CONTEXTMENU
    
    void keyPressEvent(QKeyEvent * event) {
        switch(event->key())
        {
            case Qt::Key_Space:
                toggleSuspend();
                break;
        }
    }
    
private slots:
    void toggleMode();
    void toggleSuspend();
    void deviceChanged(const QAudioDeviceInfo & device);
    void sliderChanged(int value);
    void viewChanged(bool);

private:
    // Owned by layout
    RasterView *m_canvas = nullptr;
    QPushButton *m_modeButton = nullptr;
    QPushButton *m_suspendResumeButton = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QVBoxLayout *m_layout = nullptr;
    QTimer *m_timer = nullptr;
    QMetaObject::Connection & timerConnection;
    QMetaObject::Connection & audioUpdateConnection;

    QScopedPointer<AudioInfo> m_audioInfo;
    QScopedPointer<QAudioInput> m_audioInput;
    bool m_pullMode = true;

    QMenu * sourcesMenu;
    QMenu * viewsMenu;
    QAction * hilbertScanAction;
    QAction * spectrumAction;
    
    HilbertScatterView * hilbert_canvas;
    SpectrumView * spectrum_canvas;
};

void Window::viewChanged(bool)
{
    const QAction *la = NULL;
    RasterView * new_canvas = nullptr;
    if(dynamic_cast<QAction *>(sender()) != NULL) {
        la = dynamic_cast<QAction *>(sender());
    }
    if(la != NULL) {
        if(la==hilbertScanAction)
            new_canvas = hilbert_canvas;
        else if(la == spectrumAction)
            new_canvas = spectrum_canvas;
    }
#pragma omp critical
    if(new_canvas != NULL && m_canvas != new_canvas) {
        m_audioInput->suspend();
        
        disconnect(audioUpdateConnection);
        disconnect(timerConnection);
        
        m_audioInfo->setDataMutex(new_canvas->dataMutex());
        audioUpdateConnection = connect(m_audioInfo.data(), &AudioInfo::update, new_canvas, [new_canvas, this]() {
            new_canvas->setData(m_audioInfo->dataFrame(), m_audioInfo->dataLen());
        });
        timerConnection = connect(m_timer, &QTimer::timeout, new_canvas,[new_canvas](){
            new_canvas->update();
        });
        m_layout->replaceWidget(m_canvas, new_canvas);
        m_canvas = new_canvas;
        m_audioInput->resume();
    }
}
void Window::toggleMode()
{
    m_audioInput->stop();
    toggleSuspend();

    // Change bewteen pull and push modes
    if (m_pullMode) {
        m_modeButton->setText(tr("Enable push mode"));
        m_audioInput->start(m_audioInfo.data());
    } else {
        m_modeButton->setText(tr("Enable pull mode"));
        auto io = m_audioInput->start();
        connect(io, &QIODevice::readyRead,
            [&, io]() {
                
                qint64 len = m_audioInput->bytesReady();
                const int BufferSize = FRAME_SIZE;
                if (len > BufferSize)
                    len = BufferSize;

                QByteArray buffer(len, 0);
                qint64 l = io->read(buffer.data(), len);
                if (l > 0)
                    m_audioInfo->write(buffer.constData(), l);
            });
    }

    m_pullMode = !m_pullMode;
}

void Window::toggleSuspend()
{
    // toggle suspend/resume
    if (m_audioInput->state() == QAudio::SuspendedState || m_audioInput->state() == QAudio::StoppedState) {
        m_audioInput->resume();
        m_suspendResumeButton->setText(tr("Suspend recording"));
    } else if (m_audioInput->state() == QAudio::ActiveState) {
        m_audioInput->suspend();
        m_suspendResumeButton->setText(tr("Resume recording"));
    } else if (m_audioInput->state() == QAudio::IdleState) {
        // no-op
    }
}

void Window::deviceChanged(const QAudioDeviceInfo & device)
{
    m_audioInfo->stop();
    m_audioInput->stop();
    m_audioInput->disconnect(this);
    initializeAudio(device);
}

void Window::sliderChanged(int value)
{
    qreal linearVolume = QAudio::convertVolume(value / qreal(100),
                                               QAudio::LogarithmicVolumeScale,
                                               QAudio::LinearVolumeScale);

    m_audioInput->setVolume(linearVolume);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    Window window;
    window.resize(600, 600);
    window.show();
    return app.exec();
}


#include "main.moc"

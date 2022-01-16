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

#include <fftw3.h>

class AudioInfo : public QIODevice
{
    Q_OBJECT

public:
    AudioInfo(const QAudioFormat &format);

    void start();
    void stop();

    qreal level() const { return m_level; }
    qint16 * dataFrame() const {return m_dataFrame;}
    quint64 dataLen() const {return m_dataLen;}

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    void setDataMutex(QMutex * dataMutex) { m_dataMutex = dataMutex;}

private:
    const QAudioFormat m_format;
    quint32 m_maxAmplitude = 0;
    qreal m_level = 0.0; // 0.0 <= m_level <= 1.0
    qint16 * m_dataFrame;
    quint64 m_dataLen;
    QMutex * m_dataMutex;

signals:
    void update();
};

AudioInfo::AudioInfo(const QAudioFormat &format)
    : m_format(format)
{
    switch (m_format.sampleSize()) {
    case 8:
        switch (m_format.sampleType()) {
        case QAudioFormat::UnSignedInt:
            m_maxAmplitude = 255;
            break;
        case QAudioFormat::SignedInt:
            m_maxAmplitude = 127;
            break;
        default:
            break;
        }
        break;
    case 16:
        switch (m_format.sampleType()) {
        case QAudioFormat::UnSignedInt:
            m_maxAmplitude = 65535;
            break;
        case QAudioFormat::SignedInt:
            m_maxAmplitude = 32767;
            break;
        default:
            break;
        }
        break;

    case 32:
        switch (m_format.sampleType()) {
        case QAudioFormat::UnSignedInt:
            m_maxAmplitude = 0xffffffff;
            break;
        case QAudioFormat::SignedInt:
            m_maxAmplitude = 0x7fffffff;
            break;
        case QAudioFormat::Float:
            m_maxAmplitude = 0x7fffffff; // Kind of
        default:
            break;
        }
        break;

    default:
        break;
    }
    m_dataFrame = new qint16[8192];
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
    if (m_maxAmplitude) {
        Q_ASSERT(m_format.sampleSize() % 8 == 0);
        const int channelBytes = m_format.sampleSize() / 8;
        const int sampleBytes = m_format.channelCount() * channelBytes;
        Q_ASSERT(len % sampleBytes == 0);
        const int numSamples = len / sampleBytes;

        
        m_dataMutex->lock();
        memcpy(m_dataFrame, data, numSamples * m_format.channelCount() * channelBytes);
//        qint16 maxValue = 0;
//        const unsigned char *ptr = reinterpret_cast<const unsigned char *>(data);

//        qint16 value = 0;
//        for (int i = 0; i < numSamples; ++i) {
//            for (int j = 0; j < m_format.channelCount(); ++j) {
//
//
//                if (m_format.sampleSize() == 8 && m_format.sampleType() == QAudioFormat::UnSignedInt) {
//                    value = *reinterpret_cast<const quint8*>(ptr);
//                } else if (m_format.sampleSize() == 8 && m_format.sampleType() == QAudioFormat::SignedInt) {
//                    value = qAbs(*reinterpret_cast<const qint8*>(ptr));
//                } else if (m_format.sampleSize() == 16 && m_format.sampleType() == QAudioFormat::UnSignedInt) {
//                    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
//                        value = qFromLittleEndian<quint16>(ptr) - 32768;
//                    else
//                        value = qFromBigEndian<quint16>(ptr) - 32768;
//                } else if (m_format.sampleSize() == 16 && m_format.sampleType() == QAudioFormat::SignedInt) {
//                    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
//                        value = qFromLittleEndian<qint16>(ptr);
//                    else
//                        value = qFromBigEndian<qint16>(ptr);
//                } else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::UnSignedInt) {
//                    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
//                        value = (qFromLittleEndian<quint32>(ptr) >> 16)  - 32768;
//                    else
//                        value = (qFromBigEndian<quint32>(ptr) >> 16) - 32768;
//                } else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::SignedInt) {
//                    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
//                        value = qFromLittleEndian<qint32>(ptr) >> 16;
//                    else
//                        value = qFromBigEndian<qint32>(ptr) >> 16;
//                } else if (m_format.sampleSize() == 32 && m_format.sampleType() == QAudioFormat::Float) {
//                    value = (qint32) (*reinterpret_cast<const float*>(ptr) * 0x7fffffff)>>16; // assumes 0-1.0
//                }
//
//                maxValue = qMax(value, maxValue);
//                ptr += channelBytes;
//                m_dataFrame[i] = value;
//            }
//
//        }

//        maxValue = qMin(maxValue, m_maxAmplitude);
//        m_level = qreal(maxValue) / m_maxAmplitude;
        m_dataLen = numSamples * m_format.channelCount();
        m_dataMutex->unlock();
    }

    
    emit update();
    return len;
}


class RenderArea : public QWidget
{
    Q_OBJECT

public:
    explicit RenderArea(QWidget *parent) : QWidget(parent)
    {
        m_valMutex = new QMutex;
        m_dataMutex = new QMutex;
        in   = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * 8192);
        m_data = new qint16[8192];
        setBackgroundRole(QPalette::Base);
        setAutoFillBackground(true);

        setMinimumHeight(450);
        setMinimumWidth(450);
    }
    
    QMutex * dataMutex() { return m_dataMutex; }
    void refresh()
    {
        int x, y;
        
        if(!m_dataMutex->tryLock()) {
            return;
        }
        int N = m_dataLen;

        // Set N to the number of complex elements in the input array

        

        // Initialize 'in' with N complex entries

        for(int32_t n = 0; n < N ; n++) {
            in[n][0] = (qreal) m_data[n] / 32768.0;
            in[n][1] = 0.0;
        }
        m_dataMutex->unlock();
        
        fftw_plan inPlan, outPlan;
        inPlan = fftw_plan_dft_1d(N, in, in, FFTW_FORWARD, FFTW_ESTIMATE);

        fftw_execute(inPlan);

        for(int32_t n = 0; n < N/2 ; n++) {
            in[n][0] *= 2;
            in[n][1] *= 2;
        }
        for(int32_t n = N/2; n < N ; n++) {
            in[n][0] = 0;
            in[n][1] = 0;
        }

        outPlan = fftw_plan_dft_1d(N, in, in, FFTW_BACKWARD, FFTW_ESTIMATE);
        fftw_execute(outPlan);
        // Use 'out' for something


        
        int trigger_offset = 0;
        int x0 = 0, y0 = 0;
        //n_read = fread((char *) in_, sizeof(short), 2*framesize, stdin);
        if(!m_valMutex->tryLock()) {
            return;
        }
        for(int32_t n = 0; n < N ; n++) {
            x0 = x;
            y0 = y;
            y = qMax(0,qMin(89,qFloor(in[n][0]*90 + 45)));
            x = qMax(0,qMin(89,qFloor(in[n][1]*90 + 45)));
            
            if(x > 45 && x0 <= 45 && y > 45 && qAbs(x-45) + qAbs(y-45) > 10) {
                trigger_offset = n;
                m_doRefresh = 1;
            }
            
            if(m_doRefresh) {
                int incr = 127 + qFloor(128 * (qreal) ((n - trigger_offset) % N) / (qreal) N);
                val[0][x][y] = qMin(val[0][x][y] + incr, 255);
                val[1][x][y] = 255;
            }
        }
        fftw_destroy_plan(inPlan);
        fftw_destroy_plan(outPlan);
        for(int r = 0; r < 90; r++) {
            for (int c = 0; c < 90; c++) {
                if(val[0][r][c] > 0) {
                    val[0][r][c] = qMax(0, val[0][r][c] - 32);
                }
                if(val[1][r][c] > 0) {
                    val[1][r][c] *= .75;
                }
            }
        }
        m_valMutex->unlock();
    }
    

    void setData(qint16 * data, qint64 len) {
        memcpy(m_data, data, len*sizeof(qint16));
        m_dataLen = len;
        refresh();
        
    }
    
    void setLevel(qreal value)
    {
        m_level = value;
        update();
    }
protected:
    void paintEvent(QPaintEvent *)
    {
        QPainter painter(this);

        painter.setPen(Qt::black);
        painter.drawRect(QRect(painter.viewport().left()+10,
                               painter.viewport().top()+10,
                               painter.viewport().right()-20,
                               painter.viewport().bottom()-20));
        if(!m_valMutex->tryLock())
            return;
        painter.setPen(Qt::NoPen);
        for(int r = 0; r < 90; r++) {
          for (int c = 0; c < 90; c++) {
              painter.setBrush(QColor(0, val[0][r][c], val[1][r][c]));
              painter.drawRect(QRect(r*5, c*5, 5, 5));
          }
        }
        m_doRefresh = 0;
        m_valMutex->unlock();
    }

    
private:
    qreal m_level = 0;
    qint16 * m_data;
    qint64 m_dataLen;
    QPixmap m_pixmap;
    QMutex * m_valMutex;
    QMutex * m_dataMutex;
    
    int m_doRefresh = 0;
    
    int val[2][90][90];
    fftw_complex *in;
};



class Window : public QMainWindow
{
    Q_OBJECT
    
public:
    Window() : QMainWindow()
    {
        QWidget *window = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout;

//        unsigned long sample_rate;
//
//
//        sample_rate = 48000L;

        unsigned int refresh_rate = 25; // refresh/S

        m_canvas = new RenderArea(this);
        layout->addWidget(m_canvas);

        m_deviceBox = new QComboBox(this);
        const QAudioDeviceInfo &defaultDeviceInfo = QAudioDeviceInfo::defaultInputDevice();
        m_deviceBox->addItem(defaultDeviceInfo.deviceName(), QVariant::fromValue(defaultDeviceInfo));
        for (auto &deviceInfo: QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
            if (deviceInfo != defaultDeviceInfo)
                m_deviceBox->addItem(deviceInfo.deviceName(), QVariant::fromValue(deviceInfo));
        }

        connect(m_deviceBox, QOverload<int>::of(&QComboBox::activated), this, &Window::deviceChanged);
        layout->addWidget(m_deviceBox);

        m_volumeSlider = new QSlider(Qt::Horizontal, this);
        m_volumeSlider->setRange(0, 100);
        m_volumeSlider->setValue(100);
        connect(m_volumeSlider, &QSlider::valueChanged, this, &Window::sliderChanged);
        layout->addWidget(m_volumeSlider);

        m_modeButton = new QPushButton(this);
        connect(m_modeButton, &QPushButton::clicked, this, &Window::toggleMode);
        layout->addWidget(m_modeButton);

        m_suspendResumeButton = new QPushButton(this);
        connect(m_suspendResumeButton, &QPushButton::clicked, this, &Window::toggleSuspend);
        layout->addWidget(m_suspendResumeButton);

        window->setLayout(layout);

        setCentralWidget(window);
        window->show();
        

        QTimer *timer = new QTimer();
        connect(timer, &QTimer::timeout, m_canvas,[this](){
            m_canvas->update();
        });
        timer->start(1000/refresh_rate);
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
        connect(m_audioInfo.data(), &AudioInfo::update, [this]() {
//            m_canvas->setLevel(m_audioInfo->level());
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
    

private slots:
    void toggleMode();
    void toggleSuspend();
    void deviceChanged(int index);
    void sliderChanged(int value);

private:
    // Owned by layout
    RenderArea *m_canvas = nullptr;
    QPushButton *m_modeButton = nullptr;
    QPushButton *m_suspendResumeButton = nullptr;
    QComboBox *m_deviceBox = nullptr;
    QSlider *m_volumeSlider = nullptr;

    QScopedPointer<AudioInfo> m_audioInfo;
    QScopedPointer<QAudioInput> m_audioInput;
    bool m_pullMode = true;
};

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
                const int BufferSize = 4096;
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

void Window::deviceChanged(int index)
{
    m_audioInfo->stop();
    m_audioInput->stop();
    m_audioInput->disconnect(this);

    initializeAudio(m_deviceBox->itemData(index).value<QAudioDeviceInfo>());
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
    window.resize(450, 450);
    window.show();
    return app.exec();
}


#include "main.moc"

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
#include <boost/multi_array.hpp>

#define FRAME_SPAN 64
#define FRAME_SIZE 8192

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


class RenderArea : public QWidget
{
    Q_OBJECT

public:
    explicit RenderArea(QWidget *parent) : QWidget(parent)
//    val(*new boost::multi_array<quint16, 3>())
    {
        m_valMutex = new QMutex();
        m_dataMutex = new QMutex();
        in   = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * FRAME_SIZE);
        m_data = new qint16[FRAME_SIZE];
        m_val = new boost::multi_array<quint16, 3>(boost::extents[3][m_maxX][m_maxY]);
        
        fftw_init_threads();
        fftw_plan_with_nthreads(4);
        
        setBackgroundRole(QPalette::Base);
        setAutoFillBackground(true);
        setMinimumHeight(600);
        setMinimumWidth(600);
    }
    
    QMutex * dataMutex() { return m_dataMutex; }
    void refresh()
    {
        int x, y;
        
        if(!m_dataMutex->tryLock()) {
            return;
        }
        int N = qMin((qint64) FRAME_SIZE, m_dataLen);

        for(int32_t n = 0; n < N ; n++) {
            in[n][0] = (qreal) m_data[n] / 32768.0;
            in[n][1] = 0.0;
        }
        m_dataMutex->unlock();
        
        
        inPlan = fftw_plan_dft_1d(N,  in, in, FFTW_FORWARD, FFTW_ESTIMATE);

        fftw_execute(inPlan);
        
        for(int32_t n = N/2; n < N ; n++) {
            in[n][0] = 0.0;
            in[n][1] = 0.0;
        }

        inPlan = fftw_plan_dft_1d(N,  in, in, FFTW_BACKWARD, FFTW_ESTIMATE);
        
        fftw_execute(inPlan);
        
        for(int32_t n = 0; n < N ; n++) {
            in[n][0] /= (double) N/2.0 / m_scale;
            in[n][1] /= (double) N/2.0 / m_scale;
        }
        
        fftw_destroy_plan(inPlan);
        
        int trigger_offset = 0;
        int x0 = 0, y0 = 0;
        //n_read = fread((char *) in_, sizeof(short), 2*framesize, stdin);
        if(m_valMutex->tryLock()) {
            if(m_val == NULL) {
                m_valMutex->unlock();
                return;
            }
            boost::multi_array<quint16, 3> &val = *m_val;
            m_doRefresh = 1;
            for(int32_t n = 0; n < N ; n++) {
                int maxSq = qMin(m_maxY, m_maxX);
                x0 = x;
                y0 = y;
                y = qFloor(in[n][0]*maxSq + m_maxY/2);
                x = qFloor(in[n][1]*maxSq + m_maxX/2);
                
                
                if(x > m_maxX/2 && x0 <= m_maxX/2 &&
                   qAbs(x-m_maxX/2) + qAbs(y-m_maxY/2) > maxSq/20) {
                    trigger_offset = n;
                    m_doRefresh = 1;
                }
                
                if(m_doRefresh && (
                    x >= 0 && x < m_maxX && y >= 0 && y < m_maxY)) {
                    int incr = 127 + qFloor(128 * (qreal) ((n - trigger_offset) % N) / (qreal) N);
                    val[1][x][y] = qMin(val[1][x][y] + incr, 255);
                    val[0][x][y] = 255;
                    incr = qFloor(255 * (qreal) n / (qreal) N);
                    val[2][x][y] = qMin(val[2][x][y] + incr, 255);
                }
            }
            for(boost::multi_array<quint16,3>::index r = 0; r < m_maxX; r++) {
                for (boost::multi_array<quint16,3>::index c = 0; c < m_maxY; c++) {
                    if(val[1][r][c] > 0) {
                        val[1][r][c] = qMin(255, qMax(0, val[1][r][c] - m_greenDecay));
                    }
                    if(val[2][r][c] > 0) {
                        val[2][r][c] *= m_blueDecay;
                    }
                    if(val[0][r][c] > 0) {
                        val[0][r][c] *= m_redDecay;
                    }
                }
            }
            m_valMutex->unlock();
        }
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
    void doResize() {
        m_valMutex->lock();
        m_maxX = rect().width()/3;
        m_maxY = rect().height()/3;
        if(m_val != NULL)
            delete m_val;
        m_val = new boost::multi_array<quint16, 3>(boost::extents[3][m_maxX][m_maxY]);
        m_valMutex->unlock();
    }
protected:
    void paintEvent(QPaintEvent *)
    {
        QPainter painter(this);

        painter.setPen(Qt::black);
        painter.drawRect(QRect(painter.viewport().left(),
                               painter.viewport().top(),
                               painter.viewport().right(),
                               painter.viewport().bottom()));
        if(m_valMutex->tryLock() && m_val != NULL) {
            if(m_val == NULL) {
                m_valMutex->unlock();
                return;
            }
            boost::multi_array<quint16, 3> &val = *m_val;
            painter.setPen(Qt::NoPen);
            for(boost::multi_array<quint16,3>::index r = 0; r < m_maxX; r++) {
                for (boost::multi_array<quint16,3>::index c = 0; c < m_maxY; c++) {
                  painter.setBrush(QColor(val[0][r][c], val[1][r][c], val[2][r][c]));
                  painter.drawRect(QRect(r*3, c*3, 3, 3));
              }
            }
            m_doRefresh = 0;
            m_valMutex->unlock();
        }
    }

    
    void resizeEvent(QResizeEvent *) {
        doResize();
    }
    
    void wheelEvent(QWheelEvent *ev)
    {
        if((ev->angleDelta().x() != 0 || ev->angleDelta().y() != 0) && m_valMutex->tryLock()) {
            
            if(QApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier)) {
                if(ev->angleDelta().x() > 0)
                    m_blueDecay += .01;
                else if(ev->angleDelta().x() < 0)
                    m_blueDecay -= .01;
                m_blueDecay = qMax(0.001, qMin(1.0, m_blueDecay));
                
                if(ev->angleDelta().y() > 0)
                    m_redDecay += .01;
                else if(ev->angleDelta().y() < 0)
                    m_redDecay -= .01;
                m_redDecay = qMax(0.001, qMin(1.0, m_redDecay));
            } else if(
                      QApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier)) {
                if(ev->angleDelta().x() > 0)
                    m_greenDecay += 4;
                else if(ev->angleDelta().x() < 0)
                    m_greenDecay -= 4;
                m_greenDecay = qMax(1, qMin(128, m_greenDecay));
            } else {
                if(ev->angleDelta().y() > 0) // up Wheel
                    m_scale *= 1.05;
                else if(ev->angleDelta().y() < 0) //down Wheel
                    m_scale /= 1.05;
                m_scale = qMax(0.001, qMin(1024.0, m_scale));
            }
            m_valMutex->unlock();
        }
    }
private:
    qreal m_level = 0;
    qreal m_scale = 1.0;
    qreal m_redDecay = .6667;
    qreal m_blueDecay = .75;
    int m_greenDecay = 16;
    qint16 * m_data;
    qint64 m_dataLen;
    QPixmap m_pixmap;
    QMutex * m_valMutex;
    QMutex * m_dataMutex;
    
    int m_doRefresh = 0;
    int m_maxX = 200;
    int m_maxY = 200;
    boost::multi_array<quint16, 3> * m_val = NULL;
    fftw_complex *in;
    fftw_plan inPlan;
};



class Window : public QMainWindow
{
    Q_OBJECT
    
public:
    Window() : QMainWindow()
    {
        QWidget *window = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout;

        unsigned int refresh_rate = 25; // refresh/S

        m_canvas = new RenderArea(this);
        layout->setMargin(0);
        layout->addWidget(m_canvas);

        sourcesMenu = menuBar()->addMenu(tr("&Source"));
        QMenu srcsMenu(this);
        
        const QAudioDeviceInfo &defaultDeviceInfo = QAudioDeviceInfo::defaultInputDevice();
        int i = 0;
        for (auto &deviceInfo: QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
            QAction * srcAction = new QAction(deviceInfo.deviceName(), this);
            connect(srcAction, &QAction::triggered, this, [this, deviceInfo](){
                deviceChanged(deviceInfo);
            });
        
            sourcesMenu->addAction(srcAction);
        }

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

private:
    // Owned by layout
    RenderArea *m_canvas = nullptr;
    QPushButton *m_modeButton = nullptr;
    QPushButton *m_suspendResumeButton = nullptr;
    QSlider *m_volumeSlider = nullptr;

    QScopedPointer<AudioInfo> m_audioInfo;
    QScopedPointer<QAudioInput> m_audioInput;
    bool m_pullMode = true;

    QMenu * sourcesMenu;
    QAction * volAction;
    QAction * ppAction;
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

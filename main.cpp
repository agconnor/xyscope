#include <QtCore>
#include <QtWidgets>

class RenderArea : public QWidget
{
    Q_OBJECT

public:
    explicit RenderArea(QWidget *parent, int framesize) : QWidget(parent), framesize(framesize)
    {
        setBackgroundRole(QPalette::Base);
        setAutoFillBackground(true);

        setMinimumHeight(900);
        setMinimumWidth(900);
    }
    
    void refresh()
    {
        int x, y;
        unsigned int n_read = 0;
        short in_[2*framesize];
        int trigger_offset = 0;
        int x0 = 0, y0 = 0;
        n_read = fread((char *) in_, sizeof(short), 2*framesize, stdin);
        for(uint32_t n = 0; n < n_read ; n+=2) {
            qreal in_i = (double) in_[n] / 65536.0,
            in_q = (double) in_[n+1] / 65536.0;
            x0 = x;
            y0 = y;
            y = qFloor(((in_i * 90) + 45));
            x = qFloor(((in_q * 90) + 45));
            if(x > 45 && x0 <= 45 && y > 45 && qAbs(x-45) + qAbs(y-45) > 40) {
                trigger_offset = n;
            }
            int incr = 127 + qFloor(128 * (qreal) ((n-trigger_offset) % framesize) / (qreal) framesize);
            val[0][x][y] = qMin(val[0][x][y] + incr, 255);
            val[1][x][y] = 255;
        }
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
        update();
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
        painter.setPen(Qt::NoPen);
        for(int r = 0; r < 90; r++) {
          for (int c = 0; c < 90; c++) {
              painter.setBrush(QColor(0, val[0][r][c], val[1][r][c]));
              painter.drawRect(QRect(r*10, c*10, 10, 10));
          }
        }
    }

    
private:
    QPixmap m_pixmap;
    int val[2][90][90];
    int framesize;
    
};

class Window : public QMainWindow
{
    Q_OBJECT
    
public:
    Window() : QMainWindow()
    {
        QWidget *window = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout;

        unsigned long sample_rate;
      
        
        sample_rate = 48000L;

        unsigned int refresh_rate = 25; // refresh/S

        // sample_rate / refresh_rate = # of "pixels"
        unsigned int framesize = sample_rate / refresh_rate;
        
        m_canvas = new RenderArea(this, framesize);
        layout->addWidget(m_canvas);

        window->setLayout(layout);

        setCentralWidget(window);
        window->show();
        
        

        QTimer *timer = new QTimer();
        connect(timer, &QTimer::timeout, m_canvas,
                      &RenderArea::refresh);
        timer->start(1000/refresh_rate);
    }
    
private:
    // Owned by layout
    RenderArea *m_canvas = nullptr;
};


int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    Window window;
    window.resize(900, 900);
    window.show();
    return app.exec();
}

#include "main.moc"

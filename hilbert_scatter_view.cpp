//
//  hilbert_scatter_view.cpp
//  xyscope
//
//  Created by )\( on 1/18/22.
//

#include "hilbert_scatter_view.hpp"

#include <QApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QPixmap>
#include <QImage>


HilbertScatterView::HilbertScatterView(QWidget *parent) : RasterView(parent)
{
    m_valMutex = new QMutex();
    m_dataMutex = new QMutex();
    in   = new std::complex<double>[FRAME_SIZE];
    m_data = new qint16[FRAME_SIZE];
    m_val = new val_array_t(boost::extents[3][200][200]);
    m_image = QImage(200, 200, QImage::Format_RGB16);
    
    
    fftw_init_threads();
    fftw_plan_with_nthreads(2);
    
    setBackgroundRole(QPalette::Base);
    setPalette(QPalette(QPalette::Window, Qt::black));
    setAutoFillBackground(true);
    
    setMinimumHeight(600);
    setMinimumWidth(600);
}

void HilbertScatterView::refresh()
{
    int x, y;
    
    if(!m_dataMutex->tryLock()) {
        return;
    }
    int N = qMin((qint64) FRAME_SIZE, m_dataLen);

    
    
    double pre[N];
    for(int32_t n = 0; n < N ; n++) {
        pre[n] = (double) m_data[n] / 32768.0;
    }
    inPlan = fftw_plan_dft_r2c_1d(N,  pre,
                                  reinterpret_cast<fftw_complex*>(in), FFTW_ESTIMATE);

    fftw_execute(inPlan);
    m_dataMutex->unlock();
    
    for(int32_t n = N/2; n < N ; n++) {
        in[n] = 0.0;
    }

    fftw_destroy_plan(inPlan);
    outPlan = fftw_plan_dft_1d(N,  reinterpret_cast<fftw_complex*>(in),
                                  reinterpret_cast<fftw_complex*>(in), FFTW_BACKWARD, FFTW_ESTIMATE);
    
    fftw_execute(outPlan);
    
    for(int32_t n = 0; n < N ; n++) {
        in[n] /= (double) N/2.0 / m_scale;
    }
    
    fftw_destroy_plan(outPlan);
    
    int trigger_offset = -1;
    std::complex<double> trigger_z;
    //n_read = fread((char *) in_, sizeof(short), 2*framesize, stdin);
//    if(m_valMutex->tryLock()) {
//        if(m_val == NULL) {
//            m_valMutex->unlock();
//            return;
//        }
        
        int m_maxX = m_image.width();
        int m_maxY = m_image.height();
        int maxSq = qMin(m_maxX, m_maxY);
        
        quint16 val[3][m_maxX][m_maxY];
    
        memset(val, 0, 3*m_maxX*m_maxY*sizeof(quint16));
        for(int32_t n = 1; n < N ; n++) {
            qreal trigger_level = .1;
            if(trigger_offset < 0 && real(in[n]) >= trigger_level && real(in[n-1]) < trigger_level) {
                trigger_offset = n;
                trigger_z = conj(in[n]) / abs(in[n]);
                n = 0;
            }
            if(trigger_offset < 0)
                continue;
            y = qFloor(real(in[n]*trigger_z)*maxSq + m_maxY/2);
            x = qFloor(imag(in[n]*trigger_z)*maxSq + m_maxX/2);
            
            if(x >= 0 && x < m_maxX && y >= 0 && y < m_maxY) {
                int incr = 127 + qFloor(128 * (qreal) ((n - trigger_offset) % N) / (qreal) N);
                val[1][x][y] = 255;
                val[0][x][y] = incr*m_redDecay;
                val[2][x][y] = incr*m_blueDecay;
                int M = 256/m_greenDecay;
                for(int x2 = qMax(0,x-M); x2 < qMin(m_maxX, x+M+1); x2++) {
                    if(x!=x2) {
                        val[2][x2][y] = qMax(0, (quint16) (incr*m_redDecay) - abs(x-x2)*m_greenDecay);
                    }
                            
                }
                for(int y2 = qMax(0,y-M); y2 < qMin(m_maxY, y+M+1); y2++) {
                    if(y!=y2) {
                        val[0][x][y2] = qMax(0, (quint16) (incr*m_blueDecay) - abs(y-y2)*m_greenDecay);
                    }
                }
        
            }
        }
        QPainter imgPainter(&m_image);
        imgPainter.setBrush(QColor(0,0,0,32));
        imgPainter.setPen(Qt::NoPen);
        imgPainter.drawRect(0,0,m_image.width(), m_image.height());
        #pragma omp parallel
        #pragma omp for
        for(qint32 r = 0; r < m_maxX; r++) {
            for (qint32 c = 0; c < m_maxY; c++) {
                if(val[0][r][c] > 0 && val[1][r][c] > 0 &&val[2][r][c] > 0) {
                    QColor color(m_image.pixel(r, c));
                    color.setRed(val[0][r][c]);
                    color.setGreen(val[1][r][c]);
                    color.setBlue(val[2][r][c]);
//                    color.setAlpha(240);
//                    color.setRed(qMin(255,qMax(0, (int)(color.red()*m_redDecay)+val[0][r][c])));
//                    color.setGreen(qMin(255, qMax(0, color.green() - m_greenDecay) + val[1][r][c]));
//                    color.setBlue(qMin(255,qMax(0, (int)(color.blue()*m_blueDecay)+val[2][r][c])));
                    m_image.setPixelColor(r, c, color);
                }
          }
        }
        
//        m_valMutex->unlock();
//    }
}

void HilbertScatterView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    
//    if(valMutex()->tryLock()) {
        painter.setPen(Qt::NoPen);
        
        painter.drawImage(painter.viewport(), m_image, m_image.rect());
//        valMutex()->unlock();
//    }
}

void HilbertScatterView::wheelEvent(QWheelEvent *ev)
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

void HilbertScatterView::resizeEvent(QResizeEvent *) {
//    valMutex()->lock();
    size_t maxX = rect().width()/3;
    size_t maxY = rect().height()/3;
    if(val() != NULL) {
        boost::array<size_t, 3> new_ext = {3, maxX, maxY};
        val()->resize(new_ext);
    }
    m_image = QImage(maxX, maxY, QImage::Format_RGB16);
    
//    valMutex()->unlock();
}


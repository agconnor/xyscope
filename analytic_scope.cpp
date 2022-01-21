//
//  hilbert_scatter_view.cpp
//  xyscope
//
//  Created by )\( on 1/18/22.
//



#include <QApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QPixmap>
#include <QImage>
#include <complex>
#include <qmath.h>
#include <fftw3.h>
#include "analytic_scope.hpp"

AnalyticScope::AnalyticScope(QWidget * parent) : RasterImage(parent)
{
    in   = new std::complex<double>[len()];
    fftw_init_threads();
    fftw_plan_with_nthreads(2);
}

AnalyticScope::~AnalyticScope()
{
    delete in;
}

void AnalyticScope::refreshImpl()
{
    int x, y;
    int N = qMin((quint32) FRAME_SIZE, len());
    
    
    
    double pre[N];
    for(int32_t n = 0; n < N ; n++) {
        pre[n] = (double) data()[n] / 32768.0;
    }
    inPlan = fftw_plan_dft_r2c_1d(N,  pre,
                                  reinterpret_cast<fftw_complex*>(in), FFTW_ESTIMATE);
    
    fftw_execute(inPlan);
 
    
    for(int32_t n = N/2; n < N ; n++) {
        in[n] = 0.0;
    }
    
    fftw_destroy_plan(inPlan);
    outPlan = fftw_plan_dft_1d(N,  reinterpret_cast<fftw_complex*>(in),
                               reinterpret_cast<fftw_complex*>(in), FFTW_BACKWARD, FFTW_ESTIMATE);
    
    fftw_execute(outPlan);
    
    for(int32_t n = 0; n < N ; n++) {
        in[n] /= (double) N / m_scale;
    }
    
    fftw_destroy_plan(outPlan);
    
    int trigger_offset = -1;
    std::complex<double> trigger_z;
    
    int m_maxX = width();
    int m_maxY = height();
    int maxSq = qMin(m_maxX, m_maxY);
    
    double val[3][m_maxX][m_maxY];
    
    memset(val, 0, 3*m_maxX*m_maxY*sizeof(double));
    
    for(int32_t n = 1; n < N ; n++) {
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
            double incr = (qreal) ((n - trigger_offset) % N) / (qreal) N;
            val[1][x][y] = 1.0;
            val[0][x][y] = incr*m_redDecay;
            val[2][x][y] = incr*m_blueDecay;
            int M = 256/m_greenDecay;
            for(int x2 = qMax(0,x-M); x2 < qMin(m_maxX, x+M+1); x2++) {
                if(x!=x2) {
                    val[2][x2][y] = qMax(0.0,  (incr*m_redDecay) - abs(x-x2)*m_greenDecay/256.0);
                }
                
            }
            for(int y2 = qMax(0,y-M); y2 < qMin(m_maxY, y+M+1); y2++) {
                if(y!=y2) {
                    val[0][x][y2] = qMax(0.0,  (incr*m_blueDecay) - abs(y-y2)*m_greenDecay/256.0);
                }
            }
        }
    }
    QPainter imgPainter(this);
    QColor color;
    color.setRgbF(0.0, 0.0, 0.0);
    color.setAlphaF(.15);
    imgPainter.setBrush(color);
    imgPainter.setPen(Qt::NoPen);
    imgPainter.drawRect(0,0,width(), height());
    for(qint32 r = 0; r < m_maxX; r++) {
        for (qint32 c = 0; c < m_maxY; c++) {
            if(val[0][r][c] > 0 && val[1][r][c] > 0 &&val[2][r][c] > 0) {
                color.setRedF(val[0][r][c]);
                color.setGreenF(val[1][r][c]);
                color.setBlueF(val[2][r][c]);
                setPixelColor(r, c, color);
            }
        }
    }
}



void AnalyticScope::wheelEvent(QWheelEvent *ev)
{
    if((ev->angleDelta().x() != 0 || ev->angleDelta().y() != 0)) {
        
        if(QApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier)) {
            
            if(ev->angleDelta().x() > 0)
                m_greenDecay += 4;
            else if(ev->angleDelta().x() < 0)
                m_greenDecay -= 4;
            m_greenDecay = qMax(1, qMin(128, m_greenDecay));
            
            if(ev->angleDelta().y() > 0) // up Wheel
                m_scale *= 1.05;
            else if(ev->angleDelta().y() < 0) //down Wheel
                m_scale /= 1.05;
            m_scale = qMax(0.001, m_scale);
            QApplication::activeWindow()->setWindowTitle(QString("[Scale: %1] [Green: %2]").arg( m_scale).arg(m_greenDecay));
        } else if(
                  QApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier)) {
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
            
            QApplication::activeWindow()->setWindowTitle(QString("[Red: %1] [Blue: %2]").arg( m_redDecay).arg(m_blueDecay));
        } else {
            if(ev->angleDelta().y() > 0.0)
                trigger_level += .01;
            else if(ev->angleDelta().y() < 0.0)
                trigger_level -= .01;
//            trigger_level = qBound(-1.0, trigger_level, 1.0);
            QApplication::activeWindow()->setWindowTitle(QString("[Trigger: %1]").arg( trigger_level));
            
        }
    }
}

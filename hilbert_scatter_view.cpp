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


HilbertScatterView::HilbertScatterView(QWidget *parent) : RasterView(parent)
{
    m_valMutex = new QMutex();
    m_dataMutex = new QMutex();
    in   = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * FRAME_SIZE);
    m_data = new qint16[FRAME_SIZE];
    m_val = new val_array_t(boost::extents[3][200][200]);
    
    fftw_init_threads();
    fftw_plan_with_nthreads(4);
    
    setBackgroundRole(QPalette::Base);
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
        val_array_t &val = *m_val;
        m_doRefresh = 1;
        int m_maxX = val.shape()[1];
        int m_maxY = val.shape()[2];
        int maxSq = qMin(m_maxX, m_maxY);
        for(int32_t n = 0; n < N ; n++) {
            
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
        for(val_idx_t r = 0; r < m_maxX; r++) {
            for (val_idx_t c = 0; c < m_maxY; c++) {
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

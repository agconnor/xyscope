//
//  spectrum_view.cpp
//  xyscope
//
//  Created by )\( on 1/18/22.
//


#include <QApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>

#include "spectrum_view.hpp"
#include <fftw3.h>

SpectrumView::SpectrumView(QWidget *parent) : RasterView(parent)
{
    m_valMutex = new QMutex();
    m_dataMutex = new QMutex();
    in   = (fftw_complex *)fftw_malloc(2 * sizeof(fftw_complex) * 40000);
    out   = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * 40000);
    in_w = in;
    in_r = in;
    m_X = 200;
    m_Y = 200;
    m_N = m_X * m_Y;
    m_data = new qint16[FRAME_SIZE];
    m_val = new val_array_t(boost::extents[3][m_X][m_Y]);
    
    fftw_init_threads();
    fftw_plan_with_nthreads(4);
    
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setMinimumHeight(600);
    setMinimumWidth(600);
}

void SpectrumView::refresh()
{
    if(!m_dataMutex->tryLock()) {
        return;
    }
    int M = qMin((qint64) FRAME_SIZE, m_dataLen);
    long N = m_N;

    int m_maxX = m_X;
    int m_maxY = m_Y;
    in_r = in_w = in;
    for(qint32 k = 0; k < M/FRAME_SPAN; k++) {
        for(qint32 m = 0; m < FRAME_SPAN; m++) {
            qint32 start = (m_maxX - M/FRAME_SPAN)/2*FRAME_SPAN+(m_maxY-FRAME_SPAN)/2;
            in_w[start + k*FRAME_SPAN + m][0] = (qreal) m_data[m] / 32768.0 * (double) M;
            in_w[start + k*FRAME_SPAN + m][1] = 0;
        }
    }
//    in_w = in + (((in_w - in) + M) % (2*N));
//    if((in_w - in_r) >= N+M || (in_w - in_r) <= M) {
//    in_r = in + (((in_r - in) + M) % (2*N));
//    }
    m_dataMutex->unlock();
    
    if(m_valMutex->tryLock()) {
        val_array_t &val = *m_val;
        inPlan = fftw_plan_dft_2d(m_val->shape()[1], m_val->shape()[2],  in_r, out, FFTW_FORWARD, FFTW_ESTIMATE);

        fftw_execute(inPlan);
        
        #pragma omp parallel
        #pragma omp for
        for(quint32 n = 0; n < N ; n++) {
            out[n][0] /= (double) N;
            out[n][1] /= (double) N;
        }
        
        fftw_destroy_plan(inPlan);
        
        if(m_val == NULL) {
            m_valMutex->unlock();
            return;
        }
 
        qreal scale = 2.0, shamt = 3.0;
        #pragma omp parallel
        #pragma omp for
        for(int x = 0; x < m_maxX ; x++) {
            for(int y = 0; y < m_maxY ; y++) {
                double i = out[x*m_maxY+y][0];
                double q = out[x*m_maxY+y][1];
                double lp = log(i*i+q*q);
                int x_ = (x+m_X/2) % m_X;
                int y_ = (y+m_Y/2) % m_Y;
                val[0][x_][y_] = qMin(255.0, exp(-pow(lp/scale,2.0))*256.0/(sqrt(M_PI)));
                val[1][x_][y_] = qMin(255.0, exp(-pow((lp-shamt)/scale,2.0))*256.0/(sqrt(M_PI)));
                val[2][x_][y_] = qMin(255.0, exp(-pow((lp-2.0*shamt)/scale,2.0))*256.0/(sqrt(M_PI)));
            }
        }
        m_valMutex->unlock();
    }
}

void SpectrumView::resizeEvent(QResizeEvent * event) {
    m_valMutex->lock();
    size_t maxX = rect().width()/3;
    size_t maxY = rect().height()/3;
    if(val() != NULL) {
        boost::array<size_t, 3> new_ext = {3, maxX, maxY};
        val()->resize(new_ext);
    }
    m_X = m_val->shape()[1];
    m_Y = m_val->shape()[2];
    m_N = m_X * m_Y;
    fftw_free(in);
    in = (fftw_complex *) fftw_malloc(m_N*sizeof(fftw_complex));
    in_r = in;
    in_w = in;
    fftw_free(out);
    out = (fftw_complex *) fftw_malloc(m_N*sizeof(fftw_complex));
    m_valMutex->unlock();
}

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
    in   = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * 40000);
    out   = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * 40000);
    in_w = in;
    m_data = new qint16[FRAME_SIZE];
    m_val = new val_array_t(boost::extents[3][200][200]);
    
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
    quint64 N = m_val->shape()[1] * m_val->shape()[2];
    
    for(qint32 m = 0; m < M ; m++) {
        in_w[m][0] = (qreal) m_data[m] / 32768.0;
        in_w[m][1] = 0.0;
    }
    in_w = in + (((in_w - in) + M) % N);
    
    m_dataMutex->unlock();
    
    if(m_valMutex->tryLock()) {
        inPlan = fftw_plan_dft_2d(m_val->shape()[1], m_val->shape()[2],  in, out, FFTW_FORWARD, FFTW_ESTIMATE);

        fftw_execute(inPlan);
        
        
        for(quint64 n = 0; n < N ; n++) {
            out[n][0] /= (double) N;
            out[n][1] /= (double) N;
        }
        
        fftw_destroy_plan(inPlan);
        
        if(m_val == NULL) {
            m_valMutex->unlock();
            return;
        }
        val_array_t &val = *m_val;
        int m_maxX = val.shape()[1];
        int m_maxY = val.shape()[2];
        for(int x = 0; x < m_maxX ; x++) {
            for(int y = 0; y < m_maxY ; y++) {
                double i = out[x*m_maxY+y][0];
                double q = out[x*m_maxY+y][1];
                double lp = log(i*i+q*q);
                val[0][x][y] = qMin(255.0, exp(-pow(lp/10,2.0))*256.0/sqrt(M_PI));
                val[1][x][y] = qMin(255.0, exp(-pow((lp-10)/10,2.0))*256.0/sqrt(M_PI));
                val[2][x][y] = qMin(255.0, exp(-pow((lp-20)/10,2.0))*256.0/sqrt(M_PI));
            }
        }
        m_valMutex->unlock();
    }
}

void SpectrumView::resizeEvent(QResizeEvent *) {
    m_valMutex->lock();
    fftw_free(in);
    in = (fftw_complex *) fftw_malloc(m_val->shape()[1]*m_val->shape()[2]*sizeof(fftw_complex));
    fftw_free(out);
    out = (fftw_complex *) fftw_malloc(m_val->shape()[1]*m_val->shape()[2]*sizeof(fftw_complex));
    m_valMutex->unlock();
}

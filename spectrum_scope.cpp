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

#include "spectrum_scope.hpp"
#include <fftw3.h>

SpectrumScope::SpectrumScope(QWidget *parent) : RasterImage(parent)
{
    m_X = width();
    m_Y = height();
    m_N = m_X * m_Y;
    
    in   = new std::complex<double>[m_N*2];
    out   = new std::complex<double>[m_N];
    in_w = in + m_N;
    in_r = in;
    
    fftw_init_threads();
    fftw_plan_with_nthreads(2);
}

SpectrumScope::~SpectrumScope()
{
    delete [] in;
    delete [] out;
}

void SpectrumScope::refreshImpl()
{
    int M = qMin((quint32) FRAME_SIZE, len());
    long N = m_N;
    
    int m_maxX = m_X;
    int m_maxY = m_Y;
    double pre[M];
    std::complex<double> in_[M];
    for(int32_t n = 0; n < M ; n++) {
        pre[n] = (double) data()[n] / 32768.0;
    }
    inPlan = fftw_plan_dft_r2c_1d(M,  pre,
                                  reinterpret_cast<fftw_complex*>(in_), FFTW_ESTIMATE);
    
    fftw_execute(inPlan);
    fftw_destroy_plan(inPlan);
    
    double mag_ = 0.0;
    for(int m = 0; m < M; m++) {
        mag_ = qMax(mag_, abs(in_[m]));
    }

    std::complex<double> decim[m_X*m_K];
    
    for(quint32 n = 0; n < m_X*m_K/4; n++) {
        decim[n] = in_[n] * (double)M/(double) (m_X*m_K);
        decim[m_X*m_K/2-n-1] = in_[m_X*m_K/2-n-1] * (double)M/(double) (m_X*m_K);
    }
//
//    std::complex<double> decim[m_X*K];
//
//    for(quint32 n = 0; n < m_X*K/4; n++) {
//        decim[n] = in_[n] * (double)M/(double) (m_X*K);
//        decim[m_X*K/2-n-1] = in_[m_X*K/2-n-1] * (double)M/(double) (m_X*K);
//    }
    
    inPlan = fftw_plan_dft_1d(m_X*m_K, reinterpret_cast<fftw_complex*>(decim),
                              reinterpret_cast<fftw_complex*>(in_w), FFTW_BACKWARD, FFTW_ESTIMATE);
//    memcpy(in_w+m_X, in_w, m_X*(K-1)*sizeof(std::complex<double>));
    
    in_r = in; //(((in_r - in) + m_L) % (m_N*2));
    in_w = in_r + (((in_w - in_r) + m_X*m_K) % m_N);
//    in_w = in + (((in_w - in) + m_X) % m_N);
    if(in_r - in > m_N) {
        std::complex<double> tmp[m_N];
        memcpy(tmp, in + m_N, m_N*sizeof(std::complex<double>));
        memcpy(in + m_N, in,m_N*sizeof(std::complex<double>));
        memcpy(in, tmp,m_N*sizeof(std::complex<double>));
        in_r -= m_N;
        in_w -= m_N;
        
    }
    
    fftw_execute(inPlan);
    fftw_destroy_plan(inPlan);
    
    inPlan = fftw_plan_dft_2d(m_X, m_Y,  reinterpret_cast<fftw_complex*>(in_r), reinterpret_cast<fftw_complex*>(out), FFTW_FORWARD, FFTW_ESTIMATE);
    
    fftw_execute(inPlan);
    fftw_destroy_plan(inPlan);
    
    for(quint32 n = 0; n < N ; n++) {
        out[n] /= (double) (N * scale)  ;
    }
    
    for(int x = 0; x < m_maxX ; x++) {
        for(int y = 0; y < m_maxY ; y++) {
            double mag = abs(out[x*m_maxY+y])/15.0;
            int x_ = (x+m_X/2) % m_X;
            int y_ = (y+m_Y/2) % m_Y;
            QColor color;
            color.setHsvF((arg(out[y*m_X+x])+M_PI)/(2*M_PI),
                          qBound(0.0, 1.0-mag*sat,1.0), qMin(1.0, mag));
            setPixelColor(x_, y_, color);
        }
    }
}


void SpectrumScope::resizeEvent(QResizeEvent *) {
    size_t maxX = rect().width();
    size_t maxY = rect().height();
    m_X = maxX;
    m_Y = maxY;
    m_N = m_X * m_Y;
    delete[] in;
    in = new std::complex<double>[m_N*2];
    m_K = qBound(0U, m_K, m_N-1);
    in_r = in;
    in_w = in+m_N;
    delete[] out;
    out = new std::complex<double>[m_N];
}

void SpectrumScope::wheelEvent(QWheelEvent *ev)
{
    if(QApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier)) {
        if(ev->angleDelta().y() > 0.0)
            scale = qBound(0.01, scale*1.05, 100.0);
        else if(ev->angleDelta().y() < 0.0)
            scale = qBound(0.01, scale/1.05, 100.0);
        
        if(ev->angleDelta().x() > 0.0)
            sat = qBound(0.01, sat+.01, 1.0);
        else if(ev->angleDelta().x() < 0.0)
            sat = qBound(0.01, sat-.01, 1.0);
    } else {
        if(ev->angleDelta().y() > 0.0)
            m_K = qBound(1U, m_K+1, 32U);
        else if(ev->angleDelta().y() < 0.0)
            m_K = qBound(1U, m_K-1, 32U);
    }
}

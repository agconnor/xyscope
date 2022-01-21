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
    in_w = in;
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
    
    std::complex<double> decim[m_X];
    
    for(quint32 n = 0; n < m_X/2; n++) {
        decim[n] = in_[n] * (double)M/(double) m_X;
        decim[m_X-n-1] = in_[m_X-n-1] * (double)M/(double) m_X;
    }
    
    in_r = in;
    in_w = in + (((in_w - in) + m_X) % m_N);
    
    inPlan = fftw_plan_dft_1d(m_X, reinterpret_cast<fftw_complex*>(decim),
                              reinterpret_cast<fftw_complex*>(in_w), FFTW_BACKWARD, FFTW_ESTIMATE);
    
    fftw_execute(inPlan);
    fftw_destroy_plan(inPlan);
    
    inPlan = fftw_plan_dft_2d(m_X, m_Y,  reinterpret_cast<fftw_complex*>(in_r), reinterpret_cast<fftw_complex*>(out), FFTW_FORWARD, FFTW_ESTIMATE);
    
    fftw_execute(inPlan);
    fftw_destroy_plan(inPlan);
    
    for(quint32 n = 0; n < N ; n++) {
        out[n] /= (double) N;
    }
    
    for(int x = 0; x < m_maxX ; x++) {
        for(int y = 0; y < m_maxY ; y++) {
            double mag = abs(out[x*m_maxY+y])/15.0;
            int x_ = (x+m_X/2) % m_X;
            int y_ = (y+m_Y/2) % m_Y;
            QColor color;
            color.setHsvF((arg(out[y*m_X+x])+M_PI)/(2*M_PI), 1.0, qMin(1.0, mag));
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
    in_r = &in[m_N];
    in_w = in;
    delete[] out;
    out = new std::complex<double>[m_N];
}

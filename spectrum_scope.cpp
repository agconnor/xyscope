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
    
    in_r = in;
    in_w = in;
    
    fft_dyn_alloc();
    
    fftw_init_threads();
    fftw_plan_with_nthreads(2);
}


 void SpectrumScope::fft_decim_set() {
     in_w = in + (m_Y/2 - m_Y/2/m_decimFactorV)*m_X;
     if(m_decimFactorV > 1)
         memset(in_w
                + m_N - m_Y/m_decimFactorV/4*m_X, 0,
         m_Y/m_decimFactorV/4*m_X
                *sizeof(std::complex<double>));
 }
 
 void SpectrumScope::fft_dyn_alloc() {
     if(in != NULL)
         fftw_free(in);
     in   = (std::complex<double> *) fftw_alloc_complex(m_N*2);

     if(out != NULL)
         fftw_free(out);
     out   = (std::complex<double> *) fftw_alloc_complex(m_N);
     fft_decim_set();
     decimPlan = fftw_plan_dft_1d(m_X,
                                  reinterpret_cast<fftw_complex*>(decim),
                                  reinterpret_cast<fftw_complex*>(in_w),
                                          FFTW_BACKWARD, FFTW_MEASURE);
     inPlan = fftw_plan_dft_2d(m_X, m_Y,
                 reinterpret_cast<fftw_complex*>(in),
                 reinterpret_cast<fftw_complex*>(out),
                     FFTW_FORWARD, FFTW_MEASURE);
  }
 

SpectrumScope::~SpectrumScope()
{
    fftw_destroy_plan(prePlan);
    fftw_destroy_plan(decimPlan);
    fftw_destroy_plan(inPlan);
    fftw_free((fftw_complex*)in);
    fftw_free((fftw_complex*)out);
}

void SpectrumScope::refreshImpl()
{
    int M = qMin((quint32) FRAME_SIZE, len());
    long N = m_N;
    
    int m_maxX = m_X;
    int m_maxY = m_Y;
    double pre[M];
    for(int32_t n = 0; n < M ; n++) {
        pre[n] = (double) data()[n] / 32768.0;
    }
    
    fftw_execute(prePlan);

    std::complex<double> decim[m_X];
    
    for(quint32 n = 0; n < m_X/4; n++) {
        decim[m_X/2-n-1] = decim[FRAME_SIZE/2-n-1];
    }
    
    fftw_execute_dft(decimPlan, reinterpret_cast<fftw_complex*>(decim),
                     reinterpret_cast<fftw_complex*>(in_w));

    for(quint32 n = 0; n < m_X ; n++) {
        in_w[n] /= (double) (m_X);
    }
    double mag_ = 0.0;
    for(quint32 m = 0; m < m_X; m++) {
        mag_ = qMax(mag_, abs(in_w[m]));
    }
    qint32 trigger_offset = -1;
    for(quint32 n = 1; n < m_X ; n++) {
        if(trigger_offset < 0 && real(in_w[n]) >= trigger_level * mag_ && real(in_w[n-1]) < trigger_level * mag_) {
            trigger_offset = n-1;
            break;
        }
    }
    if(trigger_offset >= 0 && trigger_offset < (int)m_X-1) {
        memcpy(in_w, in_w + trigger_offset, (m_X - trigger_offset)*sizeof(std::complex<double>));
        memset(in_w+(m_X - trigger_offset), 0, trigger_offset*sizeof(std::complex<double>));
    } else if(trigger_offset < 0) {
        memset(in_w, 0, m_X*sizeof(std::complex<double>));
    }
    in_r = in; //(((in_r - in) + m_L) % (m_N*2));
    in_w = in_r + (((in_w - in_r) + m_X) % m_N);
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
    
    for(quint32 n = 0; n < N ; n++) {
        out[n] /= (double) N  ;
        out[n] *= scale;
    }
    
    for(int x = 0; x < m_maxX ; x++) {
        for(int y = 0; y < m_maxY ; y++) {
            double mag = abs(out[x*m_maxY+y]);
            int x_ = (x+m_X/2) % m_X;
            int y_ = (y+m_Y/2) % m_Y;
            QColor color;
            color.setHsvF((arg(out[y*m_X+x])+M_PI)/(2*M_PI),
                    1.0 - qBound(0.0,
                                 qMax((mag-1.0)*sat,0.0),
                                 1.0),
                          qMin(1.0, mag));
            setPixelColor(x_, y_, color);
        }
    }
}


void SpectrumScope::postResize(){
    size_t maxX = rect().width();
    size_t maxY = rect().height();
    m_X = maxX;
    m_Y = maxY;
    m_N = m_X * m_Y;
    fft_dyn_alloc();
    setBandwidthTitle();
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
        QApplication::activeWindow()->setWindowTitle(QString("[Scale: %1] [Sat.: %2]").arg( scale).arg(sat));
    } else if(QApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier)) {
        if(ev->angleDelta().y() > 0.0)
            m_decimFactorV += 1;
        else if(ev->angleDelta().y() < 0.0)
            m_decimFactorV -= 1;
        m_decimFactorV = qBound(1U, m_decimFactorV, m_Y);
        setBandwidthTitle();
    } else {
        if(ev->angleDelta().y() > 0.0)
            trigger_level += .01;
        else if(ev->angleDelta().y() < 0.0)
            trigger_level -= .01;
        trigger_level = qBound(-1.0, trigger_level, 1.0);
        QApplication::activeWindow()->setWindowTitle(QString("[Trigger: %1%]").arg( trigger_level*100.0));
    }
}

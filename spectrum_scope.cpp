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
    
    pre = (double *) fftw_alloc_real(FRAME_SIZE);
    decim = (std::complex<double> *) fftw_alloc_complex(FRAME_SIZE);
    
    prePlan = fftw_plan_dft_r2c_1d(FRAME_SIZE, pre,
                                   reinterpret_cast<fftw_complex *>(decim), FFTW_MEASURE);
    fft_dyn_alloc();
    
    fftw_init_threads();
    fftw_plan_with_nthreads(2);
}


 void SpectrumScope::fft_decim_set() {
     in_w = in + (m_Y/2-m_Y/2/m_decimFactorV)*m_X;
     memset(in, 0, m_N*sizeof(std::complex<double>));
 }
 
 void SpectrumScope::fft_dyn_alloc() {
     if(in != NULL)
         fftw_free(in);
     in   = (std::complex<double> *) fftw_alloc_complex(m_N*2);

     if(out != NULL)
         fftw_free(out);
     out   = (std::complex<double> *) fftw_alloc_complex(m_N);
     fft_decim_set();
     decimPlan = fftw_plan_dft_1d(m_X/m_decimFactorH,
                                  reinterpret_cast<fftw_complex*>(decim+m_X/2 - m_X/2/m_decimFactorH),
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
    fftw_cleanup_threads();
}

void SpectrumScope::refreshImpl()
{
    int M = qMin((quint32) FRAME_SIZE, len());

    for(int32_t n = 0; n < M ; n++) {
        pre[n] = (double) data()[n] / 32768.0;
    }
    
    fftw_execute(prePlan);
    
//    memset(decim + m_X/m_decimFactorH, 0, (m_X - m_X/m_decimFactorH)*sizeof(std::complex<double>));
    
    fftw_execute_dft(decimPlan, reinterpret_cast<fftw_complex*>(decim),
                     reinterpret_cast<fftw_complex*>(in_w+m_X/2 - m_X/2/m_decimFactorH));

    for(quint32 n = 0; n < m_X ; n++) {
        in_w[n] /= (double) (m_X/m_decimFactorH);
    }
    
    qint32 trigger_offset = -1;
    for(quint32 n = m_X/2 - m_X/2/m_decimFactorH; n < m_X/2 + m_X/2/m_decimFactorH ; n++) {
        if(trigger_offset < 0 && abs(in_w[n]) >= trigger_level && arg(in_w[n]) >= 0.0) {
            trigger_offset = n;
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
    in_w = in_r + (m_Y/2-m_Y/2/m_decimFactorV)*m_X
                + (((in_w - (in_r + (m_Y/2-m_Y/2/m_decimFactorV)*m_X)) + m_X) % (m_N/m_decimFactorV));

    //fftw_execute(inPlan);
    memcpy(out, in, m_N*sizeof(std::complex<double>));
    
    for(quint32 n = 0; n < m_N ; n++) {
//        out[n] /= (double) N  ;
        out[n] *= scale;
    }
    
    for(quint32 x = 0; x < m_X ; x++) {
        for(quint32 y = 0; y < m_Y ; y++) {
            double mag = abs(out[y*m_X+x]);
            int x_ = (x+m_X/2) % m_X;
            int y_ = (y+m_Y/2) % m_Y;
            QColor color;
            color.setHsvF((arg(out[y*m_X+x])+M_PI)/(2*M_PI),
                    1.0 - qBound(0.0,
                                 qMax((mag-1.0)*sat,0.0),
                                 1.0),
                          qMin(1.0, mag));
            setPixelColor(x, y, color);
        }
    }
}


void SpectrumScope::postResize() {
    m_X = rect().width();
    m_Y = rect().height();
    m_N = m_X * m_Y;
    fft_dyn_alloc();
    setBandwidthTitle();
}

void SpectrumScope::wheelEvent(QWheelEvent *ev)
{
    if(QApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier)) {
        if(ev->angleDelta().y() > 0.0)
            scale *= 1.05;
        else if(ev->angleDelta().y() < 0.0)
            scale /= 1.05;
        scale = qBound(1e-10, scale, 1e10);
        
        if(ev->angleDelta().x() > 0.0)
            sat = qBound(0.01, sat+.01, 1.0);
        else if(ev->angleDelta().x() < 0.0)
            sat = qBound(0.01, sat-.01, 1.0);
        QApplication::activeWindow()->setWindowTitle(QString("[Scale: %1 dB] [Sat.: %2]").arg( log(scale)*10).arg(sat));
    } else if(QApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier)) {
        if(ev->angleDelta().y() > 0.0)
            m_decimFactorV += 1;
        else if(ev->angleDelta().y() < 0.0)
            m_decimFactorV -= 1;
        m_decimFactorV = qBound(1U, m_decimFactorV, m_Y);
        fft_decim_set();
        setBandwidthTitle();
    } else {
        if(ev->angleDelta().y() > 0.0)
            trigger_level *= 1.01;
        else if(ev->angleDelta().y() < 0.0)
            trigger_level /= 1.01;
        trigger_level = qBound(1e-10, trigger_level, 1e10);
        QApplication::activeWindow()->setWindowTitle(QString("[Trigger: %1 dB]").arg( log(trigger_level)*10.0));
    }
}

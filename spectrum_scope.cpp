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
    
    pre   = (std::complex<double> *) fftw_alloc_complex(FRAME_SIZE);
    decim = (std::complex<double> *) fftw_alloc_complex(FRAME_SIZE);
    
    prePlan = fftw_plan_dft_1d(FRAME_SIZE,
                               reinterpret_cast<fftw_complex *>(pre),
                               reinterpret_cast<fftw_complex *>(decim),
                               FFTW_FORWARD, FFTW_MEASURE);
    fft_dyn_alloc();
    fftw_init_threads();
    fftw_plan_with_nthreads(2);
}


 void SpectrumScope::fft_decim_set() {
     in_w = in + (m_Y/2-m_scanLines/2)*m_X;
     memset(in, 0, m_N*sizeof(std::complex<double>));
     decimPlan = fftw_plan_dft_1d(m_inputSamples,
                                  reinterpret_cast<fftw_complex*>(decim+m_X/2 - m_inputSamples/2),
                                  reinterpret_cast<fftw_complex*>(in_w),
                                          FFTW_BACKWARD, FFTW_MEASURE);
 }
 
 void SpectrumScope::fft_dyn_alloc() {
     if(in != NULL)
         fftw_free(in);
     in   = (std::complex<double> *) fftw_alloc_complex(m_N);

     if(out != NULL)
         fftw_free(out);
     out   = (std::complex<double> *) fftw_alloc_complex(m_N);
     fft_decim_set();

     
     inPlan = fftw_plan_dft_2d(m_X, m_Y,
                 reinterpret_cast<fftw_complex*>(out),
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

    for(int32_t n = 0; n < M/2 ; n++) {
        pre[n] = (double) data()[n] / 32768.0;
        pre[FRAME_SIZE-n-1] = (double) data()[M-n-1] / 32768.0;
    }
    
    fftw_execute(prePlan);
    for(quint32 n = 0; n < m_inputSamples*FRAME_SIZE/M/2; n++) {
        decim[n] = decim[n*(FRAME_SIZE/M)];
        decim[m_inputSamples*(FRAME_SIZE/M)-n-1] = decim[FRAME_SIZE-(n*(FRAME_SIZE/M)+1)];
    }

    quint32 x_offset = m_X/2 - m_inputSamples/2;
    memset(decim + m_inputSamples*(FRAME_SIZE/M), 0,
           (FRAME_SIZE-m_inputSamples*(FRAME_SIZE/M))*sizeof(std::complex<double>));
    
    
    fftw_execute_dft(decimPlan, reinterpret_cast<fftw_complex*>(decim),
                     reinterpret_cast<fftw_complex*>(in_w+x_offset));

    for(quint32 n = 0; n < m_X ; n++) {
        in_w[n] /= (double) (m_inputSamples);
    }
    
    qint32 trigger_offset = -1;
    for(quint32 n = x_offset; n < m_X-x_offset+1 ; n++) {
        if(trigger_offset < 0 && abs(in_w[n]) >= trigger_level && arg(in_w[n]) >= 0.0) {
            trigger_offset = n-x_offset;
            break;
        }
    }
    
    if(trigger_offset >= 0 && trigger_offset < (int)m_X-1) {
        memcpy(in_w + x_offset,
               in_w + x_offset + trigger_offset,
               (m_X - trigger_offset)
                    *sizeof(std::complex<double>));
        memset(in_w+m_X - trigger_offset, 0, trigger_offset*sizeof(std::complex<double>));
    } else if(trigger_offset < 0) {
        memset(in_w, 0, m_X*sizeof(std::complex<double>));
    }
    in_r = in;
    in_w = in_r + (m_Y/2-m_scanLines/2)*m_X
                + (((in_w - (in_r + (m_Y/2-m_scanLines/2)*m_X)) + m_X) % (m_scanLines*m_X));

    for(quint32 y = 0; y < m_Y; y++) {
        for(quint32 x = 0; x < m_X; x++) {
            quint32 x_ = (x+m_X/2) % m_X;
            quint32 y_ = (y+m_Y/2) % m_Y;
            out[y*m_X+x] = in[y_*m_X+x_];
        }
    }
    fftw_execute(inPlan);
    
    for(quint32 n = 0; n < m_N ; n++) {
        out[n] /= (double) m_scanLines * (double) m_inputSamples;
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
            setPixelColor(x_, y_, color);
        }
    }
}


void SpectrumScope::postResize() {
    if(m_scanLines == m_Y)
        m_scanLines = rect().height();
    if(m_inputSamples == m_X)
        m_inputSamples = rect().width();
    m_X = rect().width();
    m_Y = rect().height();
    m_N = m_X * m_Y;
    m_scanLines    = qMin(m_scanLines,    m_Y);
    m_inputSamples = qMin(m_inputSamples, m_X);
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
            sat = qBound(0.00, sat+.01, 1.0);
        else if(ev->angleDelta().x() < 0.0)
            sat = qBound(0.00, sat-.01, 1.0);
        QApplication::activeWindow()->setWindowTitle(QString("[Scale: %1 dB] [Sat.: %2]").arg( log(scale)*10).arg(sat));
    } else if(QApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier)) {
        if(ev->angleDelta().y() > 0.0)
            m_scanLines += 1;
        else if(ev->angleDelta().y() < 0.0)
            m_scanLines -= 1;
        m_scanLines = qBound(1U, m_scanLines, m_Y);
        
        if(ev->angleDelta().x() > 0.0)
            m_inputSamples += 1;
        else if(ev->angleDelta().x() < 0.0)
            m_inputSamples -= 1;
        m_inputSamples = qBound(1U, m_inputSamples, m_X);
        
        fft_decim_set();
        setBandwidthTitle();
    } else {
        if(ev->angleDelta().y() > 0.0)
            trigger_level *= 1.05;
        else if(ev->angleDelta().y() < 0.0)
            trigger_level /= 1.05;
        trigger_level = qBound(1e-10, trigger_level, 1e10);
        QApplication::activeWindow()->setWindowTitle(QString("[Trigger: %1 dB]").arg( log(trigger_level)*10.0));
    }
}

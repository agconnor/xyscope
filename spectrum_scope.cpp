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
    m_X = rect().width();
    m_Y = rect().height();
    m_W = qMax(m_X,m_Y);
    m_W += m_W % 2;
    m_N = m_W * m_W;
    
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
     in_r = in;
     in_w = in + (m_W/2-m_scanLines/2)*m_W;
     memset(in, 0, m_N*sizeof(std::complex<double>));
     
 }
 
 void SpectrumScope::fft_dyn_alloc() {
     if(in != NULL)
         fftw_free(in);
     in   = (std::complex<double> *) fftw_alloc_complex(m_N);

     if(out != NULL)
         fftw_free(out);
     out   = (std::complex<double> *) fftw_alloc_complex(m_N);
     
     if(post != NULL)
         fftw_free(post);
     post = (std::complex<double> *) fftw_alloc_complex(m_W);
     fft_decim_set();

     decimPlan = fftw_plan_dft_1d(m_W,
                                  reinterpret_cast<fftw_complex*>(decim),
                                  reinterpret_cast<fftw_complex*>(post),
                                          FFTW_BACKWARD, FFTW_MEASURE);
     
     inPlan = fftw_plan_dft_2d(m_W, m_W,
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
    fftw_free((fftw_complex*)decim);
    fftw_free((fftw_complex*)post);
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

    quint32 x_offset = m_W/2 - m_inputSamples/2;
    memset(decim + m_inputSamples*(FRAME_SIZE/M), 0,
           (FRAME_SIZE-m_inputSamples*(FRAME_SIZE/M))*sizeof(std::complex<double>));
    
    fftw_execute(decimPlan);
    
    for(quint32 m = 0; m < m_W; m++) {
        post[m] /= (double) m_inputSamples;
    }

    qint32 trigger_offset = -1;
    for(quint32 m = 0; m < m_W ; m++) {
        if(trigger_offset < 0 && log10(abs(post[m])) >= trigger_level && arg(post[m]) >= 0.0) {
            trigger_offset = m;
            break;
        }
    }
    
    if(trigger_offset >= 0 && trigger_offset < (int)m_W-1) {
        memcpy(post,
               post + trigger_offset,
               (m_W - trigger_offset)
                    *sizeof(std::complex<double>));
        memset(post+m_W - trigger_offset, 0, trigger_offset*sizeof(std::complex<double>));
    } else if(trigger_offset < 0) {
        memset(post, 0, m_W*sizeof(std::complex<double>));
    }
    
    memset(in_w, 0, m_W*sizeof(std::complex<double>));
    for(quint32 n = x_offset, m = 0;
            n < m_W-x_offset && m < m_W;
            n++,
            (m+=m_W/m_inputSamples)%=m_W) {
        in_w[n] =  post[m];
    }
    in_r = in;
    in_w = in_r + (m_W/2-m_scanLines/2)*m_W
                + (((in_w - (in_r + (m_W/2-m_scanLines/2)*m_W)) + m_W) % (m_scanLines*m_W));

    for(quint32 y = 0; y < m_W; y++) {
        for(quint32 x = 0; x < m_W; x++) {
            quint32 x_ = (x+m_W/2) % m_W;
            quint32 y_ = (y+m_W/2) % m_W;
            out[y*m_W+x] = in[y_*m_W+x_];
        }
    }
    fftw_execute(inPlan)    ;
    
    for(quint32 n = 0; n < m_N ; n++) {
        out[n] /= (double) m_scanLines * (double) m_inputSamples;
    }
    
    for(quint32 x = 0; x < m_X ; x++) {
        for(quint32 y = 0; y < m_Y ; y++) {
            int x_plane = (x * qMax(1U,m_W/m_X)) % m_W;
            int y_plane = (y * qMax(1U,m_W/m_Y)) % m_W;
            int x_ = (x+m_X/2) % m_X;
            int y_ = (y+m_Y/2) % m_Y;
            double mag = log10(abs(out[y_plane*m_W+x_plane])) + scale;
            
            QColor color;
            color.setHsvF((arg(out[y_plane*m_W+x_plane])+M_PI)/(2*M_PI),
                    1.0 - qBound(0.0,
                                 qMax((mag-1.0)*sat,0.0),
                                 1.0),
                          qBound(0.0, mag, 1.0));
            setPixelColor(x_, y_, color);
        }
    }
}


void SpectrumScope::postResize() {
    if(m_X != (quint32) rect().width())
    {
        m_inputSamples = qBound(1U, m_inputSamples + (m_X - rect().width()), m_W);
        setBandwidthTitle();
    }
    m_X = rect().width();
    m_Y = rect().height();
}

void SpectrumScope::wheelEvent(QWheelEvent *ev)
{
    if(QApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier)) {
        if(ev->angleDelta().y() > 0.0)
            scale += .01;
        else if(ev->angleDelta().y() < 0.0)
            scale -= .01;
        
        if(ev->angleDelta().x() > 0.0)
            sat = qBound(0.00, sat+.01, 1.0);
        else if(ev->angleDelta().x() < 0.0)
            sat = qBound(0.00, sat-.01, 1.0);
        QApplication::activeWindow()->setWindowTitle(QString("[Scale: %1 dB] [Sat.: %2]").arg(scale*10).arg(sat));
    } else if(QApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier)) {
        if(ev->angleDelta().y() > 0.0)
            m_scanLines += 1;
        else if(ev->angleDelta().y() < 0.0)
            m_scanLines -= 1;
        m_scanLines = qBound(1U, m_scanLines, m_W);
        
        if(ev->angleDelta().x() > 0.0)
            m_inputSamples += 1;
        else if(ev->angleDelta().x() < 0.0)
            m_inputSamples -= 1;
        m_inputSamples = qBound(1U, m_inputSamples, m_W);
        fft_decim_set();
        setBandwidthTitle();
    } else {
        if(ev->angleDelta().y() > 0.0)
            trigger_level += .01;
        else if(ev->angleDelta().y() < 0.0)
            trigger_level -= .01;
        QApplication::activeWindow()->setWindowTitle(QString("[Trigger: %1 dB]").arg(trigger_level*10));
    }
}

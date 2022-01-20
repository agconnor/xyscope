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
    
    m_X = 200;
    m_Y = 200;
    m_N = m_X * m_Y;
    
    in   = new std::complex<double>[m_N*2];
    out   = new std::complex<double>[m_N];
    in_w = in;
    in_r = &in[m_N];
    
    m_image = QImage(m_X, m_Y, QImage::Format_RGB16);
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
    double pre[M];
    std::complex<double> in_[M];
    for(int32_t n = 0; n < M ; n++) {
        pre[n] = (double) m_data[n] / 32768.0;
    }
    m_dataMutex->unlock();
    inPlan = fftw_plan_dft_r2c_1d(M,  pre,
                                  reinterpret_cast<fftw_complex*>(in_), FFTW_ESTIMATE);

    fftw_execute(inPlan);
    
    std::complex<double> decim[m_X];
    for(quint32 n = 0; n < m_X; n++) {
        decim[n] = in_[(n*m_X/M)%m_X] * (double)M/(double) m_X;
    }
    fftw_destroy_plan(inPlan);
    in_r = in;
    in_w = in + (((in_w - in) + m_X) % (2*N));
//    in_r = in + (((in_r - in) + m_X) % (2*N));
    inPlan = fftw_plan_dft_1d(m_X, reinterpret_cast<fftw_complex*>(decim),
                              reinterpret_cast<fftw_complex*>(in_w), FFTW_BACKWARD, FFTW_ESTIMATE);
    
    fftw_execute(inPlan);
    fftw_destroy_plan(inPlan);
    
//    for(qint32 k = 0; k < M/FRAME_SPAN; k++) {
//        for(qint32 m = 0; m < FRAME_SPAN; m++) {
//            qint32 start = (m_maxX - M/FRAME_SPAN)/2*FRAME_SPAN+(m_maxY-FRAME_SPAN)/2;
//            in_w[start + k*FRAME_SPAN + m] = (qreal) m_data[m] / 32768.0 * (double) M;
//        }
//    }
//
//    if((in_w - in_r) >= N+M || (in_w - in_r) <= M) {
//    in_r = in + (((in_r - in) + M) % (2*N));
//    }
    
    
//    if(m_valMutex->tryLock()) {
        inPlan = fftw_plan_dft_2d(m_X, m_Y,  reinterpret_cast<fftw_complex*>(in_r), reinterpret_cast<fftw_complex*>(out), FFTW_FORWARD, FFTW_ESTIMATE);

        fftw_execute(inPlan);
        
        #pragma omp parallel
        #pragma omp for
        for(quint32 n = 0; n < N ; n++) {
            out[n] /= (double) N;
        }
        
        fftw_destroy_plan(inPlan);
 
        qreal scale = 4.0, shamt = 3.0;
        #pragma omp parallel
        #pragma omp for
        for(int x = 0; x < m_maxX ; x++) {
            for(int y = 0; y < m_maxY ; y++) {
                double mag = abs(out[x*m_maxY+y]);
//                int x_ = (x+m_X/2) % m_X;
//                int y_ = (y+m_Y/2) % m_Y;
                QColor color;
                color.setRed(qMin(255.0, //exp(-pow(lp/scale,2.0))*256.0/(sqrt(M_PI)));
                                      qMax(0.0,(1.0-pow(mag/scale, 2.0)))*256.0));
                color.setGreen(qMin(255.0, //exp(-pow((lp-shamt)/scale,2.0))*256.0/(sqrt(M_PI)));
                                      qMax(0.0,(1.0-pow((mag-shamt)/scale, 2.0)))*256.0));
                color.setBlue(qMin(255.0, //exp(-pow((lp-2.0*shamt)/scale,2.0))*256.0/(sqrt(M_PI)));
                                      qMax(0.0,(1.0-pow((mag-2.0*shamt)/scale, 2.0)))*256.0));
                m_image.setPixelColor(x, y, color);
//                QColor color;
//                color.setHsvF((atan2(i, q)+M_PI)/(2*M_PI), 1.0, qMin(1.0, mag));
//                val[0][x_][y_] = color.red();
//                val[1][x_][y_] = color.green();
//                val[2][x_][y_] = color.blue();
            }
        }
//        m_valMutex->unlock();

}
void SpectrumView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    
//    if(valMutex()->tryLock()) {
        painter.setPen(Qt::NoPen);
        
        painter.drawImage(painter.viewport(), m_image, m_image.rect());
//        valMutex()->unlock();
//    }

}
void SpectrumView::resizeEvent(QResizeEvent *) {
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
    delete[] in;
    in = new std::complex<double>[m_N*2];
    in_r = &in[m_N];
    in_w = in;
    delete[] out;
    out = new std::complex<double>[m_N];
    m_image = QImage(maxX, maxY, QImage::Format_RGB16);
    m_valMutex->unlock();
}

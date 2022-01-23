//
//  spectrum_view.hpp
//  xyscope
//
//  Created by )\( on 1/18/22.
//

#ifndef spectrum_view_hpp
#define spectrum_view_hpp

#include <complex>
#include <qmath.h>
#include <fftw3.h>
#include "raster_image.hpp"

class SpectrumScope : public RasterImage {
public:
    explicit SpectrumScope(QWidget * parent);
    ~SpectrumScope();
    
    void postResize();
    void wheelEvent(QWheelEvent *ev) override;
protected:
    void refreshImpl() override;
    
private:
    
    std::complex<double> *decim = NULL, *in = NULL, *out = NULL;
    std::complex<double> *in_w = NULL, *in_r = NULL;
    double *pre;
    
    fftw_plan prePlan, decimPlan, inPlan;
    
    quint32 m_X = 0;
    quint32 m_Y = 0;
    quint32 m_N = 0;
    
    qreal scale = 1.0;
    qreal sat = 0.5;
    qreal trigger_level = 0.2;
    quint32 m_decimFactorV = 4;
    quint32 m_decimFactorH = 2;
    
    void fft_dyn_alloc();
    void fft_decim_set();
    void setBandwidthTitle() {
        QApplication::activeWindow()
          ->setWindowTitle(
          QString().asprintf(
            "[∆ƒ (H): %'d Hz] [∆T (V): %'d ms]",
              (int) ((double) m_X *
                ((double)SAMPLE_RATE/
                        (double)FRAME_SIZE/
                        (double) m_decimFactorH)),
              (int) ((double) m_Y *
                (double)FRAME_SIZE/48.0/(double)m_decimFactorV)));
    }
};
#endif /* spectrum_view_hpp */

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
    
    std::complex<double> *pre = NULL, *decim = NULL, *in = NULL, *out = NULL;
    std::complex<double> *in_w = NULL, *in_r = NULL;
    
    fftw_plan prePlan, decimPlan, inPlan;
    
    quint32 m_X = 0;
    quint32 m_Y = 0;
    quint32 m_N = 0;
    
    qreal scale = 1.0;
    qreal sat = 0.5;
    qreal trigger_level = 0.1;
    quint32 m_scanLines = INIT_SIZE/PIXEL_SCALE;
    quint32 m_decimFactorH = 4;
    
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
              (int) ((double) m_scanLines *
                (double)FRAME_SIZE
                     /((double)SAMPLE_RATE / 1000.0))));
    }
};
#endif /* spectrum_view_hpp */

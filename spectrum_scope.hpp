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
    
    void resizeEvent(QResizeEvent *) override;
    void wheelEvent(QWheelEvent *ev) override;
protected:
    void refreshImpl() override;
private:
    
    int m_doRefresh = 0;
    std::complex<double> *in, *out;
    std::complex<double> *in_w = NULL, *in_r = NULL;
    fftw_plan inPlan;
    
    quint32 m_X = 0;
    quint32 m_Y = 0;
    quint32 m_N = 0;
    quint32 m_K = 1;
    
    qreal scale = 1.0;
    qreal sat = 0.05;
};
#endif /* spectrum_view_hpp */

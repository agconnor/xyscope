//
//  hilbert_scatter_view.hpp
//  xyscope
//
//  Created by )\( on 1/18/22.
//

#ifndef hilbert_scatter_view_hpp
#define hilbert_scatter_view_hpp

#include "raster_image.hpp"
#include <complex>
#include <qmath.h>
#include <fftw3.h>



class AnalyticScope : public RasterImage
{

public:
    explicit AnalyticScope(QWidget *parent);
    ~AnalyticScope();

    
protected:
    void wheelEvent(QWheelEvent *ev) override;
    void refreshImpl() override;
private:
    qreal m_level = 0;
    qreal m_scale = 1.0;
    qreal m_redDecay = .6667;
    qreal m_blueDecay = .75;
    int m_greenDecay = 4;
    
    int m_doRefresh = 0;
    std::complex<double> *in;
    fftw_plan inPlan, outPlan;
    
    qreal trigger_level = 0.0;
};

#endif /* hilbert_scatter_view_hpp */

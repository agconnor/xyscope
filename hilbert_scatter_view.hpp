//
//  hilbert_scatter_view.hpp
//  xyscope
//
//  Created by )\( on 1/18/22.
//

#ifndef hilbert_scatter_view_hpp
#define hilbert_scatter_view_hpp

#include "raster_view.hpp"
#include <qmath.h>
#include <fftw3.h>


class HilbertScatterView : public RasterView
{
    Q_OBJECT

public:
    explicit HilbertScatterView(QWidget *parent);
    virtual QMutex * & dataMutex() override {return m_dataMutex;}
    virtual QMutex * & valMutex() override {return m_valMutex;}
    void refresh() override;
protected:
    virtual void wheelEvent(QWheelEvent *ev) override;
    virtual qint16 * & data() override {return m_data;}
    virtual qint64 & dataLen() override {return m_dataLen;}
    virtual val_array_t * & val() override {return m_val;}
    
    
private:
    qreal m_level = 0;
    qreal m_scale = 1.0;
    qreal m_redDecay = .6667;
    qreal m_blueDecay = .75;
    int m_greenDecay = 16;

    QPixmap m_pixmap;
    QMutex * m_valMutex;
    QMutex * m_dataMutex;
    
    int m_doRefresh = 0;
    val_array_t * m_val = NULL;
    fftw_complex *in;
    fftw_plan inPlan;
    qint16 * m_data;
    qint64 m_dataLen;
};

#endif /* hilbert_scatter_view_hpp */

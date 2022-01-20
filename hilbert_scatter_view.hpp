//
//  hilbert_scatter_view.hpp
//  xyscope
//
//  Created by )\( on 1/18/22.
//

#ifndef hilbert_scatter_view_hpp
#define hilbert_scatter_view_hpp

#include "raster_view.hpp"
#include <complex>
#include <fftw3.h>
#include <qmath.h>


#include <QColorTransform>


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
    virtual void paintEvent(QPaintEvent*) override;
    virtual void resizeEvent(QResizeEvent *) override;
    virtual qint16 * & data() override {return m_data;}
    virtual qint64 & dataLen() override {return m_dataLen;}
    virtual val_array_t * & val() override {return m_val;}
    
    
private:
    qreal m_level = 0;
    qreal m_scale = 1.0;
    qreal m_redDecay = .6667;
    qreal m_blueDecay = .75;
    int m_greenDecay = 4;

    QPixmap m_pixmap;
    QImage m_image;
    QMutex * m_valMutex;
    QMutex * m_dataMutex;
    
    int m_doRefresh = 0;
    val_array_t * m_val = NULL;
    std::complex<double> *in;
    fftw_plan inPlan, outPlan;
    qint16 * m_data;
    qint64 m_dataLen;
};

#endif /* hilbert_scatter_view_hpp */

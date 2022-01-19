//
//  spectrum_view.hpp
//  xyscope
//
//  Created by )\( on 1/18/22.
//

#ifndef spectrum_view_hpp
#define spectrum_view_hpp

#include <qmath.h>
#include <fftw3.h>
#include "raster_view.hpp"

class SpectrumView : public RasterView {
    Q_OBJECT
public:
    explicit SpectrumView(QWidget *parent);
    virtual QMutex * & dataMutex() override {return m_dataMutex;}
    virtual QMutex * & valMutex() override {return m_valMutex;}
    void refresh() override;
protected:
    virtual void wheelEvent(QWheelEvent *) override {};
    virtual qint16 * & data() override {return m_data;}
    virtual qint64 & dataLen() override {return m_dataLen;}
    virtual val_array_t * & val() override {return m_val;}
    virtual void resizeEvent(QResizeEvent *) override;
    
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
    fftw_complex *in, *out;
    fftw_complex *in_w = NULL, *in_r = NULL;
    fftw_plan inPlan;
    qint16 * m_data;
    
    qint64 m_dataLen;
    quint32 m_X = 0;
    quint32 m_Y = 0;
    quint32 m_N = 0;
};
#endif /* spectrum_view_hpp */

//
//  scatter_view.hpp
//  xyscope
//
//  Created by )\( on 1/18/22.
//

#ifndef raster_view_hpp
#define raster_view_hpp

#include <QWidget>
#include <QTypeInfo>
#include <QMutex>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QImage>

#include "raster_image.hpp"

class RasterView : public QWidget
{
    Q_OBJECT
public:
    explicit RasterView(QWidget *parent) : QWidget(parent)
    {
        setBackgroundRole(QPalette::Base);
        setPalette(QPalette(QPalette::Window, Qt::black));
        setAutoFillBackground(true);
    }
    ~RasterView() {delete m_image;}
    void setData(qint16 * _data, qint64 len) {
        RasterImage * rim = (RasterImage *)m_image;
        rim->refresh(_data, len);
    }
    QImage * & image() {return m_image;}
    
public slots:
    virtual void postResize();
protected:
    virtual void paintEvent(QPaintEvent *) override;
    virtual void wheelEvent(QWheelEvent *ev) override {
        RasterImage * rim = (RasterImage *)m_image;
        if(rim->running()) {
            rim->stop();
            rim->wheelEvent(ev);
            rim->start();
        }
    }
private:
    QImage * m_image;
};



#endif /* raster_view_hpp */

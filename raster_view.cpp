//
//  raster_view.cpp
//  xyscope
//
//  Created by )\( on 1/18/22.
//

#include <QPainter>
#include <QRect>

#include "raster_view.hpp"

void RasterView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    
    painter.setPen(Qt::NoPen);
    painter.drawImage(painter.viewport(), *m_image, m_image->rect());
}

void RasterView::postResize() {
    size_t maxX = rect().width()/PIXEL_SCALE;
    size_t maxY = rect().height()/PIXEL_SCALE;
    RasterImage * rim = (RasterImage *) m_image;
    if(rim->running()) {
        rim->stop();
        (*m_image) = m_image->scaled(maxX, maxY);
        ((RasterImage *) m_image)->postResize();
        rim->start();
    }
}


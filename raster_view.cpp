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

void RasterView::resizeEvent(QResizeEvent *e) {
    size_t maxX = rect().width()/3;
    size_t maxY = rect().height()/3;
    (*m_image) = m_image->scaled(maxX, maxY);
    ((RasterImage *) m_image)->resizeEvent(e);
}


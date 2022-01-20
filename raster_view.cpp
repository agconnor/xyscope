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

    painter.setPen(Qt::black);
    painter.drawRect(QRect(painter.viewport().left(),
                           painter.viewport().top(),
                           painter.viewport().right(),
                           painter.viewport().bottom()));
    if(valMutex()->tryLock() && val() != NULL) {
        if(val() == NULL) {
            valMutex()->unlock();
            return;
        }
        val_array_t &v = *(val());
        painter.setPen(Qt::NoPen);
        for(quint64 r = 0; r < v.shape()[1]; r++) {
            for (quint64 c = 0; c < v.shape()[2]; c++) {
              painter.setBrush(QColor(v[0][r][c], v[1][r][c], v[2][r][c]));
              painter.drawRect(QRect(r*3, c*3, 3, 3));
          }
        }
        valMutex()->unlock();
    }
}

void RasterView::resizeEvent(QResizeEvent *) {
    valMutex()->lock();
    size_t maxX = rect().width()/3;
    size_t maxY = rect().height()/3;
    if(val() != NULL) {
        boost::array<size_t, 3> new_ext = {3, maxX, maxY};
        val()->resize(new_ext);
    }
    
    valMutex()->unlock();
}


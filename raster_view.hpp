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
#include <boost/multi_array.hpp>

#define FRAME_SPAN 64
#define FRAME_SIZE 8192

typedef boost::multi_array<quint16, 3> val_array_t;
typedef val_array_t::index val_idx_t;

class RasterView : public QWidget
{
    Q_OBJECT
public:
    explicit RasterView(QWidget *parent) : QWidget(parent)
    {
        
    }
    
    void setData(qint16 * _data, qint64 len) {
        memcpy(data(), _data, len*sizeof(qint16));
        dataLen() = len;
        refresh();
        
    }
    virtual QMutex * & dataMutex() = 0;
    virtual QMutex * & valMutex() = 0;
    virtual void refresh() = 0;
protected:
    virtual qint16 * & data() = 0;
    virtual qint64 & dataLen() = 0;
    virtual val_array_t * & val() = 0;
    virtual void resizeEvent(QResizeEvent *);
    virtual void paintEvent(QPaintEvent *);
};



#endif /* raster_view_hpp */

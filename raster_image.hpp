//
//  raster_image.hpp
//  xyscope
//
//  Created by )\( on 1/20/22.
//

#ifndef raster_image_hpp
#define raster_image_hpp

#include <QImage>
#include <QMutex>
#include <QWidget>
#include <QWheelEvent>
#include <QResizeEvent>

#define FRAME_SPAN 64
#define FRAME_SIZE 4096
#define PIXEL_SCALE 2
#define INIT_SIZE 600

class RasterImage : public QImage {
    
public:
    explicit RasterImage(QWidget * parent) : QImage(INIT_SIZE/PIXEL_SCALE,  //parent->rect().width(),
               INIT_SIZE/PIXEL_SCALE, //parent->rect().height(),
               QImage::Format_RGB888){
        m_mutex = new QMutex();
        m_len = FRAME_SIZE;
        m_data = new qint16[m_len];
    }
    
    ~RasterImage() {
        delete m_mutex;
        delete [] m_data;
    }
    qint16 * & data()  {return m_data;}
    quint32 & len()    {return m_len;}
    void start() {
        if(m_mutex->tryLock()) {
            m_running = 1;
            m_mutex->unlock();
        }
        
    }
    void stop()  {
        m_mutex->lock();
        m_running = 0;
        m_mutex->unlock();
        
    }
    void refresh(qint16 * _data, quint32 len) {
        if(m_running) {
            if(m_mutex->tryLock()) {
                memcpy(m_data, _data, len*sizeof(qint16));
                m_len = len;
                refreshImpl();
                m_mutex->unlock();
            }
        }
    }
    
    bool running() {return m_running;}
    
    virtual void wheelEvent(QWheelEvent *ev) {}
    virtual void resizeEvent(QResizeEvent *) {}
protected:
    virtual void refreshImpl() = 0;
private:
    QMutex * m_mutex;
    qint16 * m_data;
    quint32 m_len;
    bool m_running = 0;
};

#endif /* raster_image_hpp */

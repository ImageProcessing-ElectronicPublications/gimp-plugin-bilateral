#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

//#include "stm_export.h"

namespace spectral
{

// Generic image template.
template <typename T>
class image
{
public:
    image(uint32_t width,
          uint32_t height,
          uint32_t channels = 1,
          const T *buf = NULL)
        : m_buffer(NULL)
        , m_width(width)
        , m_height(height)
        , m_channels(channels)
    {
        uint32_t size = width * height * channels;

        if(size)
        {
            m_buffer = new T[size];

            if(buf && m_buffer)
            {
                for(uint32_t i=0; i<size; i++)
                {
                    m_buffer[i] = buf[i];
                }
            }
            else if(m_buffer)
            {
                for(uint32_t i=0; i<size; i++)
                {
                    m_buffer[i] = 0;
                }
            }
        }
    }

    virtual ~image()
    {
        if(m_buffer)
        {
            delete [] m_buffer;
        }
    }

    size_t get_index(uint32_t x, uint32_t y) const
    {
        if((x < m_width) && (y < m_height))
        {
            uint32_t idx = x + (y * m_width);
            return (size_t)(idx * m_channels);
        }
        return (size_t)(-1);
    }

    const T *get_pixel(uint32_t x, uint32_t y) const
    {
        T *result = (T *)NULL;
        size_t index = get_index(x, y);

        if(m_buffer)
        {
            result = &(m_buffer[index]);
        }

        return result;
    }

    const T *get_constrained_pixel(uint32_t x, uint32_t y,
                                   bool wrap_x = false,
                                   bool wrap_y = false) const
    {
        int xx = (int)x;
        int yy = (int)y;

        if(wrap_x)
        {
            xx = constrain_wrap(xx, m_width);
        }
        else
        {
            xx = constrain_reflect(xx, m_width);
        }
        if(wrap_y)
        {
            yy = constrain_wrap(yy, m_height);
        }
        else
        {
            yy = constrain_reflect(yy, m_height);
        }

        return get_pixel(xx, yy);
    }

    T*       get_buffer(void) const
    {
        return m_buffer;
    }
    uint32_t get_width(void)  const
    {
        return m_width;
    }
    uint32_t get_height(void) const
    {
        return m_height;
    }
    uint32_t get_channels(void) const
    {
        return m_channels;
    }

    void set_pixel(uint32_t x, uint32_t y, const T *pixel)
    {
        size_t index = get_index(x, y);
        if(pixel && m_buffer && (index < (m_width * m_height * m_channels)))
        {
            for(size_t i=0; i<(size_t)m_channels; i++)
            {
                m_buffer[i + index] = pixel[i];
            }
        }
    }
private:

    int constrain_reflect(int x, uint32_t extent) const
    {
        extent--;

        if(extent)
        {
            x = abs(x);

            while(uint32_t(x) > extent)
            {
                x = abs((int)(2 * extent) - (int)x);
            }
        }
        return x;
    }

    int constrain_wrap(int x, uint32_t extent) const
    {
        if(extent)
        {
            while(x < 0)
            {
                x+= extent;
            }
            while(x >= (int32_t)extent)
            {
                x-= extent;
            }
        }
        return x;
    }

    T *m_buffer;
    uint32_t m_width, m_height, m_channels;
};

// 8 bit image.
class Image : public image<uint8_t>
{
public:
    Image(uint32_t width, uint32_t height, uint32_t channels, uint8_t *buffer = NULL);
    Image(const Image &);
    virtual ~Image();

    // Expand an image, adding a reflected border.
    Image *Expand(uint32_t x_border, uint32_t y_border,
                  bool wrap_x, bool wrap_y) const;

    Image *Contract(uint32_t x_border, uint32_t y_border) const;

private:
};

class IntegralHistogram : public image<uint32_t>
{
public:
    IntegralHistogram(uint32_t bins, const Image &img, uint32_t channel);

    IntegralHistogram(uint32_t bins, const Image &img, uint32_t x0, uint32_t y0,
                      uint32_t width, uint32_t height, uint32_t channel);


    virtual ~IntegralHistogram();

    void GetHistogram(uint32_t x1, uint32_t y1,
                      uint32_t x2, uint32_t y2,
                      uint32_t *result) const;
private:
    void BuildHistogram(uint32_t bins, const Image &img,
                        uint32_t x0, uint32_t y0,
                        uint32_t width, uint32_t height,
                        uint32_t channel);

};

}

#endif


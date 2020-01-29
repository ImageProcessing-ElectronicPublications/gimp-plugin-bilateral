#include <math.h>
#include "image.h"

namespace spectral
{

Image::Image(uint32_t width,
             uint32_t height,
             uint32_t channels,
             uint8_t *buffer)
    : image<uint8_t>(width, height, channels, buffer)
{
}

Image::Image(const Image &other)
    : image<uint8_t>(other.get_width(),
                     other.get_height(),
                     other.get_channels(),
                     other.get_buffer())
{
}

Image::~Image()
{
}

// Expand an image, adding a reflected border.
Image *
Image::Expand(uint32_t x_border, uint32_t y_border,
              bool wrap_x, bool wrap_y) const
{
    Image *result(NULL);

    uint32_t new_width  = get_width() + (x_border * 2);
    uint32_t new_height = get_height()+ (y_border * 2);
    result = new Image(new_width, new_height, get_channels());

    if(result)
    {
        for (int32_t y=0; uint32_t(y)<new_height; y++)
        {
            int32_t yy;
            yy = int32_t(y) - int32_t(y_border);
            for(int32_t x=0; uint32_t(x)<new_width; x++)
            {
                int32_t xx;
                xx = int32_t(x) - int32_t(x_border);
                result->set_pixel(x,y,get_constrained_pixel(xx,yy,
                                  wrap_x,wrap_y));
            }
        }
    }

    return result;
}

Image *
Image::Contract(uint32_t x_border, uint32_t y_border) const
{
    Image *result(NULL);

    if((get_height() > (y_border * 2) &&
            (get_width() > (x_border * 2))))
    {
        uint32_t new_width  = get_width() - (x_border * 2);
        uint32_t new_height = get_height()- (y_border * 2);
        result = new Image(new_width, new_height, get_channels());

        if(result)
        {
            for (int32_t y=0; uint32_t(y)<new_height; y++)
            {
                int32_t yy;
                yy = y + y_border;
                for(int32_t x=0; uint32_t(x)<new_width; x++)
                {
                    int32_t xx;
                    xx = x + x_border;
                    result->set_pixel(x,y,get_constrained_pixel(xx,yy));
                }
            }
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////
IntegralHistogram::IntegralHistogram(uint32_t bins, const Image &img, uint32_t channel)
    : image<uint32_t>(img.get_width(), img.get_height(), bins)
{
    BuildHistogram(bins, img, 0, 0, img.get_width(), img.get_height(), channel);
}

IntegralHistogram::IntegralHistogram(uint32_t bins, const Image &img,
                                     uint32_t x0, uint32_t y0,
                                     uint32_t width, uint32_t height,
                                     uint32_t channel)
    : image<uint32_t>(width, height, bins)
{
    BuildHistogram(bins, img, x0, y0, width, height, channel);
}

IntegralHistogram::~IntegralHistogram()
{
}

void
IntegralHistogram::BuildHistogram(uint32_t bins, const Image &img,
                                  uint32_t x0, uint32_t y0,
                                  uint32_t width, uint32_t height,
                                  uint32_t channel)
{
    uint32_t x,y;
    uint32_t index(0), in_index(0);
    uint32_t shift(0);

    {
        uint32_t tmp(256);

        while(tmp > bins)
        {
            tmp>>=1;
            shift++;
        }
    }

    for(y=0; y<get_height(); y++)
    {
        {
            uint32_t yy = y0 + y;
            in_index = (((yy * img.get_width()) + x0) * img.get_channels()) + channel;
        }

        for(x=0; x<get_width(); x++)
        {
            uint32_t bin;

            // Add 1 to this bin if we are in range.
            if((x < width) && (y < height))
            {
                bin = (img.get_buffer()[in_index]) >> shift;

                if(bin < bins)
                {
                    get_buffer()[index + bin] = 1;
                }
            }

            in_index+= img.get_channels();

            if(x)
            {
                // Add previous pixel in x
                const uint32_t *pixel;
                uint32_t i;
                pixel = get_pixel(x-1, y);
                for(i=0; i<bins; i++)
                {
                    get_buffer()[index + i]+= pixel[i];
                }
            }
            if(y)
            {
                // Add previous pixel in y
                const uint32_t *pixel;
                uint32_t i;

                pixel = get_pixel(x, y-1);
                for(i=0; i<bins; i++)
                {
                    get_buffer()[index + i]+= pixel[i];
                }
            }
            if(x && y)
            {
                // subtract previous pixel in x,y
                const uint32_t *pixel;
                uint32_t i;

                pixel = get_pixel(x-1, y-1);
                for(i=0; i<bins; i++)
                {
                    get_buffer()[index + i]-= pixel[i];
                }
            }

            index+= bins;
        }
    }
}

void
IntegralHistogram::GetHistogram(uint32_t x1, uint32_t y1,
                                uint32_t x2, uint32_t y2,
                                uint32_t *result) const
{
    uint32_t i;

    if(x1 > x2)
    {
        uint32_t tmp = x1;
        x1 = x2;
        x2 = tmp;
    }
    if(y1 > y2)
    {
        uint32_t tmp = y1;
        y1 = y2;
        y2 = tmp;
    }

    for(i=0; i<get_channels(); i++)
    {
        result[i] = 0;
    }

    if((x1 < get_width()) && (y1 < get_height()))
    {
        const uint32_t *a, *b, *c, *d;
        //
        //  a-----b
        //  |     |
        //  d-----c
        //
        // We want to get inclusive points, so we move the top left up,
        // potentially making a, b, c invalid in the process.

        a = b = c = d = NULL;

        if(x2 >= get_width())
        {
            x2 = get_width() - 1;
        }
        if(y2 >= get_height())
        {
            y2 = get_height() - 1;
        }

        c = get_pixel(x2, y2);

        if(x1 && y1)
        {
            a = get_pixel(x1 - 1, y1 - 1);
        }
        if(x1)
        {
            d = get_pixel(x1 - 1, y2);
        }
        if(y1)
        {
            b = get_pixel(x2, y1 - 1);
        }

        // calculate histogram.
        // TODO: optimise
        for(i=0; i<get_channels(); i++)
        {
            if(a) result[i]+=a[i];
            if(c) result[i]+=c[i];
            if(b) result[i]-=b[i];
            if(d) result[i]-=d[i];
        }
    }
}

}

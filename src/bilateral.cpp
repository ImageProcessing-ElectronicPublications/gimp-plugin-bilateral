#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "bilateral.h"
#include "image.h"

#include "settings.h"

#define FILTER_JOB_SHARE 0.75
#define ENHANCE_JOB_SHARE 0.25
// Filter context contains precalculated weights and mean values for various
// bins and bin/offset combinations.
typedef struct _filter_context
{
    uint32_t bin_map[256], offset_map[256];
    float bin_value[NUM_BINS];
    float offset_weights[NUM_BINS][BIN_SIZE];
} filter_context;

// Integrals of filter weights as a function of distance from centre.
static float linear_integral_fun(float threshold, float x)
{
    float integral = x - ((x * x) / (2.0 * threshold));
    return integral;
}

static float quadratic_integral_fun(float threshold, float x)
{
    float integral = x - ((x * x * x) / (3.0 * threshold * threshold));
    return integral;
}

// Initialisation of various lookup tables for bin average intensity values,
// bin weights for various initial offsets etc.
void initialise_filter_context(uint32_t threshold,
                               bool quadratic,
                               filter_context &ctx)
{
    float (*get_integral)(float, float)(NULL);
    float ft;
    ft = float(threshold)/256.0;

    if(quadratic)
    {
        get_integral = quadratic_integral_fun;
    }
    else
    {
        get_integral = linear_integral_fun;
    }

    for(uint32_t i=0; i<256; i++)
    {
        ctx.bin_map[i] = div(i,BIN_SIZE).quot;
        ctx.offset_map[i] = div(i,BIN_SIZE).rem;
    }

    for(uint32_t i=0; i<NUM_BINS; i++)
    {
        ctx.bin_value[i] = ((i * BIN_SIZE) + ((i+1) * BIN_SIZE))/2.0;

        for(uint32_t j=0; j<BIN_SIZE; j++)
        {
            uint32_t near_dist, far_dist;

            near_dist = j + (i * BIN_SIZE);
            far_dist = near_dist + BIN_SIZE;

            // Spot the edge of the filter kernel.
            if(far_dist <= threshold)
            {
                float near(near_dist), far(far_dist);
                near/= 256;
                far/= 256;
                ctx.offset_weights[i][j] = get_integral(ft, far) - get_integral(ft, near);
            }
            else if (near_dist <= threshold)
            {
                float near(near_dist), far(threshold);
                near/= 256;
                far/= 256;
                ctx.offset_weights[i][j] = get_integral(ft, far) - get_integral(ft, near);
            }
            else
            {
                ctx.offset_weights[i][j] = -1;
            }
        }
    }
}

spectral::Image *rgn_to_image(GimpPixelRgn &rgn_in, uint32_t width, uint32_t height, uint32_t channels)
{
    guchar *row;
    row = g_new(guchar, channels * width);
    spectral::Image *result(NULL);
    {
        uint32_t index(0), y(0);

        result = new spectral::Image(width, height, channels);

        if(result)
        {
            for(y=0; y<height; y++)
            {
                gimp_pixel_rgn_get_row(&rgn_in, row, 0, y, width);
                memcpy((void *)(result->get_buffer()+index), (void *)row, width * channels);
                index+=(width * channels);
            }
        }
    }
    g_free(row);
    return result;
}

void write_image_to_rgn(spectral::Image *img, GimpPixelRgn &rgn_out)
{
    guchar *row;
    row = g_new(guchar, img->get_channels() * img->get_width());
    {
        uint32_t index(0), y(0);

        if(img)
        {
            for(y=0; y<img->get_height(); y++)
            {
                memcpy((void *)row, (void *)(img->get_buffer()+index),
                       img->get_width() * img->get_channels());
                gimp_pixel_rgn_set_row(&rgn_out, row, 0, y, img->get_width());
                index+=(img->get_width() * img->get_channels());
            }
        }
    }
    g_free(row);
}

void filter_tile(const spectral::IntegralHistogram *hist,
                 const filter_context &ctx,
                 uint32_t radius,
                 uint32_t x_offset,
                 uint32_t y_offset,
                 uint32_t width,
                 uint32_t height,
                 uint32_t channel,
                 double min_progress,
                 double max_progress,
                 spectral::Image *dest)
{
    uint32_t channels;

    channels = dest->get_channels();

    if((width + x_offset) > dest->get_width())
    {
        width = dest->get_width() - x_offset;
    }
    if((height + y_offset) > dest->get_height())
    {
        height = dest->get_height() - y_offset;
    }

    if(hist && dest)
    {
        for(uint32_t y=0; y<height; y++)
        {
            double progress;
            uint32_t ymax, row_index;
            uint8_t *row;

            row_index = channel;
            ymax = y + (radius * 2);// - 1;
            {
                uint32_t offset;
                offset  = (x_offset + ((y + y_offset) * dest->get_width()))*channels;
                row = dest->get_buffer() + offset;
            }

            for(uint32_t x=0; x<width; x++)
            {
                uint32_t bins[NUM_BINS];
                uint32_t xmax;

                uint32_t cur_val, cur_bin, offset, value;
                float total_value, total_weight;
                xmax = x + (radius * 2);

                // Get a histogram for the sub region.

                hist->GetHistogram(x, y, xmax, ymax, bins);

                cur_val = row[row_index];

                cur_bin = ctx.bin_map[cur_val];
                offset = ctx.offset_map[cur_val];

                total_weight = BIN_SIZE/256;
                total_value = total_weight * (float)cur_val;

                // work up.
                {
                    uint32_t dir_offset(BIN_SIZE - offset);
                    uint32_t this_bin(cur_bin);
                    uint32_t bin_counter(0);

                    while((this_bin < NUM_BINS) &&
                            (ctx.offset_weights[bin_counter][dir_offset] >= 0))
                    {
                        float this_weight, this_value;
                        this_weight = ctx.offset_weights[bin_counter][dir_offset] * bins[this_bin];
                        this_value = this_weight * ctx.bin_value[this_bin];

                        total_weight+= this_weight;
                        total_value+= this_value;
                        this_bin++;
                        bin_counter++;
                    }
                }
                // work down.
                {
                    uint32_t dir_offset(offset);
                    uint32_t this_bin(cur_bin);
                    uint32_t bin_counter(0);

                    while((this_bin < NUM_BINS) &&
                            (ctx.offset_weights[bin_counter][dir_offset] >= 0))
                    {
                        float this_weight, this_value;
                        this_weight = ctx.offset_weights[bin_counter][dir_offset] * bins[this_bin];
                        this_value = this_weight * ctx.bin_value[this_bin];
                        total_weight+= this_weight;
                        total_value+= this_value;
                        this_bin--;
                        bin_counter++;
                    }
                }

                if(total_weight > 0)
                {
                    value = trunc(total_value/total_weight);
                    if(value > 255)
                    {
                        value = 255;
                    }
                }
                else
                {
                    value = 0;
                }
                {
                    uint8_t val = value & 255;
                    row[row_index] = val;
                }
                row_index+= channels;
            }

            progress = (double)y/(double)height;
            progress*= (max_progress-min_progress);
            progress+= min_progress;
            gimp_progress_update(progress);
        }
    }
}

void tile_and_filter(const spectral::Image *source,
                     const filter_context &ctx,
                     uint32_t tile_size,
                     uint32_t radius,
                     uint32_t channel,
                     double min_progress,
                     double max_progress,
                     spectral::Image *dest)
{
    uint32_t effective_tile_size;
    uint32_t x, y;
    uint32_t num_tiles, tiles_processed;

    effective_tile_size = tile_size - (radius * 2);
    num_tiles = ((dest->get_width() / effective_tile_size) + 1) *
                ((dest->get_height() / effective_tile_size) + 1);
    x = y = tiles_processed = 0;

    do
    {
        uint32_t next_y;

        next_y = y + effective_tile_size;

        if(next_y > dest->get_height())
        {
            next_y = dest->get_height();
        }

        x = 0;
        do
        {
            uint32_t next_x;
            spectral::IntegralHistogram *hist;

            next_x = x + effective_tile_size;

            if(next_x > dest->get_width())
            {
                next_x = dest->get_width();
            }

            hist = NULL;
            {
                uint32_t xmax, ymax;
                xmax = x + tile_size;
                ymax = y + tile_size;
                if(xmax > source->get_width())
                {
                    xmax = source->get_width();
                }

                if(ymax > source->get_height())
                {
                    ymax = source->get_height();
                }

                hist = new spectral::IntegralHistogram(NUM_BINS, *source, x, y,
                                                       xmax - x,
                                                       ymax - y,
                                                       channel);
            }
            if(hist)
            {
                double min, max;
                min = tiles_processed;
                max = tiles_processed + 1;
                min/= num_tiles;
                max/= num_tiles;
                min*= (max_progress - min_progress);
                max*= (max_progress - min_progress);
                min+= min_progress;
                max+= min_progress;

                printf("processing tile at %i,%i\n",x,y);
                filter_tile(hist, ctx, radius, x, y, next_x - x, next_y - y, channel, min, max, dest);
                delete hist;
            }

            tiles_processed++;

            x = next_x;
        }
        while(x < dest->get_width());
        y = next_y;

    }
    while(y < dest->get_height());
}

void bilateral_filter(uint32_t radius, uint32_t threshold, uint32_t tile_size,
                      gboolean use_linear, gint32 image_id,
                      GimpDrawable *drawable)
{
    if(drawable)
    {
        uint32_t width, height, channels;
        gint32 drawable_id(drawable->drawable_id);
        spectral::Image *source(NULL), *dest(NULL);
        gint32 tmp;
        GimpPixelRgn rgn_in, rgn_out;
        width = drawable->width;
        height = drawable->height;

        gimp_drawable_get_pixel(drawable_id, 0, 0, &tmp);
        channels = tmp;
        gimp_pixel_rgn_init(&rgn_in, drawable, 0, 0, width, height, FALSE, FALSE);
        gimp_pixel_rgn_init(&rgn_out, drawable, 0, 0, width, height, TRUE, TRUE);

        // Build a source image.
        dest = rgn_to_image(rgn_in, width, height, channels);

        source = dest->Expand(radius, radius, false, false);

        if(source && dest)
        {
            filter_context ctx;

            gimp_progress_init("Bilateral Filter");
            initialise_filter_context(threshold, (use_linear == 0), ctx);

            for(uint32_t i=0; i<channels; i++)
            {
                float min_progress, max_progress;
                min_progress = (double)i/(double)channels;
                max_progress = (double)(i+1)/(double)channels;

                printf("crunching channel %i\n", i);

                tile_and_filter(source, ctx, tile_size, radius, i,
                                min_progress, max_progress, dest);
            }
        }

        write_image_to_rgn(dest, rgn_out);

        // clean up.
        if(source)
        {
            delete source;
        }
        if(dest)
        {
            delete dest;
        }

        // Finish working.
        gimp_drawable_flush (drawable);
        gimp_drawable_merge_shadow (drawable_id, TRUE);
        gimp_drawable_update (drawable_id, 0, 0, width, height);
    }
}

void bilateral_enhance(uint32_t radius, uint32_t threshold, float contrast, uint32_t tile_size,
                       gboolean use_linear, gint32 image_id,
                       GimpDrawable *drawable)
{
    if(drawable)
    {
        uint32_t width, height, channels;
        gint32 drawable_id(drawable->drawable_id);
        spectral::Image *source(NULL), *filtered(NULL), *enhanced(NULL);
        gint32 tmp;
        GimpPixelRgn rgn_in, rgn_out;
        width = drawable->width;
        height = drawable->height;

        gimp_drawable_get_pixel(drawable_id, 0, 0, &tmp);
        channels = tmp;
        gimp_pixel_rgn_init(&rgn_in, drawable, 0, 0, width, height, FALSE, FALSE);
        gimp_pixel_rgn_init(&rgn_out, drawable, 0, 0, width, height, TRUE, TRUE);

        // Build a source image.
        filtered = rgn_to_image(rgn_in, width, height, channels);
        enhanced = new spectral::Image(*filtered);
        source = filtered->Expand(radius, radius, false, false);

        if(source && filtered)
        {
            filter_context ctx;

            gimp_progress_init("Enhance Details");
            initialise_filter_context(threshold, (use_linear == 0), ctx);

            for(uint32_t i=0; i<channels; i++)
            {
                float min_progress, max_progress;
                min_progress = (double)i/(double)channels;
                max_progress = (double)(i+1)/(double)channels;

                min_progress *= FILTER_JOB_SHARE;
                max_progress *= FILTER_JOB_SHARE;
                printf("crunching channel %i\n", i);

                tile_and_filter(source, ctx, tile_size, radius, i,
                                min_progress, max_progress, filtered);
            }
        }

        {
            uint8_t *filtered_buf, *enhanced_buf;
            filtered_buf = filtered->get_buffer();
            enhanced_buf = enhanced->get_buffer();

            uint32_t size = width * height * channels;
            float *float_buf = (float *)calloc(size, sizeof(float));
            float max = 0;
            for(uint32_t i=0; i<size; i++)
            {
                if(i%10000 == 0)
                {
                    double progress;
                    progress = i;
                    progress/= size;
                    progress/= 2;
                    progress *= ENHANCE_JOB_SHARE;
                    progress+= FILTER_JOB_SHARE;
                    gimp_progress_update(progress);
                }

                double filtered_val, enhanced_val, difference;

                filtered_val = filtered_buf[i];
                enhanced_val = enhanced_buf[i];

                difference = enhanced_val - filtered_val;
                difference *= contrast;
                enhanced_val+= difference;
                if(enhanced_val > max)
                {
                    max = enhanced_val;
                }
                float_buf[i] = enhanced_val;
            }
            if(max > 255)
            {
                max/= 255;
            }
            else
            {
                max = 1;
            }
            for(uint32_t i=0; i<size; i++)
            {
                if(i%10000 == 0)
                {
                    double progress;
                    progress = i;
                    progress/= size;
                    progress/= 2;
                    progress+= 0.5;
                    progress *= ENHANCE_JOB_SHARE;
                    progress+= FILTER_JOB_SHARE;
                    gimp_progress_update(progress);
                }
                {
                    int32_t val;
                    val = trunc(float_buf[i]/max);
                    if(val > 255) val = 255;
                    if(val < 0) val = 0;
                    enhanced_buf[i] = val;
                }
            }
            free(float_buf);
        }
        write_image_to_rgn(enhanced, rgn_out);

        // clean up.
        if(source)
        {
            delete source;
        }
        if(filtered)
        {
            delete filtered;
        }
        if(enhanced)
        {
            delete enhanced;
        }

        // Finish working.
        gimp_drawable_flush (drawable);
        gimp_drawable_merge_shadow (drawable_id, TRUE);
        gimp_drawable_update (drawable_id, 0, 0, width, height);
    }
}

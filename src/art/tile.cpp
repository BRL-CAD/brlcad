#include "tile.h"
#include "dm.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wall"
#  pragma GCC diagnostic ignored "-Wextra"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wdocumentation"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wpedantic"
#  pragma clang diagnostic ignored "-Wignored-qualifiers"
#endif

#include "renderer/modeling/frame/frame.h"
#include "foundation/image/pixel.h"
#include "foundation/image/image.h"
#include "foundation/image/tile.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

using namespace foundation;

extern struct fb *fbp;	/* Framebuffer handle */

void ArtTileCallback::on_tile_end(const renderer::Frame* frame, const size_t tile_x, const size_t tile_y) {
    foundation::Tile& t = frame->image().tile(tile_x, tile_y);
    const foundation::Tile rgb(t, PixelFormatUInt8);
    // fb_write(fbp, tile_x, tile_y, rgb.get_storage(), rgb.get_size());
    // printf("yay!\n");
    // printf("%lu %lu \n", tile_x, tile_y);
    // printf("%lu %lu \n", rgb.get_width(), rgb.get_height());
    for (size_t y = 0; y < rgb.get_height(); y++) {
            for (size_t x = 0; x < rgb.get_width(); x++) {
                    size_t x_coord = tile_x * rgb.get_width() + x;
                    size_t y_coord = tile_y * rgb.get_height() + y;
                    size_t img_h = frame->image().properties().m_canvas_height;
                    fb_write(fbp, x_coord, img_h - y_coord, rgb.get_storage()+((y * rgb.get_width() * 4) + (x * 4)), 1);
            }

    }
}

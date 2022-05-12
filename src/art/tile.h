
#include "renderer/kernel/rendering/itilecallback.h"

class ArtTileCallback: public renderer::ITileCallback {
        public:
                virtual void on_tile_end(const renderer::Frame* frame, const size_t tile_x, const size_t tile_y);
                virtual void release() {};
                virtual void on_tiled_frame_begin(const renderer::Frame* frame) {};
                virtual void on_tiled_frame_end(const renderer::Frame* frame) {};
                virtual void on_tile_begin(const renderer::Frame* frame, const size_t tile_x, const size_t tile_y){};
                virtual void on_progressive_frame_update(const renderer::Frame* frame) {};
};

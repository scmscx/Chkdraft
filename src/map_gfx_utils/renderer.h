#ifndef RENDERER_H
#define RENDERER_H
#include "sc_map.h"
#include "screenshot.h"
#include <glfw/context.h>
#include <glfw/window.h>
#include <glad/glad.h>
#include <glad/utils.h>
#include <gl/utils.h>

enum class RenderSkin
{
    Classic,
    RemasteredSD,
    RemasteredHD2,
    RemasteredHD,
    CarbotHD2,
    CarbotHD,
    Total,
    Unknown,
};

/* A renderer has one skin+tileset active at a time,
    maps (which have chk data and an anim state) can be passed to a renderer to save screenshots or display the map;
    multiple maps may be used with the same renderer.
   
   Batching Considerations:
    - Renderer options besides the skin can be changed without load time or additional memory use
    - Initial loads of skins is expensive, once a skin is loaded, switching skins is ~free, however
       the VRAM/RAM that the skin requires will not be free'd until the renderer is disposed of.
    - Image Textures (per-skin):
        - Initial Load: Long, happens first time using a skin
        - Memory: Collectively these take up a lot, images stay in VRAM/RAM till disposing of the renderer
    - Tileset Textures (per-skin, per-tileset):
        - Initial Load: Short, happens first time using a tileset on a given skin
        - Memory: Relatively small, tileset textures stay in VRAM/RAM till disposing of the renderer
    - If tight on memory, load a renderer for a single skin and go through a batch of maps...
       whilst on a map, vary the draw settings (except for the skin) and the anim state and get all the images you want
       then free the renderer, create a new renderer for the next skin, and go through that batch of maps for that skin */
struct Renderer
{
    glfw::Context glfwContext {};
    glfw::Window window {};
    std::string starCraftDirectory;
    const Sc::Data & scData;
    ScMap* displayedMap = nullptr;
    std::unique_ptr<GraphicsData> graphicsData;
    RenderSkin renderSkin = RenderSkin::Unknown;
    bool showGui = false;

    struct Options {
        bool drawStars = false;
        bool drawTerrain = true;
        bool drawActors = true; // Units & sprites
        std::optional<u8> drawFogPlayer = std::nullopt; // Zero-based
        bool drawLocations = false;
        bool displayFps = false;
        bool drawMiniMap = false;
    };

    Renderer(const Sc::Data & scData);

    void initialize();

    void processInput();

    void clearWindow();

    bool loadSkinAndTileSet(RenderSkin skin, ScMap & map);

    int encodeMapImageAsWebP(ScMap & map, const Options & options, auto use) // Returns the time spent in the renderMap(..) function in ms
    {
        int tileWidth = int(map.getTileWidth());
        int tileHeight = int(map.getTileHeight());
        int maxTileDimension = std::max(tileWidth, tileHeight);
        int zoomSizeMultiplier = 1;
        switch ( renderSkin )
        {
        case RenderSkin::Classic: zoomSizeMultiplier = 1; break;
        case RenderSkin::RemasteredSD: zoomSizeMultiplier = 1; break;
        case RenderSkin::RemasteredHD2: zoomSizeMultiplier = 2; break;
        case RenderSkin::RemasteredHD: zoomSizeMultiplier = 4; break;
        case RenderSkin::CarbotHD2: zoomSizeMultiplier = 2; break;
        case RenderSkin::CarbotHD: zoomSizeMultiplier = 4; break;
        case RenderSkin::Unknown: case RenderSkin::Total:
            throw std::logic_error("encodeMapImageAsWebP called without properly initializing the renderer skin!");
        }

        // HD and HD2 are best 400% and 200% zoomed respectively, and can be smoothly halved until they fit in one image
        int pixelSizeMultiplier = 32 * zoomSizeMultiplier;
        while ( zoomSizeMultiplier > 1 && pixelSizeMultiplier*maxTileDimension > 16384 )
        {
            zoomSizeMultiplier /= 2;
            pixelSizeMultiplier = 32 * zoomSizeMultiplier;
        }

        map.graphics.setZoom(float(zoomSizeMultiplier));
        auto imageWidth = pixelSizeMultiplier*tileWidth;
        auto imageHeight = pixelSizeMultiplier*tileHeight;
        if ( imageWidth > 16384 || imageHeight > 16384 )
        {
            throw std::logic_error("Map image dimensions exceed maximum for webp format!");
            return -1;
        }

        GLint savedViewport[4] {};
        glGetIntegerv(GL_VIEWPORT, savedViewport);

        GLuint framebuffer = 0;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        GLuint renderbuffer = 0;
        glGenRenderbuffers(1, &renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, imageWidth, imageHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, renderbuffer);
        glDrawBuffer(GL_COLOR_ATTACHMENT1);

        glViewport(0, 0, imageWidth, imageHeight);
        map.graphics.windowBoundsChanged({.left = 0, .top = 0, .right = imageWidth, .bottom = imageHeight});

        map.graphics.setVerticallyFlipped(true);
        auto start = std::chrono::high_resolution_clock::now();
        renderMap(map, options);
        auto end = std::chrono::high_resolution_clock::now();

        glFlush();

        glReadBuffer(GL_COLOR_ATTACHMENT1);
        EncodedWebP encodedMapImage {};
        if ( encodeOpenGlViewportImage(0, 0, imageWidth, imageHeight, encodedMapImage) )
            use(encodedMapImage);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        window.pollEvents();
        window.swapBuffers();

        map.graphics.setVerticallyFlipped(false);
        map.graphics.windowBoundsChanged({ savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3] });
        glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
        return std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
    }

    int encodeMiniMapImageAsWebP(ScMap & map, auto use) // Returns the time spent specifically rendering the minimap in ms
    {
        auto start = std::chrono::high_resolution_clock::now();
        ClassicMiniMap classicMiniMap(this->scData, map);
        classicMiniMap.render();
        
        if ( offsetof(Sc::Terrain::WpeColor, red) == offsetof(Sc::SystemColor, blue) &&
            offsetof(Sc::Terrain::WpeColor, green) == offsetof(Sc::SystemColor, green) &&
            offsetof(Sc::Terrain::WpeColor, blue) == offsetof(Sc::SystemColor, red) )
        {
            for ( std::uint32_t & pixel : classicMiniMap.pixels )
                std::swap(((std::uint8_t*)&pixel)[0], ((std::uint8_t*)&pixel)[2]); // red-blue swap
        }
        for ( std::uint32_t & pixel : classicMiniMap.pixels )
            ((std::uint8_t*)&pixel)[3] = 255; // opacity

        auto end = std::chrono::high_resolution_clock::now();

        EncodedWebP encodedMiniMapImage {};
        if ( encodePixelDataImage(classicMiniMap.width, classicMiniMap.height, &classicMiniMap.pixels[0], encodedMiniMapImage) )
            use(encodedMiniMapImage);

        return std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
    }

    struct SaveWebpResult
    {
        int loadSkinAndTilesetTimeMs = 0;
        int renderTimeMs = 0;
        int encodeTimeMs = 0; // Time for WebPEncodeLosslessRGB https://developers.google.com/speed/webp/docs/api , usually dwarves other times
        int outFileTimeMs = 0; // Time to output to disk
        std::string outputFilePath;
        std::string miniMapOutputFilePath;
    };

    std::optional<SaveWebpResult> saveMiniMapImageAsWebP(ScMap & map, const std::string & outputFilePath, bool dataLoaded = false);

    std::optional<SaveWebpResult> saveMapImageAsWebP(ScMap & map, const Options & options, const std::string & outputFilePath, const std::string & miniMapOutputFilePath = "");

    struct RawImageResult
    {
        std::vector<uint8_t> pixelData; // RGB, width*height*3
        int width = 0;
        int height = 0;
        int renderTimeMs = 0;
    };

    // Render map to raw RGB pixels without WebP encoding.
    // If the full image exceeds GL_MAX_RENDERBUFFER_SIZE, renders in tiles and stitches them.
    RawImageResult renderMapToRawImage(ScMap & map, const Options & options)
    {
        // Drain any accumulated GL errors from prior operations
        while ( glGetError() != GL_NO_ERROR ) {}

        int tileWidth = int(map.getTileWidth());
        int tileHeight = int(map.getTileHeight());
        int zoomSizeMultiplier = 1;
        switch ( renderSkin )
        {
        case RenderSkin::Classic: zoomSizeMultiplier = 1; break;
        case RenderSkin::RemasteredSD: zoomSizeMultiplier = 1; break;
        case RenderSkin::RemasteredHD2: zoomSizeMultiplier = 2; break;
        case RenderSkin::RemasteredHD: zoomSizeMultiplier = 4; break;
        case RenderSkin::CarbotHD2: zoomSizeMultiplier = 2; break;
        case RenderSkin::CarbotHD: zoomSizeMultiplier = 4; break;
        case RenderSkin::Unknown: case RenderSkin::Total:
            throw std::logic_error("renderMapToRawImage called without properly initializing the renderer skin!");
        }

        int pixelSizeMultiplier = 32 * zoomSizeMultiplier;
        map.graphics.setZoom(float(zoomSizeMultiplier));
        auto imageWidth = pixelSizeMultiplier*tileWidth;
        auto imageHeight = pixelSizeMultiplier*tileHeight;

        GLint maxRbSize = 0;
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRbSize);
        if ( maxRbSize <= 0 ) maxRbSize = 4096;

        if ( imageWidth <= maxRbSize && imageHeight <= maxRbSize )
            return renderMapToRawImageSinglePass(map, options, imageWidth, imageHeight);
        else
            return renderMapToRawImageTiled(map, options, imageWidth, imageHeight, maxRbSize, zoomSizeMultiplier);
    }

private:
    RawImageResult renderMapToRawImageSinglePass(ScMap & map, const Options & options, int imageWidth, int imageHeight)
    {
        GLint savedViewport[4] {};
        glGetIntegerv(GL_VIEWPORT, savedViewport);

        GLuint framebuffer = 0;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        GLuint renderbuffer = 0;
        glGenRenderbuffers(1, &renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, imageWidth, imageHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, renderbuffer);
        glDrawBuffer(GL_COLOR_ATTACHMENT1);

        glViewport(0, 0, imageWidth, imageHeight);
        map.graphics.windowBoundsChanged({.left = 0, .top = 0, .right = imageWidth, .bottom = imageHeight});

        map.graphics.setVerticallyFlipped(true);
        auto start = std::chrono::high_resolution_clock::now();
        renderMap(map, options);
        auto end = std::chrono::high_resolution_clock::now();

        glFlush();

        glReadBuffer(GL_COLOR_ATTACHMENT1);
        std::vector<uint8_t> pixelData(imageWidth * imageHeight * 3);
        glReadPixels(0, 0, imageWidth, imageHeight, GL_RGB, GL_UNSIGNED_BYTE, pixelData.data());

        glDeleteRenderbuffers(1, &renderbuffer);
        glDeleteFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        window.pollEvents();
        window.swapBuffers();

        map.graphics.setVerticallyFlipped(false);
        map.graphics.windowBoundsChanged({ savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3] });
        glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);

        RawImageResult result;
        result.pixelData = std::move(pixelData);
        result.width = imageWidth;
        result.height = imageHeight;
        result.renderTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
        return result;
    }

    RawImageResult renderMapToRawImageTiled(ScMap & map, const Options & options, int imageWidth, int imageHeight, int maxTileSize, int zoom)
    {
        // Disable stars for tiled rendering — star parallax is viewport-dependent and causes seams
        Options tiledOptions = options;
        tiledOptions.drawStars = false;

        size_t totalBytes = size_t(imageWidth) * size_t(imageHeight) * 3;
        std::vector<uint8_t> pixelData(totalBytes, 0);

        GLint savedViewport[4] {};
        glGetIntegerv(GL_VIEWPORT, savedViewport);

        GLuint framebuffer = 0;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        GLuint renderbuffer = 0;
        glGenRenderbuffers(1, &renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, maxTileSize, maxTileSize);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, renderbuffer);
        glDrawBuffer(GL_COLOR_ATTACHMENT1);

        map.graphics.setVerticallyFlipped(true);
        auto start = std::chrono::high_resolution_clock::now();

        for ( int tileY = 0; tileY < imageHeight; tileY += maxTileSize )
        {
            for ( int tileX = 0; tileX < imageWidth; tileX += maxTileSize )
            {
                int tw = std::min(maxTileSize, imageWidth - tileX);
                int th = std::min(maxTileSize, imageHeight - tileY);

                // windowBoundsChanged expects: .left/.top = game-space scroll offset,
                // .right/.bottom = game offset + screen-space tile size.
                // Convert pixel tile offset to game coordinates by dividing by zoom.
                int gameX = tileX / zoom;
                int gameY = tileY / zoom;
                glViewport(0, 0, tw, th);
                map.graphics.windowBoundsChanged({.left = gameX, .top = gameY, .right = gameX + tw, .bottom = gameY + th});

                renderMap(map, tiledOptions);
                glFlush();

                glReadBuffer(GL_COLOR_ATTACHMENT1);
                std::vector<uint8_t> tilePixels(tw * th * 3);
                glReadPixels(0, 0, tw, th, GL_RGB, GL_UNSIGNED_BYTE, tilePixels.data());

                // Copy tile rows into the correct position in the full image
                for ( int row = 0; row < th; row++ )
                {
                    auto* src = &tilePixels[row * tw * 3];
                    auto* dst = &pixelData[(size_t(tileY + row) * imageWidth + tileX) * 3];
                    std::memcpy(dst, src, tw * 3);
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();

        glDeleteRenderbuffers(1, &renderbuffer);
        glDeleteFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        window.pollEvents();
        window.swapBuffers();

        map.graphics.setVerticallyFlipped(false);
        map.graphics.windowBoundsChanged({ savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3] });
        glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);

        RawImageResult result;
        result.pixelData = std::move(pixelData);
        result.width = imageWidth;
        result.height = imageHeight;
        result.renderTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
        return result;
    }

public:

    // Render minimap to raw RGB pixels without WebP encoding
    RawImageResult renderMiniMapToRawImage(ScMap & map)
    {
        auto start = std::chrono::high_resolution_clock::now();
        ClassicMiniMap classicMiniMap(this->scData, map);
        classicMiniMap.render();

        if ( offsetof(Sc::Terrain::WpeColor, red) == offsetof(Sc::SystemColor, blue) &&
            offsetof(Sc::Terrain::WpeColor, green) == offsetof(Sc::SystemColor, green) &&
            offsetof(Sc::Terrain::WpeColor, blue) == offsetof(Sc::SystemColor, red) )
        {
            for ( std::uint32_t & pixel : classicMiniMap.pixels )
                std::swap(((std::uint8_t*)&pixel)[0], ((std::uint8_t*)&pixel)[2]); // red-blue swap
        }
        auto end = std::chrono::high_resolution_clock::now();

        // Convert RGBA uint32_t pixels to RGB uint8_t
        int w = int(classicMiniMap.width);
        int h = int(classicMiniMap.height);
        std::vector<uint8_t> rgbData(w * h * 3);
        for ( int i = 0; i < w * h; i++ )
        {
            auto* src = reinterpret_cast<uint8_t*>(&classicMiniMap.pixels[i]);
            rgbData[i*3 + 0] = src[0]; // R
            rgbData[i*3 + 1] = src[1]; // G
            rgbData[i*3 + 2] = src[2]; // B
        }

        RawImageResult result;
        result.pixelData = std::move(rgbData);
        result.width = w;
        result.height = h;
        result.renderTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
        return result;
    }

    int getMapImageAsWebP(ScMap & map, const Options & options, EncodedWebP & encodedWebP); // Returns time spend in renderMap(..) in ms

    void displayInGui(ScMap & map, const Options & options, bool autoAnimate);

    // Renders the map with the current settings, see GuiMapGraphics::render
    void renderMap(ScMap & map, const Options & options, std::chrono::system_clock::time_point frameStart = std::chrono::system_clock::now());
};


#endif

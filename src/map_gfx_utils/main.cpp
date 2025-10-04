#include "gfx_util.h"
#include <iostream>
#include <thread>
#include <filesystem>

extern Logger logger;

// /** If a map is coming not from the filesystem but from something that can be loaded in memory... Or if using a parser more robust than chkd...
//     This serves as an example of how to place such data into a map for rendering

//     All the Chk:: data structures are packed so you should be able to allocate a vector then push binary data into it if desired
// */
// std::unique_ptr<ScMap> exampleLoadFromMemory(GfxUtil & gfxUtil, Sc::Terrain::Tileset tileset)
// {
//     auto scMap = gfxUtil.createBlankMap();

//     std::vector<Chk::Unit> units {};
//     std::vector<Chk::Sprite> sprites {};
//     for ( std::size_t i=0; i<Sc::Unit::TotalTypes; ++i )
//     {
//         int x = i%32;
//         int y = i/32;
//         units.push_back(Chk::Unit {0, u16(x*64+64), u16(y*64+64), Sc::Unit::Type(i), 0, 0, 0, Sc::Player::Id::Player1});
//     }
//     for ( std::size_t i=0; i<Sc::Sprite::TotalSprites; ++i )
//     {
//         int x = i%32;
//         int y = i/32;
//         //sprites.push_back(Chk::Sprite {Sc::Sprite::Type(i), u16(x*64+64), u16(y*64+64), Sc::Player::Id::Player1, 0, Chk::Sprite::SpriteFlags::DrawAsSprite});
//     }

//     std::vector<u16> mtxmTerrain(std::size_t(64*64), u16(0));
//     std::iota(mtxmTerrain.begin(), mtxmTerrain.end(), 0); // Fills with ascending tile ids starting from 0

//     scMap->initData(MapData {
//         .sprites = std::move(sprites),
//         .units = std::move(units),
//         .dimensions { .tileWidth = 64, .tileHeight = 64 },
//         .tileset = tileset,
//         .tiles = std::move(mtxmTerrain),
//     });
//     scMap->initAnims();
    
//     return scMap;
// }

// void testRender(GfxUtil & gfxUtil, Renderer & renderer, Sc::Terrain::Tileset tileset, const std::string & outFilePath)
// {
//     Renderer::Options options {
//         .drawStars = true,
//         .drawTerrain = true,
//         .drawActors = true,
//         .drawFogPlayer = std::nullopt,
//         .drawLocations = false,
//         .displayFps = false
//     };

//     auto mapStartTime = std::chrono::high_resolution_clock::now();
//     //auto map = gfxUtil.loadMap(); // Use a browser
//     //auto map = gfxUtil.loadMap("C:/misc/SimpleLoc.scx"); // Use an absolute file path
//     auto map = exampleLoadFromMemory(gfxUtil, tileset); // Use something you already have in memory
//     auto mapLoadTime = std::chrono::high_resolution_clock::now();

//     auto animTime = map->simulateAnim(52); // Simulate n anim ticks occuring, need at least 52 to extend all the tanks

//     //renderer.displayInGui(*map, options, true);
//     auto imageTimes = renderer.saveMapImageAsWebP(*map, options, outFilePath);
//     auto mapFinishTime = std::chrono::high_resolution_clock::now();
//     logger.log(GfxUtilInfo) << MapTimings(mapStartTime, mapLoadTime, animTime, imageTimes, mapFinishTime) << std::endl;
// }

int main(int argc, char* argv[])
{
    if (argc != 4) {
        logger.info() << "Usage: " << argv[0] << " <starcraft_path> <input_folder> <output_folder>" << std::endl;
    }

    auto starcraftPath = argv[1];
    auto inputFolder = argv[2];
    auto outputDirectory = argv[3];

    auto startInitialLoad = std::chrono::high_resolution_clock::now();
    GfxUtil gfxUtil {};
    gfxUtil.loadScData(starcraftPath);
    auto renderer = gfxUtil.createRenderer(RenderSkin::Classic);
    auto endInitialLoad = std::chrono::high_resolution_clock::now();

        logger.log(GfxUtilInfo) << "Initial load completed in " << std::chrono::duration_cast<std::chrono::milliseconds>(endInitialLoad-startInitialLoad).count() << "ms" << std::endl;

    // testRender(gfxUtil, *renderer, Sc::Terrain::Tileset::SpacePlatform, getDefaultFolder() + "space.webp");
    // testRender(gfxUtil, *renderer, Sc::Terrain::Tileset::Jungle, getDefaultFolder() + "jungle.webp");
    // testRender(gfxUtil, *renderer, Sc::Terrain::Tileset::SpacePlatform, getDefaultFolder() + "space2.webp");

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        for (const auto& entry : std::filesystem::directory_iterator(inputFolder)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            
            std::string inputFilePath = entry.path().string();
            std::string inputFileName = entry.path().filename().string();

            // the filename is likely in the format <chkblob>.scx, we want to remove the extension, if it exists
            std::string outputFileName;
            if (inputFileName.size() > 4 && (
                        (inputFileName.substr(inputFileName.size() - 4) == ".scx") || 
                        (inputFileName.substr(inputFileName.size() - 4) == ".scm")
                    )
                )
            {
                outputFileName = inputFileName.substr(0, inputFileName.size() - 4);
            } else {
                // not a map file.
                continue;
            }

            try {
                logger.info() << "Processing " << inputFilePath << std::endl;
                auto tempOutputFilePath = std::string(outputDirectory) + "/" + outputFileName + ".tmp";

                // Load the map
                auto mapStartTime = std::chrono::high_resolution_clock::now();
                auto map = gfxUtil.loadMap(inputFilePath);
                if (!map) {
                    logger.error() << "Failed to load map " << inputFilePath << std::endl;
                    std::filesystem::rename(inputFilePath, inputFilePath + ".failed");
                    continue;
                }
                auto mapLoadTime = std::chrono::high_resolution_clock::now();

                auto animTime = map->simulateAnim(52); // Simulate n anim ticks occuring, need at least 52 to extend all the tanks

                //renderer.displayInGui(*map, options, true);
                Renderer::Options options {
                    .drawStars = true,
                    .drawTerrain = true,
                    .drawActors = true,
                    .drawFogPlayer = std::nullopt,
                    .drawLocations = false,
                    .displayFps = false
                };
                auto imageTimes = renderer->saveMapImageAsWebP(*map, options, tempOutputFilePath);
                auto mapFinishTime = std::chrono::high_resolution_clock::now();
                logger << "Output file: " << tempOutputFilePath << std::endl << MapTimings(mapStartTime, mapLoadTime, animTime, imageTimes, mapFinishTime) << std::endl;

                // rename the tempfile to the correct directory
                std::filesystem::rename(tempOutputFilePath, std::string(outputDirectory) + "/" + outputFileName + ".webp");

                // Delete the input file
                std::filesystem::remove(inputFilePath);

            } catch (const std::exception& e) {
                logger.error() << "Some Error: " << e.what() << std::endl;
            } catch (...) {
                logger.error() << "Unknown Error" << std::endl;
            }
        }
    }

    return 0;
}

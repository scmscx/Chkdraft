#include "gfx_util.h"
#include <iostream>
#include <thread>
#include <filesystem>

extern Logger logger;

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
    auto renderer = gfxUtil.createRenderer(RenderSkin::ClassicGL);
    auto endInitialLoad = std::chrono::high_resolution_clock::now();

    logger.info() << "Initial load completed in " << std::chrono::duration_cast<std::chrono::milliseconds>(endInitialLoad-startInitialLoad).count() << "ms" << std::endl;

    // testRender(gfxUtil, *renderer, Sc::Terrain::Tileset::SpacePlatform, getDefaultFolder() + "space.webp");
    // testRender(gfxUtil, *renderer, Sc::Terrain::Tileset::Jungle, getDefaultFolder() + "jungle.webp");
    // testRender(gfxUtil, *renderer, Sc::Terrain::Tileset::SpacePlatform, getDefaultFolder() + "space2.webp");

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        try {
            // Find one file in the input folder
            std::string inputFilePath;
            std::string inputFileName;
            for (const auto& entry : std::filesystem::directory_iterator(inputFolder)) {
                if (entry.is_regular_file()) {
                    inputFilePath = entry.path().string();
                    inputFileName = entry.path().filename().string();
                    break;
                }
            }

            if (inputFilePath.empty()) {
                continue;
            }

            // the filename is likely in the format <chkblob>.scx, we want to remove the extension, if it exists
            std::string outputFileName;
            if (inputFileName.size() > 4 && inputFileName.substr(inputFileName.size() - 4) == ".scx") {
                outputFileName = inputFileName.substr(0, inputFileName.size() - 4);
            } else {
                outputFileName = inputFileName;
            }

            logger.info() << "Processing " << inputFilePath << std::endl;
            auto tempOutputFilePath = std::string(outputDirectory) + "/" + outputFileName + ".tmp";

            // Load the map
            auto mapStartTime = std::chrono::high_resolution_clock::now();
            auto map = gfxUtil.loadMap(inputFilePath);
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
            if (std::filesystem::exists(inputFilePath)) {
                std::filesystem::remove(inputFilePath);
            }
        }
        catch (const std::exception& e) {
            logger.error() << "Some Error: " <<e.what() << std::endl;
        }
        catch (...) {
            logger.error() << "Unknown Error" << std::endl;
        }
    }

    return 0;
}

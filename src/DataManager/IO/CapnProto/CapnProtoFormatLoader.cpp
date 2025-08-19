#include "CapnProtoFormatLoader.hpp"

#include "Line_Data_Binary.hpp"
#include "Lines/Line_Data.hpp"

#include <iostream>

LoadResult CapnProtoFormatLoader::load(std::string const& filepath, 
                                      IODataType dataType, 
                                      nlohmann::json const& config, 
                                      DataFactory* factory) const {
    if (!factory) {
        return LoadResult("DataFactory is null");
    }

    switch (dataType) {
        case IODataType::Line:
            return loadLineDataCapnProto(filepath, config, factory);
            
        default:
            return LoadResult("CapnProto loader does not support data type: " + std::to_string(static_cast<int>(dataType)));
    }
}

bool CapnProtoFormatLoader::supportsFormat(std::string const& format, IODataType dataType) const {
    // Support capnp and binary formats for LineData
    if ((format == "capnp" || format == "binary") && dataType == IODataType::Line) {
        return true;
    }
    
    // Could add support for other data types in the future
    return false;
}

std::string CapnProtoFormatLoader::getLoaderName() const {
    return "CapnProtoLoader";
}

LoadResult CapnProtoFormatLoader::loadLineDataCapnProto(std::string const& filepath, 
                                                       nlohmann::json const& config, 
                                                       DataFactory* factory) const {
    try {
        // Use existing CapnProto loading functionality
        BinaryLineLoaderOptions opts;
        opts.file_path = filepath;
        
        auto loaded_line_data = ::load(opts);
        if (!loaded_line_data) {
            return LoadResult("Failed to load CapnProto LineData from: " + filepath);
        }
        
        // Convert to factory-created object
        // First extract the data map from the loaded LineData
        std::map<TimeFrameIndex, std::vector<Line2D>> line_map;
        for (auto const& time : loaded_line_data->getTimesWithData()) {
            line_map[time] = loaded_line_data->getAtTime(time);
        }
        
        // Create new LineData using factory
        auto line_data_variant = factory->createLineData(line_map);
        
        // Apply image size from the loaded data
        ImageSize image_size = loaded_line_data->getImageSize();
        if (image_size.width > 0 && image_size.height > 0) {
            factory->setLineDataImageSize(line_data_variant, 
                                         static_cast<int>(image_size.width), 
                                         static_cast<int>(image_size.height));
        }
        
        // Apply image size override from config if specified
        if (config.contains("image_width") && config.contains("image_height")) {
            int width = config["image_width"];
            int height = config["image_height"];
            factory->setLineDataImageSize(line_data_variant, width, height);
        }
        
        return LoadResult(std::move(line_data_variant));
        
    } catch (std::exception const& e) {
        return LoadResult("CapnProto loading failed: " + std::string(e.what()));
    }
}

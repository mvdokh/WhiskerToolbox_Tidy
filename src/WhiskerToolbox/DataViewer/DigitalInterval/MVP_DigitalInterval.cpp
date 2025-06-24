#include "MVP_DigitalInterval.hpp"

#include "DigitalIntervalSeriesDisplayOptions.hpp"
#include "PlottingManager/PlottingManager.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>


glm::mat4 new_getIntervalModelMat(NewDigitalIntervalSeriesDisplayOptions const & display_options,
                                  PlottingManager const & plotting_manager) {

    static_cast<void>(plotting_manager);

    auto Model = glm::mat4(1.0f);

    // Apply global zoom scaling
    float const global_scale = display_options.global_zoom * display_options.global_vertical_scale;

    // For digital intervals extending full canvas, we want them to span the entire allocated height
    // The allocated_height represents the total height available for this series
    if (display_options.extend_full_canvas) {
        // Scale to use the full allocated height with margin factor
        float const height_scale = display_options.allocated_height * display_options.margin_factor * 0.5f;
        Model = glm::scale(Model, glm::vec3(1.0f, height_scale * global_scale, 1.0f));

        // Translate to allocated center position
        Model = glm::translate(Model, glm::vec3(0.0f, display_options.allocated_y_center, 0.0f));
    } else {
        // Apply standard scaling and positioning
        Model = glm::scale(Model, glm::vec3(1.0f, global_scale, 1.0f));
        Model = glm::translate(Model, glm::vec3(0.0f, display_options.allocated_y_center, 0.0f));
    }

    return Model;
}

glm::mat4 new_getIntervalViewMat(PlottingManager const & plotting_manager) {

    static_cast<void>(plotting_manager);

    auto View = glm::mat4(1.0f);

    // Digital intervals should NOT move with vertical panning
    // They always extend from the top to bottom of the current view
    // Unlike analog series, panning should not affect their position
    // (they remain "pinned" to the current viewport)

    return View;
}

glm::mat4 new_getIntervalProjectionMat(int start_data_index,
                                       int end_data_index,
                                       float y_min,
                                       float y_max,
                                       PlottingManager const & plotting_manager) {

    static_cast<void>(y_min);
    static_cast<void>(y_max);

    // Convert data indices to normalized coordinate range
    // Map [start_data_index, end_data_index] to [-1, 1] in X
    // Map [y_min, y_max] to viewport range in Y

    auto const left = static_cast<float>(start_data_index);
    auto const right = static_cast<float>(end_data_index);

    // Use viewport Y range from plotting manager with any additional pan offset
    float const bottom = plotting_manager.viewport_y_min;
    float const top = plotting_manager.viewport_y_max;

    // Validate and sanitize input parameters to prevent OpenGL state corruption
    float safe_left = left;
    float safe_right = right;
    float safe_bottom = bottom;
    float safe_top = top;
    
    // Check for and fix invalid or extreme values that could cause NaN/Infinity
    
    // 1. Ensure all values are finite
    if (!std::isfinite(safe_left)) {
        std::cout << "Warning: Invalid interval left=" << safe_left << ", using fallback" << std::endl;
        safe_left = 0.0f;
    }
    if (!std::isfinite(safe_right)) {
        std::cout << "Warning: Invalid interval right=" << safe_right << ", using fallback" << std::endl;
        safe_right = 1000.0f;
    }
    if (!std::isfinite(safe_bottom)) {
        std::cout << "Warning: Invalid interval bottom=" << safe_bottom << ", using fallback" << std::endl;
        safe_bottom = -1.0f;
    }
    if (!std::isfinite(safe_top)) {
        std::cout << "Warning: Invalid interval top=" << safe_top << ", using fallback" << std::endl;
        safe_top = 1.0f;
    }
    
    // 2. Ensure X range is valid (left < right with minimum separation)
    constexpr float min_range = 1e-6f;  // Minimum range to prevent division by zero
    if (safe_right <= safe_left) {
        std::cout << "Warning: Invalid interval X range [" << safe_left << ", " << safe_right 
                  << "], fixing to valid range" << std::endl;
        float center = (safe_left + safe_right) * 0.5f;
        safe_left = center - min_range * 0.5f;
        safe_right = center + min_range * 0.5f;
    } else if ((safe_right - safe_left) < min_range) {
        std::cout << "Warning: Interval X range too small [" << safe_left << ", " << safe_right 
                  << "], expanding to minimum safe range" << std::endl;
        float center = (safe_left + safe_right) * 0.5f;
        safe_left = center - min_range * 0.5f;
        safe_right = center + min_range * 0.5f;
    }
    
    // 3. Ensure Y range is valid
    if (safe_top <= safe_bottom) {
        std::cout << "Warning: Invalid interval Y range [" << safe_bottom << ", " << safe_top 
                  << "], fixing to valid range" << std::endl;
        float center = (safe_bottom + safe_top) * 0.5f;
        safe_bottom = center - min_range * 0.5f;
        safe_top = center + min_range * 0.5f;
    } else if ((safe_top - safe_bottom) < min_range) {
        std::cout << "Warning: Interval Y range too small [" << safe_bottom << ", " << safe_top 
                  << "], expanding to minimum safe range" << std::endl;
        float center = (safe_bottom + safe_top) * 0.5f;
        safe_bottom = center - min_range * 0.5f;
        safe_top = center + min_range * 0.5f;
    }
    
    // 4. Clamp extreme values to prevent numerical issues
    constexpr float max_abs_value = 1e8f;  // Large but safe limit
    if (std::abs(safe_left) > max_abs_value || std::abs(safe_right) > max_abs_value) {
        std::cout << "Warning: Extremely large interval X range [" << safe_left << ", " << safe_right 
                  << "], clamping to safe range" << std::endl;
        float range = safe_right - safe_left;
        if (range > 2 * max_abs_value) {
            safe_left = -max_abs_value;
            safe_right = max_abs_value;
        } else {
            float center = (safe_left + safe_right) * 0.5f;
            center = std::clamp(center, -max_abs_value * 0.5f, max_abs_value * 0.5f);
            safe_left = center - range * 0.5f;
            safe_right = center + range * 0.5f;
        }
    }

    // Create orthographic projection matrix with validated parameters
    auto Projection = glm::ortho(safe_left, safe_right, safe_bottom, safe_top);
    
    // Final validation: check that the resulting matrix is valid
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (!std::isfinite(Projection[i][j])) {
                std::cout << "Error: Interval projection matrix contains invalid value at [" << i << "][" << j 
                          << "]=" << Projection[i][j] << ", using identity matrix" << std::endl;
                return glm::mat4(1.0f);  // Return identity matrix as safe fallback
            }
        }
    }

    return Projection;
}

std::vector<Interval> generateTestIntervalData(size_t num_intervals,
                                               float max_time,
                                               float min_duration,
                                               float max_duration,
                                               unsigned int seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> time_dist(0.0f, max_time * 0.8f);// Leave some space for intervals
    std::uniform_real_distribution<float> duration_dist(min_duration, max_duration);

    std::vector<Interval> intervals;
    intervals.reserve(num_intervals);

    // Generate random intervals
    for (size_t i = 0; i < num_intervals; ++i) {
        float start_time = time_dist(gen);
        float duration = duration_dist(gen);
        float end_time = start_time + duration;

        // Ensure we don't exceed max_time
        if (end_time > max_time) {
            end_time = max_time;
            start_time = std::max(0.0f, end_time - duration);
        }

        // Convert to int64_t for Interval struct
        intervals.emplace_back(Interval{static_cast<int64_t>(start_time), static_cast<int64_t>(end_time)});
    }

    // Sort intervals by start time to ensure ordering requirement
    std::sort(intervals.begin(), intervals.end(),
              [](Interval const & a, Interval const & b) {
                  return a.start < b.start;
              });

    // Validate all intervals (end > start)
    for (auto & interval: intervals) {
        if (interval.end <= interval.start) {
            // This shouldn't happen with our generation method, but just in case
            interval.end = interval.start + static_cast<int64_t>(min_duration);
        }
    }

    return intervals;
}

void setIntervalIntrinsicProperties(std::vector<Interval> const & intervals,
                                    NewDigitalIntervalSeriesDisplayOptions & display_options) {
    if (intervals.empty()) {
        return;
    }

    // For digital intervals, intrinsic properties are minimal since they're meant to be
    // visual indicators rather than quantitative data. The main consideration is
    // ensuring good visibility across the canvas.

    // Calculate some basic statistics for potential future use
    int64_t total_duration = 0;
    int64_t min_duration_val = std::numeric_limits<int64_t>::max();
    int64_t max_duration_val = 0;

    for (auto const & interval: intervals) {
        int64_t const duration = interval.end - interval.start;
        total_duration += duration;
        min_duration_val = std::min(min_duration_val, duration);
        max_duration_val = std::max(max_duration_val, duration);
    }

    //int64_t const avg_duration = total_duration / static_cast<int64_t>(intervals.size());

    // For now, we keep the default display options
    // Future enhancements could adjust alpha based on interval density,
    // or adjust positioning based on overlap analysis

    // Ensure alpha is appropriate for overlapping intervals
    if (intervals.size() > 50) {
        // With many intervals, reduce alpha to prevent over-saturation
        display_options.alpha = std::max(0.1f, 0.3f * std::sqrt(50.0f / static_cast<float>(intervals.size())));
    }

    // Set full canvas extension for maximum visibility
    display_options.extend_full_canvas = true;
}

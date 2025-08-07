#include "PointComponentAdapter.h"

#include <algorithm>
#include <stdexcept>

PointComponentAdapter::PointComponentAdapter(std::shared_ptr<PointData> pointData,
                                             Component component,
                                             std::shared_ptr<TimeFrame> timeFrame,
                                             std::string name)
    : m_pointData(std::move(pointData)),
      m_component(component),
      m_timeFrame(std::move(timeFrame)),
      m_name(std::move(name)) {
    if (!m_pointData) {
        throw std::invalid_argument("PointData cannot be null");
    }
}

std::string const & PointComponentAdapter::getName() const {
    return m_name;
}

std::shared_ptr<TimeFrame> PointComponentAdapter::getTimeFrame() const {
    return m_timeFrame;
}

size_t PointComponentAdapter::size() const {
    if (!m_isMaterialized) {
        // Calculate size without materializing
        size_t totalPoints = 0;
        for (auto const & [time, points]: m_pointData->GetAllPointsAsRange()) {
            totalPoints += points.size();
        }
        return totalPoints;
    }
    return m_materializedData.size();
}

std::vector<float> PointComponentAdapter::getDataInRange(TimeFrameIndex start,
                                                         TimeFrameIndex end,
                                                         TimeFrame const * target_timeFrame) {
    // Special case: if start == end, we're looking for points at exactly one time frame
    if (start == end) {
        auto points = m_pointData->getAtTime(start, target_timeFrame, m_timeFrame.get());
        std::vector<float> componentValues;
        componentValues.reserve(points.size());
        
        for (auto const & point: points) {
            float componentValue = (m_component == Component::X) ? point.x : point.y;
            componentValues.push_back(componentValue);
        }
        return componentValues;
    }
    
    // For ranges with different start and end, use the range view
    auto point_range = m_pointData->GetPointsInRange(TimeFrameInterval(start, end),
                                                     target_timeFrame,
                                                     m_timeFrame.get());
    
    auto componentValues = std::vector<float>();

    for (auto const & time_point_pair: point_range) {
        // Check if there are points at this time
        if (time_point_pair.points.empty()) {
            continue;
        }
        
        // Add all points at this time frame
        for (auto const & point: time_point_pair.points) {
            float componentValue = (m_component == Component::X) ? point.x : point.y;
            componentValues.push_back(componentValue);
        }
    }
    
    return componentValues;
}

void PointComponentAdapter::materializeData() {
    if (m_isMaterialized) {
        return;
    }

    // Reserve space for efficiency
    m_materializedData.clear();
    m_materializedData.reserve(size());

    // Collect all points in time order and extract the specified component
    std::vector<std::pair<TimeFrameIndex, std::vector<Point2D<float>> const *>> timePointPairs;

    for (auto const & [time, points]: m_pointData->GetAllPointsAsRange()) {
        timePointPairs.emplace_back(time, &points);
    }

    // Sort by time to ensure consistent ordering
    std::sort(timePointPairs.begin(), timePointPairs.end(),
              [](auto const & a, auto const & b) {
                  return a.first < b.first;
              });

    // Extract the component values
    for (auto const & [time, points]: timePointPairs) {
        for (auto const & point: *points) {
            double componentValue = (m_component == Component::X) ? static_cast<double>(point.x) : static_cast<double>(point.y);
            m_materializedData.push_back(componentValue);
        }
    }

    m_isMaterialized = true;
}

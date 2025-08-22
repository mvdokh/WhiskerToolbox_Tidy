#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/adapters/LineDataAdapter.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/computers/LineSamplingMultiComputer.h"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

#include <memory>
#include <vector>
#include <stdexcept>
#include <iostream>

/**
 * @brief Test fixture for multi-sample LineData integration tests
 * 
 * This fixture creates a DataManager with two LineData objects:
 * - SingleSampleLines: exactly one line per timestamp
 * - MultiSampleLines: multiple lines per timestamp at some times
 */
class MultiSampleLineDataFixture {
protected:
    MultiSampleLineDataFixture() {
        setupDataManager();
    }

    DataManager& getDataManager() { return m_dataManager; }

private:
    DataManager m_dataManager;

    void setupDataManager() {
        // Create a common timeframe
        std::vector<int> timeValues = {0, 1, 2, 3, 4};
        auto timeFrame = std::make_shared<TimeFrame>(timeValues);
        m_dataManager.setTime(TimeKey("test_time"), timeFrame);

        // Create single-sample line data (exactly one line per timestamp)
        auto singleSampleLines = std::make_shared<LineData>();
        singleSampleLines->setTimeFrame(timeFrame);
        
        // Add one line at each timestamp
        for (int t = 0; t < 5; ++t) {
            std::vector<Point2D<float>> points = {
                {static_cast<float>(t), 0.0f},
                {static_cast<float>(t + 1), 1.0f}
            };
            singleSampleLines->addAtTime(TimeFrameIndex(t), Line2D(points));
        }
        m_dataManager.setData<LineData>("SingleSampleLines", singleSampleLines, TimeKey("test_time"));

        // Create multi-sample line data (multiple lines at some timestamps)
        auto multiSampleLines = std::make_shared<LineData>();
        multiSampleLines->setTimeFrame(timeFrame);
        
        // t=0: one line
        {
            std::vector<Point2D<float>> points = {{0.0f, 0.0f}, {1.0f, 1.0f}};
            multiSampleLines->addAtTime(TimeFrameIndex(0), Line2D(points));
        }
        
        // t=1: three lines (multi-sample)
        {
            std::vector<Point2D<float>> points1 = {{1.0f, 0.0f}, {2.0f, 1.0f}};
            std::vector<Point2D<float>> points2 = {{1.0f, 1.0f}, {2.0f, 2.0f}};
            std::vector<Point2D<float>> points3 = {{1.0f, 2.0f}, {2.0f, 3.0f}};
            multiSampleLines->addAtTime(TimeFrameIndex(1), Line2D(points1));
            multiSampleLines->addAtTime(TimeFrameIndex(1), Line2D(points2));
            multiSampleLines->addAtTime(TimeFrameIndex(1), Line2D(points3));
        }
        
        // t=2: one line
        {
            std::vector<Point2D<float>> points = {{2.0f, 0.0f}, {3.0f, 1.0f}};
            multiSampleLines->addAtTime(TimeFrameIndex(2), Line2D(points));
        }
        
        // t=3: two lines (multi-sample)
        {
            std::vector<Point2D<float>> points1 = {{3.0f, 0.0f}, {4.0f, 1.0f}};
            std::vector<Point2D<float>> points2 = {{3.0f, 1.0f}, {4.0f, 2.0f}};
            multiSampleLines->addAtTime(TimeFrameIndex(3), Line2D(points1));
            multiSampleLines->addAtTime(TimeFrameIndex(3), Line2D(points2));
        }
        
        // t=4: one line
        {
            std::vector<Point2D<float>> points = {{4.0f, 0.0f}, {5.0f, 1.0f}};
            multiSampleLines->addAtTime(TimeFrameIndex(4), Line2D(points));
        }
        
        m_dataManager.setData<LineData>("MultiSampleLines", multiSampleLines, TimeKey("test_time"));

        // Create a third line data that also has multi-samples for conflict testing
        auto conflictMultiSampleLines = std::make_shared<LineData>();
        conflictMultiSampleLines->setTimeFrame(timeFrame);
        
        // t=0: two lines (multi-sample)
        {
            std::vector<Point2D<float>> points1 = {{0.0f, 10.0f}, {1.0f, 11.0f}};
            std::vector<Point2D<float>> points2 = {{0.0f, 12.0f}, {1.0f, 13.0f}};
            conflictMultiSampleLines->addAtTime(TimeFrameIndex(0), Line2D(points1));
            conflictMultiSampleLines->addAtTime(TimeFrameIndex(0), Line2D(points2));
        }
        
        // t=1,2,3,4: one line each
        for (int t = 1; t < 5; ++t) {
            std::vector<Point2D<float>> points = {
                {static_cast<float>(t), 10.0f},
                {static_cast<float>(t + 1), 11.0f}
            };
            conflictMultiSampleLines->addAtTime(TimeFrameIndex(t), Line2D(points));
        }
        
        m_dataManager.setData<LineData>("ConflictMultiSampleLines", conflictMultiSampleLines, TimeKey("test_time"));
    }
};

TEST_CASE_METHOD(MultiSampleLineDataFixture, "LineDataAdapter hasMultiSamples detection", "[LineDataAdapter][MultiSample][Detection]") {
    auto& dm = getDataManager();
    auto timeFrame = dm.getTime(TimeKey("test_time"));
    
    // Test single-sample line data
    auto singleSampleData = dm.getData<LineData>("SingleSampleLines");
    auto singleAdapter = std::make_shared<LineDataAdapter>(singleSampleData, timeFrame, "SingleSampleLines");
    
    SECTION("Single-sample data should not have multi-samples") {
        REQUIRE_FALSE(singleAdapter->hasMultiSamples());
    }
    
    // Test multi-sample line data
    auto multiSampleData = dm.getData<LineData>("MultiSampleLines");
    auto multiAdapter = std::make_shared<LineDataAdapter>(multiSampleData, timeFrame, "MultiSampleLines");
    
    SECTION("Multi-sample data should have multi-samples") {
        REQUIRE(multiAdapter->hasMultiSamples());
    }
    
    // Test conflict multi-sample line data
    auto conflictData = dm.getData<LineData>("ConflictMultiSampleLines");
    auto conflictAdapter = std::make_shared<LineDataAdapter>(conflictData, timeFrame, "ConflictMultiSampleLines");
    
    SECTION("Conflict data should have multi-samples") {
        REQUIRE(conflictAdapter->hasMultiSamples());
    }
}

TEST_CASE_METHOD(MultiSampleLineDataFixture, "TableViewBuilder allows single multi-sample source", "[TableViewBuilder][MultiSample][SingleSource]") {
    auto& dm = getDataManager();
    auto dme = std::make_shared<DataManagerExtension>(dm);
    auto timeFrame = dm.getTime(TimeKey("test_time"));
    
    // Create timestamp selector
    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, timeFrame);
    
    SECTION("Single-sample source works normally") {
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(rowSelector));
        
        auto singleSource = dme->getLineSource("SingleSampleLines");
        auto computer = std::make_unique<LineSamplingMultiComputer>(
            singleSource, "SingleSampleLines", timeFrame, 1);
        
        REQUIRE_NOTHROW(builder.addColumns<double>("SingleLine", std::move(computer)));
        REQUIRE_NOTHROW(builder.build());
    }
    
    SECTION("Multi-sample source works when it's the only multi-sample source") {
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(rowSelector));
        
        auto multiSource = dme->getLineSource("MultiSampleLines");
        auto computer = std::make_unique<LineSamplingMultiComputer>(
            multiSource, "MultiSampleLines", timeFrame, 1);
        
        REQUIRE_NOTHROW(builder.addColumns<double>("MultiLine", std::move(computer)));
        
        // Building should succeed since we have only one multi-sample source
        auto table = builder.build();
        
        // Verify the table was created successfully
        REQUIRE(table.getColumnCount() > 0);
        REQUIRE(table.getRowCount() > 0);
    }
}

TEST_CASE_METHOD(MultiSampleLineDataFixture, "TableViewBuilder rejects multiple multi-sample sources", "[TableViewBuilder][MultiSample][Conflict]") {
    auto& dm = getDataManager();
    auto dme = std::make_shared<DataManagerExtension>(dm);
    auto timeFrame = dm.getTime(TimeKey("test_time"));
    
    // Create timestamp selector
    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, timeFrame);
    
    TableViewBuilder builder(dme);
    builder.setRowSelector(std::move(rowSelector));
    
    // Add first multi-sample source
    auto multiSource1 = dme->getLineSource("MultiSampleLines");
    auto computer1 = std::make_unique<LineSamplingMultiComputer>(
        multiSource1, "MultiSampleLines", timeFrame, 1);
    
    REQUIRE_NOTHROW(builder.addColumns<double>("MultiLine1", std::move(computer1)));
    
    // Add second multi-sample source - this should cause build() to fail
    auto multiSource2 = dme->getLineSource("ConflictMultiSampleLines");
    auto computer2 = std::make_unique<LineSamplingMultiComputer>(
        multiSource2, "ConflictMultiSampleLines", timeFrame, 1);
    
    REQUIRE_NOTHROW(builder.addColumns<double>("MultiLine2", std::move(computer2)));
    
    SECTION("Building with multiple multi-sample sources should throw") {
        REQUIRE_THROWS_AS(builder.build(), std::runtime_error);
        
        // Verify the error message is informative
        try {
            builder.build();
            FAIL("Expected exception was not thrown");
        } catch (std::runtime_error const& e) {
            std::string errorMsg = e.what();
            REQUIRE(errorMsg.find("multiple multi-sample") != std::string::npos);
            REQUIRE(errorMsg.find("MultiSampleLines") != std::string::npos);
            REQUIRE(errorMsg.find("ConflictMultiSampleLines") != std::string::npos);
        }
    }
}

TEST_CASE_METHOD(MultiSampleLineDataFixture, "TableViewBuilder allows mixed single and multi-sample sources", "[TableViewBuilder][MultiSample][Mixed]") {
    auto& dm = getDataManager();
    auto dme = std::make_shared<DataManagerExtension>(dm);
    auto timeFrame = dm.getTime(TimeKey("test_time"));
    
    // Create timestamp selector
    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, timeFrame);
    
    TableViewBuilder builder(dme);
    builder.setRowSelector(std::move(rowSelector));
    
    // Add single-sample source
    auto singleSource = dme->getLineSource("SingleSampleLines");
    auto computer1 = std::make_unique<LineSamplingMultiComputer>(
        singleSource, "SingleSampleLines", timeFrame, 1);
    
    REQUIRE_NOTHROW(builder.addColumns<double>("SingleLine", std::move(computer1)));
    
    // Add multi-sample source
    auto multiSource = dme->getLineSource("MultiSampleLines");
    auto computer2 = std::make_unique<LineSamplingMultiComputer>(
        multiSource, "MultiSampleLines", timeFrame, 1);
    
    REQUIRE_NOTHROW(builder.addColumns<double>("MultiLine", std::move(computer2)));
    
    SECTION("Mixed sources should work") {
        auto table = builder.build();
        
        // Verify the table was created successfully
        REQUIRE(table.getColumnCount() > 0);
        REQUIRE(table.getRowCount() > 0);
        
        // The table should have expanded rows for the multi-sample source
        // At t=1, MultiSampleLines has 3 lines, so we expect expanded rows
        REQUIRE(table.getRowCount() >= 3); // At least 3 rows for t=1 expansion
    }
}

TEST_CASE_METHOD(MultiSampleLineDataFixture, "Multi-sample expansion generates correct row count", "[TableViewBuilder][MultiSample][Expansion]") {
    auto& dm = getDataManager();
    auto dme = std::make_shared<DataManagerExtension>(dm);
    auto timeFrame = dm.getTime(TimeKey("test_time"));
    
    // Create timestamp selector for times with different numbers of lines
    std::vector<TimeFrameIndex> timestamps = {
        TimeFrameIndex(0), // 1 line
        TimeFrameIndex(1), // 3 lines (multi-sample)
        TimeFrameIndex(2), // 1 line
        TimeFrameIndex(3)  // 2 lines (multi-sample)
    };
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, timeFrame);
    
    TableViewBuilder builder(dme);
    builder.setRowSelector(std::move(rowSelector));
    
    auto multiSource = dme->getLineSource("MultiSampleLines");
    auto computer = std::make_unique<LineSamplingMultiComputer>(
        multiSource, "MultiSampleLines", timeFrame, 1);
    
    builder.addColumns<double>("MultiLine", std::move(computer));
    
    TableView table = builder.build();
    
    SECTION("Row count should equal total number of lines across selected timestamps") {
        // Expected: 1 (t=0) + 3 (t=1) + 1 (t=2) + 2 (t=3) = 7 rows
        REQUIRE(table.getRowCount() == 7);
    }
    
    SECTION("Column values should be correctly expanded") {
        // Test that we can access all columns
        REQUIRE(table.hasColumn("MultiLine.x@0.000"));
        REQUIRE(table.hasColumn("MultiLine.y@0.000"));
        REQUIRE(table.hasColumn("MultiLine.x@1.000"));
        REQUIRE(table.hasColumn("MultiLine.y@1.000"));
        
        auto xStart = table.getColumnValues<double>("MultiLine.x@0.000");
        auto yStart = table.getColumnValues<double>("MultiLine.y@0.000");
        
        REQUIRE(xStart.size() == 7);
        REQUIRE(yStart.size() == 7);
        
        // Verify some expected values based on our test data
        // t=0: line from (0,0) to (1,1)
        REQUIRE(xStart[0] == Catch::Approx(0.0));
        REQUIRE(yStart[0] == Catch::Approx(0.0));
        
        // t=1: first line from (1,0) to (2,1)
        REQUIRE(xStart[1] == Catch::Approx(1.0));
        REQUIRE(yStart[1] == Catch::Approx(0.0));
        
        // t=1: second line from (1,1) to (2,2)
        REQUIRE(xStart[2] == Catch::Approx(1.0));
        REQUIRE(yStart[2] == Catch::Approx(1.0));
        
        // t=1: third line from (1,2) to (2,3)
        REQUIRE(xStart[3] == Catch::Approx(1.0));
        REQUIRE(yStart[3] == Catch::Approx(2.0));
    }
}

TEST_CASE_METHOD(MultiSampleLineDataFixture, "Error messages are informative for debugging", "[TableViewBuilder][MultiSample][ErrorMessages]") {
    auto& dm = getDataManager();
    auto dme = std::make_shared<DataManagerExtension>(dm);
    auto timeFrame = dm.getTime(TimeKey("test_time"));
    
    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, timeFrame);
    
    TableViewBuilder builder(dme);
    builder.setRowSelector(std::move(rowSelector));
    
    // Add multiple multi-sample sources
    auto multiSource1 = dme->getLineSource("MultiSampleLines");
    auto computer1 = std::make_unique<LineSamplingMultiComputer>(
        multiSource1, "MultiSampleLines", timeFrame, 1);
    builder.addColumns<double>("MultiLine1", std::move(computer1));
    
    auto multiSource2 = dme->getLineSource("ConflictMultiSampleLines");
    auto computer2 = std::make_unique<LineSamplingMultiComputer>(
        multiSource2, "ConflictMultiSampleLines", timeFrame, 1);
    builder.addColumns<double>("MultiLine2", std::move(computer2));
    
    SECTION("Error message contains source names and guidance") {
        try {
            builder.build();
            FAIL("Expected exception was not thrown");
        } catch (std::runtime_error const& e) {
            std::string errorMsg = e.what();
            
            // Check that error message contains key information
            REQUIRE(errorMsg.find("Cannot build TableView") != std::string::npos);
            REQUIRE(errorMsg.find("multiple multi-sample") != std::string::npos);
            REQUIRE(errorMsg.find("Entity expansion is undefined") != std::string::npos);
            REQUIRE(errorMsg.find("MultiSampleLines") != std::string::npos);
            REQUIRE(errorMsg.find("ConflictMultiSampleLines") != std::string::npos);
            
            // Should provide guidance on how to fix
            REQUIRE(errorMsg.find("ensure only one") != std::string::npos);
            
            std::cout << "Error message: " << errorMsg << std::endl;
        }
    }
}

TEST_CASE_METHOD(MultiSampleLineDataFixture, "Table view construction with no multi-sample sources", "[TableViewBuilder][MultiSample][NoMultiSample]") {
    auto& dm = getDataManager();
    auto dme = std::make_shared<DataManagerExtension>(dm);
    auto timeFrame = dm.getTime(TimeKey("test_time"));
    
    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, timeFrame);
    
    TableViewBuilder builder(dme);
    builder.setRowSelector(std::move(rowSelector));
    
    // Add only single-sample sources
    auto singleSource = dme->getLineSource("SingleSampleLines");
    auto computer = std::make_unique<LineSamplingMultiComputer>(
        singleSource, "SingleSampleLines", timeFrame, 1);
    
    builder.addColumns<double>("SingleLine", std::move(computer));
    
    SECTION("No multi-sample sources should work normally") {
        auto table = builder.build();
        
        // Should have exactly as many rows as timestamps (no expansion)
        REQUIRE(table.getRowCount() == 3);
        
        // Columns should be present and accessible
        REQUIRE(table.hasColumn("SingleLine.x@0.000"));
        REQUIRE(table.hasColumn("SingleLine.y@0.000"));
        REQUIRE(table.hasColumn("SingleLine.x@1.000"));
        REQUIRE(table.hasColumn("SingleLine.y@1.000"));
    }
}

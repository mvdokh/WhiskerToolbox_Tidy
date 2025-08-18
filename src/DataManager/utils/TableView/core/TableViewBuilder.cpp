#include "TableViewBuilder.h"

#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IMultiColumnComputer.h"
#include "utils/TableView/interfaces/IRowSelector.h"

#include <stdexcept>

TableViewBuilder::~TableViewBuilder() = default;

TableViewBuilder::TableViewBuilder(std::shared_ptr<DataManagerExtension> dataManager)
    : m_dataManager(std::move(dataManager)) {
    if (!m_dataManager) {
        throw std::invalid_argument("DataManagerExtension cannot be null");
    }
}

TableViewBuilder & TableViewBuilder::setRowSelector(std::unique_ptr<IRowSelector> rowSelector) {
    if (!rowSelector) {
        throw std::invalid_argument("IRowSelector cannot be null");
    }
    m_rowSelector = std::move(rowSelector);
    return *this;
}

TableViewBuilder & TableViewBuilder::addColumn(std::string const & name, std::unique_ptr<IColumnComputer<double>> computer) {
    if (name.empty()) {
        throw std::invalid_argument("Column name cannot be empty");
    }
    if (!computer) {
        throw std::invalid_argument("IColumnComputer cannot be null");
    }

    // Check for duplicate names
    for (auto const & column: m_columns) {
        if (column->getName() == name) {
            throw std::runtime_error("Column '" + name + "' already exists");
        }
    }

    // Create the double column
    auto column = std::shared_ptr<IColumn>(new Column<double>(name, std::move(computer)));
    m_columns.push_back(std::move(column));

    return *this;
}

TableView TableViewBuilder::build() {
    // Validate configuration
    if (!m_rowSelector) {
        throw std::runtime_error("Row selector must be set before building");
    }
    if (m_columns.empty()) {
        throw std::runtime_error("At least one column must be added before building");
    }

    // Create the TableView
    TableView tableView(std::move(m_rowSelector), m_dataManager);

    // Add columns to the table view
    for (auto & column: m_columns) {
        tableView.addColumn(std::move(column));
    }

    // Clear our state since we've moved everything
    m_columns.clear();
    m_rowSelector.reset();

    return tableView;
}

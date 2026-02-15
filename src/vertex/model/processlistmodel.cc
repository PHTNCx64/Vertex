//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <unordered_set>
#include <fmt/format.h>

#include <vertex/utility.hh>
#include <vertex/model/processlistmodel.hh>
#include <vertex/runtime/caller.hh>

namespace Vertex::Model
{
    void ProcessListModel::set_selected_process(const Class::SelectedProcess& process)
    {
        std::scoped_lock<std::shared_mutex> lock(m_stateMutex);
        m_selectedProcess = process;
    }

    void ProcessListModel::set_filter_type(const Enums::FilterType filter)
    {
        std::scoped_lock<std::shared_mutex> lock(m_stateMutex);
        if (m_filterType != filter)
        {
            m_filterType = filter;
            m_filterDirty.store(true, std::memory_order_release);
        }
    }

    void ProcessListModel::set_filter_text(const std::string_view text)
    {
        std::scoped_lock<std::shared_mutex> lock(m_stateMutex);
        if (m_filterText != text)
        {
            m_filterText = std::string{text};
            m_filterDirty.store(true, std::memory_order_release);
        }
    }

    StatusCode ProcessListModel::open_process() const
    {
        if (m_selectedProcess.get_selected_process_id().has_value())
        {
            if (!m_loaderService.get_active_plugin().has_value())
            {
                m_loggerService.log_error(fmt::format("{}: {}", MODEL_NAME, "No active plugin set. Cannot proceed."));
                return StatusCode::STATUS_ERROR_PLUGIN_NOT_FOUND;
            }

            const auto& activePlugin = m_loaderService.get_active_plugin().value().get();

            const std::uint32_t processId = m_selectedProcess.get_selected_process_id().value();
            const auto result = Runtime::safe_call(activePlugin.internal_vertex_process_open, processId);
            const auto status = Runtime::get_status(result);
            if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
            {
                m_loggerService.log_error(fmt::format("{}: {}", MODEL_NAME, "vertex_open_process is not implemented by plugin!"));
                return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
            }
            if (!Runtime::status_ok(result))
            {
                m_loggerService.log_error(fmt::format("{}: {} {}", MODEL_NAME, "vertex_open_process failed with STATUS_CODE: ", static_cast<int>(status)));
                return status;
            }

            ProcessEventData eventData{};
            eventData.processId = processId;
            eventData.processHandle = nullptr;
            m_loaderService.dispatch_event(VERTEX_PROCESS_OPENED, &eventData);

            return StatusCode::STATUS_OK;
        }
        return StatusCode::STATUS_ERROR_PROCESS_INVALID;
    }

    std::size_t ProcessListModel::get_processes_count() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(m_stateMutex);
        return m_shouldFilter.load(std::memory_order_acquire) ? m_filteredIndices.size() : m_processes.size();
    }

    void ProcessListModel::set_should_filter(const bool shouldFilter) noexcept
    {
        bool expected = !shouldFilter;
        if (m_shouldFilter.compare_exchange_strong(expected, shouldFilter, std::memory_order_acq_rel))
        {
            m_filterDirty.store(true, std::memory_order_release);
        }
    }

    bool ProcessListModel::get_should_filter() const noexcept
    {
        return m_shouldFilter.load(std::memory_order_acquire);
    }

    void ProcessListModel::clear_selected_process() noexcept
    {
        std::shared_lock<std::shared_mutex> lock(m_stateMutex);
        m_selectedProcess.set_selected_process_id(0);
        m_selectedProcess.set_selected_process_name({});
    }

    std::string ProcessListModel::get_process_item(const long item, const long col) const
    {
        std::shared_lock<std::shared_mutex> lock(m_stateMutex);

        std::size_t actualIndex = item;
        if (m_shouldFilter.load(std::memory_order_acquire))
        {
            if (item < 0 || static_cast<std::size_t>(item) >= m_filteredIndices.size())
            {
                return EMPTY_STRING;
            }
            actualIndex = m_filteredIndices[item];
        }
        else
        {
            if (item < 0 || static_cast<std::size_t>(item) >= m_processes.size())
            {
                return EMPTY_STRING;
            }
            if (!m_sortIndices.empty() && static_cast<std::size_t>(item) < m_sortIndices.size())
            {
                actualIndex = m_sortIndices[item];
            }
        }

        const auto& [processName, processOwner, processId] = m_processes[actualIndex];

        switch (col)
        {
        case PROCESS_ID_COLUMN:
            return fmt::format("{}", processId);
        case PROCESS_NAME_COLUMN:
            return fmt::format("{}", processName);
        case PROCESS_OWNER_COLUMN:
            return fmt::format("{}", processOwner);
        [[unlikely]] default:
            return EMPTY_STRING;
        }
    }

    StatusCode ProcessListModel::get_process_list()
    {
        auto result = m_loaderService.has_plugin_loaded();
        if (result != StatusCode::STATUS_OK)
        {
            return result;
        }

        const auto& activePlugin = m_loaderService.get_active_plugin().value().get();

        std::vector<ProcessInformation> safeProcesses{};
        safeProcesses.reserve(INITIAL_PROCESS_LIST_SIZE);

        std::uint32_t processCount{};

        const auto processCountResult = Runtime::safe_call(activePlugin.internal_vertex_process_get_list, nullptr, &processCount);
        result = Runtime::get_status(processCountResult);
        if (!Runtime::status_ok(result))
        {
            safeProcesses.clear();
        }
        else
        {
            safeProcesses.resize(processCount);

            ProcessInformation* buffer = safeProcesses.data();
            std::uint32_t bufferSize = processCount;

            auto listResult = Runtime::safe_call(activePlugin.internal_vertex_process_get_list, &buffer, &bufferSize);
            result = Runtime::get_status(listResult);

            if (Runtime::status_ok(listResult))
            {
                safeProcesses.resize(bufferSize);
            }
            else if (result == StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL)
            {
                safeProcesses.resize(bufferSize);
                buffer = safeProcesses.data();
                listResult = Runtime::safe_call(activePlugin.internal_vertex_process_get_list, &buffer, &bufferSize);
                result = Runtime::get_status(listResult);

                if (!Runtime::status_ok(result))
                {
                    return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
                }
            }
            else
            {
                return result;
            }
        }

        {
            std::scoped_lock<std::shared_mutex> lock(m_stateMutex);
            m_processes = std::move(safeProcesses);
            invalidate_cache();
            m_filterDirty.store(true, std::memory_order_release);
        }

        return StatusCode::STATUS_OK;
    }

    void ProcessListModel::sort_list()
    {
        std::scoped_lock<std::shared_mutex> lock(m_stateMutex);

        if (m_shouldFilter.load(std::memory_order_acquire))
        {
            std::ranges::sort(m_filteredIndices,
                              [this](std::size_t a, std::size_t b)
                              {
                                  return compare_processes(a, b);
                              });
        }
        else
        {
            m_sortIndices.resize(m_processes.size());
            std::iota(m_sortIndices.begin(), m_sortIndices.end(), 0);

            std::ranges::sort(m_sortIndices,
                              [this](std::size_t a, std::size_t b)
                              {
                                  return compare_processes(a, b);
                              });
        }
    }

    void ProcessListModel::filter_list()
    {
        if (!m_shouldFilter.load(std::memory_order_acquire))
        {
            return;
        }

        if (!m_filterDirty.exchange(false, std::memory_order_acq_rel))
        {
            return;
        }

        std::string filterText{};
        Enums::FilterType filterType{};
        {
            std::shared_lock<std::shared_mutex> lock(m_stateMutex);
            filterText = m_filterText;
            filterType = m_filterType;
        }

        std::string lowercaseFilterText = filterText;
        std::ranges::transform(lowercaseFilterText, lowercaseFilterText.begin(), ::tolower);

        std::optional<std::boyer_moore_searcher<std::string::const_iterator>> searcher;
        if (!lowercaseFilterText.empty())
        {
            searcher.emplace(lowercaseFilterText.begin(), lowercaseFilterText.end());
        }

        {
            std::scoped_lock<std::shared_mutex> lock(m_stateMutex);

            m_filteredIndices = std::views::iota(std::size_t{0}, m_processes.size())
                | std::views::filter([this, &filterType, &lowercaseFilterText, &searcher](std::size_t i) {
                    return should_keep_processes(m_processes[i], filterType, lowercaseFilterText, searcher);
                })
                | std::ranges::to<std::vector>();
        }
    }

    bool ProcessListModel::compare_processes(std::size_t a, std::size_t b) const
    {
        switch (m_selectedColumn)
        {
        case PROCESS_ID_COLUMN:
            return m_sortOrder == Enums::SortOrder::ASCENDING ? m_processes[a].processId < m_processes[b].processId : m_processes[a].processId > m_processes[b].processId;
        case PROCESS_NAME_COLUMN:
        {
            const int cmp = std::strcmp(m_processes[a].processName, m_processes[b].processName);
            return m_sortOrder == Enums::SortOrder::ASCENDING ? cmp < 0 : cmp > 0;
        }
        case PROCESS_OWNER_COLUMN:
        {
            const int cmp = std::strcmp(m_processes[a].processOwner, m_processes[b].processOwner);
            return m_sortOrder == Enums::SortOrder::ASCENDING ? cmp < 0 : cmp > 0;
        }
        default:
            return false;
        }
    }

    bool ProcessListModel::should_keep_processes(const ProcessInformation& process,
                                                 const Enums::FilterType filterType,
                                                 const std::string& lowercaseFilterText,
                                                 const std::optional<std::boyer_moore_searcher<std::string::const_iterator>>& searcher)
    {
        if (lowercaseFilterText.empty())
        {
            return true;
        }

        auto& [lowercaseName, lowercaseOwner, idString, dirty] = m_processCache[&process];
        if (dirty)
        {
            lowercaseName = process.processName;
            std::ranges::transform(lowercaseName, lowercaseName.begin(), ::tolower);

            lowercaseOwner = process.processOwner;
            std::ranges::transform(lowercaseOwner, lowercaseOwner.begin(), ::tolower);

            idString = std::to_string(process.processId);
            dirty = false;
        }

        const std::string* searchString = nullptr;
        switch (filterType)
        {
        case Enums::FilterType::PROCESSNAME:
            searchString = &lowercaseName;
            break;
        case Enums::FilterType::PROCESSID:
            searchString = &idString;
            break;
        case Enums::FilterType::PROCESSOWNER:
            searchString = &lowercaseOwner;
            break;
        default:
            return true;
        }

        if (searcher && searchString)
        {
            return std::search(searchString->begin(), searchString->end(), *searcher) != searchString->end();
        }

        return searchString && searchString->find(lowercaseFilterText) != std::string::npos;
    }

    Class::SelectedProcess ProcessListModel::make_selected_process_from_id(const long index) noexcept
    {
        std::scoped_lock<std::shared_mutex> lock(m_stateMutex);

        std::size_t actualIndex = index;
        if (m_shouldFilter.load(std::memory_order_acquire))
        {
            if (index >= 0 && static_cast<std::size_t>(index) < m_filteredIndices.size())
            {
                actualIndex = m_filteredIndices[index];
            }
            else
            {
                m_selectedProcess.set_selected_process_id(0);
                m_selectedProcess.set_selected_process_name({});
                return m_selectedProcess;
            }
        }
        else
        {
            if (index < 0 || static_cast<std::size_t>(index) >= m_processes.size())
            {
                m_selectedProcess.set_selected_process_id(0);
                m_selectedProcess.set_selected_process_name({});
                return m_selectedProcess;
            }
            if (!m_sortIndices.empty() && static_cast<std::size_t>(index) < m_sortIndices.size())
            {
                actualIndex = m_sortIndices[index];
            }
        }

        m_selectedProcess.set_selected_process_id(m_processes[actualIndex].processId);
        m_selectedProcess.set_selected_process_name(m_processes[actualIndex].processName);
        return m_selectedProcess;
    }

    void ProcessListModel::invalidate_cache()
    {
        for (auto& cache : m_processCache | std::views::values)
        {
            cache.dirty = true;
        }
        if (m_processCache.size() > m_processes.size() * 2)
        {
            auto validPointers = m_processes
                | std::views::transform([](const auto& proc) { return &proc; })
                | std::ranges::to<std::unordered_set>();

            std::erase_if(m_processCache,
                          [&validPointers](const auto& item)
                          {
                              return !validPointers.contains(item.first);
                          });
        }
    }

    void ProcessListModel::set_clicked_column(const long col) noexcept
    {
        std::scoped_lock<std::shared_mutex> lock(m_stateMutex);
        m_selectedColumn = col;
    }

    long ProcessListModel::get_clicked_column() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(m_stateMutex);
        return m_selectedColumn;
    }

    void ProcessListModel::set_sort_order(const Enums::SortOrder sortOrder)
    {
        std::scoped_lock<std::shared_mutex> lock(m_stateMutex);
        m_sortOrder = sortOrder;
    }

    Class::SelectedProcess ProcessListModel::get_selected_process() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(m_stateMutex);
        return m_selectedProcess;
    }

    Enums::FilterType ProcessListModel::get_filter_type() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(m_stateMutex);
        return m_filterType;
    }

    std::string ProcessListModel::get_filter_text() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(m_stateMutex);
        return m_filterText;
    }

    Enums::SortOrder ProcessListModel::get_sort_order() const noexcept
    {
        std::shared_lock<std::shared_mutex> lock(m_stateMutex);
        return m_sortOrder;
    }

    int ProcessListModel::get_ui_state_int(const std::string_view key, const int defaultValue) const
    {
        const std::string keyStr{key};
        if (!m_settingsService.get_bool("general.guiSavingEnabled", true))
        {
            return defaultValue;
        }
        return m_settingsService.get_int(keyStr, defaultValue);
    }

    void ProcessListModel::set_ui_state_int(const std::string_view key, const int value) const
    {
        const std::string keyStr{key};
        if (m_settingsService.get_bool("general.guiSavingEnabled", true))
        {
            m_settingsService.set_value(keyStr, value);
        }
    }
}

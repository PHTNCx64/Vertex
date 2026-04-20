//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/accesstrackerdatamodel.hh>

#include <algorithm>
#include <string>

#include <fmt/format.h>

#include <vertex/utility.hh>

namespace Vertex::CustomWidgets
{
    namespace
    {
        constexpr std::string_view PLACEHOLDER{"\xE2\x80\xA6"};

        [[nodiscard]] wxString access_glyph(Debugger::WatchpointType type)
        {
            switch (type)
            {
            case Debugger::WatchpointType::Read:      return "R";
            case Debugger::WatchpointType::Write:     return "W";
            case Debugger::WatchpointType::Execute:   return "X";
            case Debugger::WatchpointType::ReadWrite: return "RW";
            }
            return "RW";
        }

        [[nodiscard]] std::string build_registers_preview(const Debugger::RegisterSet& registers)
        {
            if (registers.generalPurpose.empty())
            {
                return {};
            }
            std::string preview{};
            preview.reserve(64);
            const auto limit = std::min(
                registers.generalPurpose.size(),
                static_cast<std::size_t>(AccessTrackerValues::REGISTERS_PREVIEW_LIMIT));
            for (std::size_t i{}; i < limit; ++i)
            {
                const auto& reg = registers.generalPurpose[i];
                if (i > 0)
                {
                    preview.append(" ");
                }
                preview.append(fmt::format("{}=0x{:X}", reg.name, reg.value));
            }
            return preview;
        }

        [[nodiscard]] const Debugger::StackFrame& frame_at(const ViewModel::AccessEntry& entry,
                                                           const std::size_t index) noexcept
        {
            static const Debugger::StackFrame EMPTY{};
            if (index < entry.lastCallStack.size())
            {
                return entry.lastCallStack[index];
            }
            return EMPTY;
        }
    }

    AccessTrackerDataModel::AccessTrackerDataModel(const ViewModel::AccessTrackerViewModel& viewModel)
        : wxDataViewVirtualListModel(0)
        , m_viewModel(viewModel)
    {
    }

    void AccessTrackerDataModel::refresh_from_viewmodel()
    {
        m_snapshot = m_viewModel.snapshot_entries();
        Reset(static_cast<unsigned int>(m_snapshot.size()));
    }

    unsigned int AccessTrackerDataModel::GetColumnCount() const
    {
        return COLUMN_COUNT;
    }

    wxString AccessTrackerDataModel::GetColumnType([[maybe_unused]] const unsigned int col) const
    {
        return "string";
    }

    void AccessTrackerDataModel::GetValueByRow(wxVariant& variant, const unsigned int row, const unsigned int col) const
    {
        if (row >= m_snapshot.size())
        {
            variant = wxString{};
            return;
        }

        const auto& entry = m_snapshot[row];
        const auto& topFrame = frame_at(entry, 0);
        const auto& callerFrame = frame_at(entry, 1);

        switch (col)
        {
        case INSTRUCTION_COL:
            variant = wxString::FromUTF8(fmt::format("0x{:X}", entry.instructionAddress));
            break;
        case MODULE_COL:
            variant = topFrame.moduleName.empty()
                ? wxString::FromUTF8(PLACEHOLDER)
                : wxString::FromUTF8(topFrame.moduleName);
            break;
        case FUNCTION_COL:
            variant = topFrame.functionName.empty()
                ? wxString::FromUTF8(PLACEHOLDER)
                : wxString::FromUTF8(topFrame.functionName);
            break;
        case MNEMONIC_COL:
            variant = entry.lastMnemonic.empty()
                ? wxString::FromUTF8(PLACEHOLDER)
                : wxString::FromUTF8(entry.lastMnemonic);
            break;
        case HITS_COL:
            variant = wxString::FromUTF8(std::to_string(entry.hitCount));
            break;
        case ACCESS_COL:
            variant = access_glyph(entry.lastAccessType);
            break;
        case SIZE_COL:
            variant = wxString::FromUTF8(std::to_string(static_cast<unsigned>(entry.accessSize)));
            break;
        case REGISTERS_COL:
        {
            const auto preview = build_registers_preview(entry.lastRegisters);
            variant = preview.empty()
                ? wxString::FromUTF8(PLACEHOLDER)
                : wxString::FromUTF8(preview);
            break;
        }
        case CALLER_COL:
            variant = callerFrame.functionName.empty()
                ? wxString::FromUTF8(PLACEHOLDER)
                : wxString::FromUTF8(callerFrame.functionName);
            break;
        default:
            variant = wxString{};
            break;
        }
    }

    bool AccessTrackerDataModel::GetAttrByRow(const unsigned int row, const unsigned int col, wxDataViewItemAttr& attr) const
    {
        if (row >= m_snapshot.size())
        {
            return false;
        }

        const auto& entry = m_snapshot[row];
        const auto& topFrame = frame_at(entry, 0);
        const auto& callerFrame = frame_at(entry, 1);

        const auto setPlaceholder = [&](const bool empty, const wxColour& normal)
        {
            attr.SetColour(empty ? m_colorPlaceholder : normal);
            return true;
        };

        switch (col)
        {
        case INSTRUCTION_COL:
            attr.SetColour(m_colorInstruction);
            return true;
        case MODULE_COL:
            return setPlaceholder(topFrame.moduleName.empty(), m_colorModule);
        case FUNCTION_COL:
            return setPlaceholder(topFrame.functionName.empty(), m_colorFunction);
        case MNEMONIC_COL:
            return setPlaceholder(entry.lastMnemonic.empty(), m_colorMnemonic);
        case HITS_COL:
            attr.SetColour(m_colorHits);
            return true;
        case ACCESS_COL:
            attr.SetColour(m_colorAccess);
            return true;
        case SIZE_COL:
            attr.SetColour(m_colorSize);
            return true;
        case REGISTERS_COL:
            return setPlaceholder(entry.lastRegisters.generalPurpose.empty(), m_colorRegisters);
        case CALLER_COL:
            return setPlaceholder(callerFrame.functionName.empty(), m_colorCaller);
        default:
            return false;
        }
    }

    bool AccessTrackerDataModel::SetValueByRow([[maybe_unused]] const wxVariant& variant,
                                                [[maybe_unused]] const unsigned int row,
                                                [[maybe_unused]] const unsigned int col)
    {
        return false;
    }
}

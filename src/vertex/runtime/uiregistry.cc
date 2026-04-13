//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/runtime/uiregistry.hh>

#include <algorithm>
#include <ranges>
#include <span>

namespace Vertex::Runtime
{
    UIRegistry::OwnedPanel UIRegistry::copy_panel(const UIPanel& panel)
    {
        OwnedPanel owned{};
        std::ranges::copy(panel.panelId, owned.panelId);
        std::ranges::copy(panel.title, owned.title);
        owned.onApply = panel.onApply;
        owned.onReset = panel.onReset;
        owned.userData = panel.userData;

        const auto srcSections = std::span{panel.sections, panel.sectionCount};
        owned.sections.reserve(srcSections.size());

        for (const auto& srcSection : srcSections)
        {
            OwnedSection ownedSection{};
            std::ranges::copy(srcSection.title, ownedSection.title);

            auto srcFields = std::span{srcSection.fields, srcSection.fieldCount};
            ownedSection.fields.reserve(srcFields.size());

            for (const auto& srcField : srcFields)
            {
                OwnedField ownedField{};
                ownedField.header = srcField;
                ownedField.header.options = nullptr;
                ownedField.options.assign(srcField.options, srcField.options + srcField.optionCount);

                ownedSection.fields.push_back(std::move(ownedField));
            }

            owned.sections.push_back(std::move(ownedSection));
        }

        return owned;
    }

    PanelSnapshot UIRegistry::build_snapshot(const OwnedPanel& owned)
    {
        PanelSnapshot snapshot{};
        std::ranges::copy(owned.panelId, snapshot.panel.panelId);
        std::ranges::copy(owned.title, snapshot.panel.title);
        snapshot.panel.onApply = owned.onApply;
        snapshot.panel.onReset = owned.onReset;
        snapshot.panel.userData = owned.userData;

        std::size_t totalOptions{};
        std::size_t totalFields{};
        for (const auto& section : owned.sections)
        {
            totalFields += section.fields.size();
            for (const auto& field : section.fields)
            {
                totalOptions += field.options.size();
            }
        }

        snapshot.options.reserve(totalOptions);
        snapshot.fields.reserve(totalFields);
        snapshot.sections.reserve(owned.sections.size());

        for (const auto& ownedSection : owned.sections)
        {
            for (const auto& ownedField : ownedSection.fields)
            {
                snapshot.options.insert(snapshot.options.end(), ownedField.options.begin(), ownedField.options.end());
            }
        }

        std::uint32_t optionOffset{};
        for (const auto& ownedSection : owned.sections)
        {
            for (const auto& ownedField : ownedSection.fields)
            {
                UIField field = ownedField.header;
                field.options = snapshot.options.data() + optionOffset;
                field.optionCount = static_cast<std::uint32_t>(ownedField.options.size());
                snapshot.fields.push_back(field);
                optionOffset += field.optionCount;
            }
        }

        std::uint32_t fieldOffset{};
        for (const auto& ownedSection : owned.sections)
        {
            UISection section{};
            std::ranges::copy(ownedSection.title, section.title);
            section.fields = snapshot.fields.data() + fieldOffset;
            section.fieldCount = static_cast<std::uint32_t>(ownedSection.fields.size());
            snapshot.sections.push_back(section);
            fieldOffset += section.fieldCount;
        }

        snapshot.panel.sections = snapshot.sections.data();
        snapshot.panel.sectionCount = static_cast<std::uint32_t>(snapshot.sections.size());

        return snapshot;
    }

    StatusCode UIRegistry::register_panel(const UIPanel& panel)
    {
        std::scoped_lock lock{m_mutex};

        auto owned = copy_panel(panel);
        std::string panelId{owned.panelId};

        auto& panelValues = m_values[panelId];
        for (const auto& section : owned.sections)
        {
            for (const auto& field : section.fields)
            {
                std::string fieldId{field.header.fieldId};
                if (!panelValues.contains(fieldId))
                {
                    panelValues[fieldId] = field.header.defaultValue;
                }
            }
        }

        m_panels[panelId] = std::move(owned);
        return StatusCode::STATUS_OK;
    }

    std::vector<PanelSnapshot> UIRegistry::get_panels() const
    {
        std::scoped_lock lock{m_mutex};

        std::vector<PanelSnapshot> result{};
        result.reserve(m_panels.size());
        for (const auto& owned : m_panels | std::views::values)
        {
            result.push_back(build_snapshot(owned));
        }
        return result;
    }

    std::optional<PanelSnapshot> UIRegistry::get_panel(const std::string_view panelId) const
    {
        std::scoped_lock lock{m_mutex};

        const std::string key{panelId};
        const auto it = m_panels.find(key);
        if (it == m_panels.end())
        {
            return std::nullopt;
        }

        return build_snapshot(it->second);
    }

    StatusCode UIRegistry::set_value(const std::string_view panelId, const std::string_view fieldId, const UIValue& value)
    {
        std::scoped_lock lock{m_mutex};

        const std::string panelKey{panelId};
        const auto panelIt = m_panels.find(panelKey);
        if (panelIt == m_panels.end())
        {
            return StatusCode::STATUS_ERROR_GENERAL_NOT_FOUND;
        }

        m_values[panelKey][std::string{fieldId}] = value;
        return StatusCode::STATUS_OK;
    }

    std::optional<UIValue> UIRegistry::get_value(const std::string_view panelId, const std::string_view fieldId) const
    {
        std::scoped_lock lock{m_mutex};

        const std::string panelKey{panelId};
        const auto panelIt = m_values.find(panelKey);
        if (panelIt == m_values.end())
        {
            return std::nullopt;
        }

        const std::string fieldKey{fieldId};
        const auto fieldIt = panelIt->second.find(fieldKey);
        if (fieldIt == panelIt->second.end())
        {
            return std::nullopt;
        }

        return fieldIt->second;
    }

    void UIRegistry::clear()
    {
        std::scoped_lock lock{m_mutex};
        m_panels.clear();
        m_values.clear();
    }

    bool UIRegistry::has_panels() const
    {
        std::scoped_lock lock{m_mutex};
        return !m_panels.empty();
    }
}

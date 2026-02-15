//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/savedaddressescontrol.hh>
#include <vertex/customwidgets/valueeditdialog.hh>
#include <vertex/scanner/valuetypes.hh>
#include <wx/menu.h>
#include <wx/clipbrd.h>
#include <wx/renderer.h>
#include <fmt/format.h>
#include <algorithm>

namespace Vertex::CustomWidgets
{
    SavedAddressesHeader::SavedAddressesHeader(
        wxWindow* parent,
        Language::ILanguage& languageService
    )
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
    {
        wxWindowBase::SetBackgroundStyle(wxBG_STYLE_PAINT);

        m_codeFont = wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        m_codeFont.SetFaceName("Consolas");
        m_codeFontBold = m_codeFont.Bold();

        wxClientDC dc(this);
        dc.SetFont(m_codeFontBold);
        m_charWidth = dc.GetCharWidth();
        m_headerHeight = dc.GetCharHeight() + FromDIP(8);
        m_columnPadding = FromDIP(8);

        m_freezeWidth = m_charWidth * 8;
        m_addressWidth = m_charWidth * 18;
        m_typeWidth = m_charWidth * 12;
        m_valueWidth = m_charWidth * 24;

        m_headerFreeze = wxString::FromUTF8(languageService.fetch_translation("mainWindow.ui.savedColumnFreeze"));
        m_headerAddress = wxString::FromUTF8(languageService.fetch_translation("mainWindow.ui.savedColumnAddress"));
        m_headerType = wxString::FromUTF8(languageService.fetch_translation("mainWindow.ui.savedColumnType"));
        m_headerValue = wxString::FromUTF8(languageService.fetch_translation("mainWindow.ui.savedColumnValue"));

        wxWindowBase::SetMinSize(wxSize(-1, m_headerHeight));
        wxWindowBase::SetMaxSize(wxSize(-1, m_headerHeight));

        Bind(wxEVT_PAINT, &SavedAddressesHeader::on_paint, this);
        Bind(wxEVT_ERASE_BACKGROUND, &SavedAddressesHeader::on_erase_background, this);
        Bind(wxEVT_MOTION, &SavedAddressesHeader::on_mouse_motion, this);
        Bind(wxEVT_LEFT_DOWN, &SavedAddressesHeader::on_mouse_left_down, this);
        Bind(wxEVT_LEFT_UP, &SavedAddressesHeader::on_mouse_left_up, this);
        Bind(wxEVT_MOUSE_CAPTURE_LOST, &SavedAddressesHeader::on_mouse_capture_lost, this);
        Bind(wxEVT_LEAVE_WINDOW, &SavedAddressesHeader::on_mouse_leave, this);
    }

    void SavedAddressesHeader::set_horizontal_scroll_offset(const int offset)
    {
        if (m_hScrollOffset != offset)
        {
            m_hScrollOffset = offset;
            Refresh(false);
        }
    }

    void SavedAddressesHeader::set_column_resize_callback(ColumnResizeCallback callback)
    {
        m_columnResizeCallback = std::move(callback);
    }

    int SavedAddressesHeader::get_separator_x(const int separatorIndex) const
    {
        int x = m_columnPadding - m_hScrollOffset;

        switch (separatorIndex)
        {
            case 0:
                x += m_freezeWidth + m_columnPadding / 2;
                break;
            case 1:
                x += m_freezeWidth + m_columnPadding + m_addressWidth + m_columnPadding / 2;
                break;
            case 2:
                x += m_freezeWidth + m_columnPadding + m_addressWidth + m_columnPadding +
                     m_typeWidth + m_columnPadding / 2;
                break;
            default:
                return -1;
        }
        return x;
    }

    int SavedAddressesHeader::get_separator_at_x(const int x) const
    {
        for (int i = 0; i < 3; ++i)
        {
            const int sepX = get_separator_x(i);
            if (std::abs(x - sepX) <= SEPARATOR_HIT_TOLERANCE)
            {
                return i;
            }
        }
        return -1;
    }

    void SavedAddressesHeader::on_mouse_motion(wxMouseEvent& event)
    {
        const int mouseX = event.GetX();

        if (m_resizingColumn >= 0)
        {
            const int delta = mouseX - m_resizeStartX;
            const int newWidth = std::max(MIN_COLUMN_WIDTH, m_resizeStartWidth + delta);

            switch (m_resizingColumn)
            {
                case 0: m_freezeWidth = newWidth; break;
                case 1: m_addressWidth = newWidth; break;
                case 2: m_typeWidth = newWidth; break;
                default: break;
            }

            Refresh(false);

            if (m_columnResizeCallback)
            {
                m_columnResizeCallback();
            }
        }
        else
        {
            const int sep = get_separator_at_x(mouseX);
            if (sep >= 0)
            {
                SetCursor(wxCursor(wxCURSOR_SIZEWE));
            }
            else
            {
                SetCursor(wxNullCursor);
            }
        }

        event.Skip();
    }

    void SavedAddressesHeader::on_mouse_left_down(wxMouseEvent& event)
    {
        const int mouseX = event.GetX();
        const int sep = get_separator_at_x(mouseX);

        if (sep >= 0)
        {
            m_resizingColumn = sep;
            m_resizeStartX = mouseX;

            switch (sep)
            {
                case 0: m_resizeStartWidth = m_freezeWidth; break;
                case 1: m_resizeStartWidth = m_addressWidth; break;
                case 2: m_resizeStartWidth = m_typeWidth; break;
                default: break;
            }

            CaptureMouse();
        }

        event.Skip();
    }

    void SavedAddressesHeader::on_mouse_left_up([[maybe_unused]] wxMouseEvent& event)
    {
        if (m_resizingColumn >= 0)
        {
            m_resizingColumn = -1;

            if (HasCapture())
            {
                ReleaseMouse();
            }

            Refresh(false);
            if (m_columnResizeCallback)
            {
                m_columnResizeCallback();
            }
        }

        event.Skip();
    }

    void SavedAddressesHeader::on_mouse_capture_lost([[maybe_unused]] wxMouseCaptureLostEvent& event)
    {
        m_resizingColumn = -1;
        SetCursor(wxNullCursor);
    }

    void SavedAddressesHeader::on_mouse_leave([[maybe_unused]] wxMouseEvent& event)
    {
        if (m_resizingColumn < 0)
        {
            SetCursor(wxNullCursor);
        }
        event.Skip();
    }

    void SavedAddressesHeader::on_paint([[maybe_unused]] wxPaintEvent& event)
    {
        wxBufferedPaintDC dc(this);
        const wxSize size = GetClientSize();

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_colors.headerBackground));
        dc.DrawRectangle(0, 0, size.GetWidth(), size.GetHeight());

        dc.SetPen(wxPen(m_colors.headerBorder, 1));
        dc.DrawLine(0, size.GetHeight() - 1, size.GetWidth(), size.GetHeight() - 1);

        dc.SetFont(m_codeFontBold);
        dc.SetTextForeground(m_colors.headerText);

        int x = m_columnPadding - m_hScrollOffset;
        const int y = (m_headerHeight - dc.GetCharHeight()) / 2;

        dc.DrawText(m_headerFreeze, x, y);
        x += m_freezeWidth + m_columnPadding;

        dc.SetPen(wxPen(m_resizingColumn == 0 ? m_colors.separatorHover : m_colors.headerBorder, 1));
        dc.DrawLine(x - m_columnPadding / 2, 2, x - m_columnPadding / 2, m_headerHeight - 2);

        dc.DrawText(m_headerAddress, x, y);
        x += m_addressWidth + m_columnPadding;

        dc.SetPen(wxPen(m_resizingColumn == 1 ? m_colors.separatorHover : m_colors.headerBorder, 1));
        dc.DrawLine(x - m_columnPadding / 2, 2, x - m_columnPadding / 2, m_headerHeight - 2);

        dc.DrawText(m_headerType, x, y);
        x += m_typeWidth + m_columnPadding;

        dc.SetPen(wxPen(m_resizingColumn == 2 ? m_colors.separatorHover : m_colors.headerBorder, 1));
        dc.DrawLine(x - m_columnPadding / 2, 2, x - m_columnPadding / 2, m_headerHeight - 2);

        dc.DrawText(m_headerValue, x, y);
    }

    void SavedAddressesHeader::on_erase_background([[maybe_unused]] wxEraseEvent& event)
    {
    }

    SavedAddressesControl::SavedAddressesControl(
        wxWindow* parent,
        Language::ILanguage& languageService,
        const std::shared_ptr<ViewModel::MainViewModel>& viewModel,
        SavedAddressesHeader* header
    )
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxVSCROLL | wxHSCROLL | wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
        , m_languageService(languageService)
        , m_viewModel(viewModel)
        , m_header(header)
    {
        wxWindowBase::SetBackgroundStyle(wxBG_STYLE_PAINT);

        m_codeFont = wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        m_codeFont.SetFaceName("Consolas");

        wxClientDC dc(this);
        dc.SetFont(m_codeFont);
        m_lineHeight = dc.GetCharHeight() + FromDIP(4);

        m_refreshTimer = new wxTimer(this, wxID_ANY);
        m_scrollStopTimer = new wxTimer(this, wxID_ANY + 1);

        Bind(wxEVT_PAINT, &SavedAddressesControl::on_paint, this);
        Bind(wxEVT_SIZE, &SavedAddressesControl::on_size, this);
        Bind(wxEVT_LEFT_DOWN, &SavedAddressesControl::on_mouse_left_down, this);
        Bind(wxEVT_LEFT_DCLICK, &SavedAddressesControl::on_mouse_left_dclick, this);
        Bind(wxEVT_RIGHT_DOWN, &SavedAddressesControl::on_mouse_right_down, this);
        Bind(wxEVT_MOUSEWHEEL, &SavedAddressesControl::on_mouse_wheel, this);
        Bind(wxEVT_KEY_DOWN, &SavedAddressesControl::on_key_down, this);
        Bind(wxEVT_ERASE_BACKGROUND, &SavedAddressesControl::on_erase_background, this);

        Bind(wxEVT_SCROLLWIN_TOP, &SavedAddressesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_BOTTOM, &SavedAddressesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_LINEUP, &SavedAddressesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_LINEDOWN, &SavedAddressesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_PAGEUP, &SavedAddressesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_PAGEDOWN, &SavedAddressesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_THUMBTRACK, &SavedAddressesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_THUMBRELEASE, &SavedAddressesControl::on_scrollwin, this);

        Bind(wxEVT_TIMER, &SavedAddressesControl::on_refresh_timer, this, m_refreshTimer->GetId());
        Bind(wxEVT_TIMER, &SavedAddressesControl::on_scroll_timer, this, m_scrollStopTimer->GetId());

        SetScrollRate(m_header->get_char_width(), m_lineHeight);
    }

    SavedAddressesControl::~SavedAddressesControl()
    {
        if (m_refreshTimer)
        {
            m_refreshTimer->Stop();
            delete m_refreshTimer;
        }
        if (m_scrollStopTimer)
        {
            m_scrollStopTimer->Stop();
            delete m_scrollStopTimer;
        }
    }

    void SavedAddressesControl::refresh_list()
    {
        m_itemCount = m_viewModel->get_saved_addresses_count();
        update_virtual_size();
        Refresh(false);
    }

    void SavedAddressesControl::clear_list()
    {
        stop_auto_refresh();
        m_itemCount = 0;
        m_selectedLine = -1;
        update_virtual_size();
        Refresh(false);
    }

    void SavedAddressesControl::start_auto_refresh() const
    {
        if (m_refreshTimer && !m_refreshTimer->IsRunning())
        {
            m_refreshTimer->Start(100);
        }
    }

    void SavedAddressesControl::stop_auto_refresh() const
    {
        if (m_refreshTimer && m_refreshTimer->IsRunning())
        {
            m_refreshTimer->Stop();
        }
    }

    void SavedAddressesControl::set_selection_change_callback(SelectionChangeCallback callback)
    {
        m_selectionChangeCallback = std::move(callback);
    }

    void SavedAddressesControl::set_freeze_toggle_callback(FreezeToggleCallback callback)
    {
        m_freezeToggleCallback = std::move(callback);
    }

    void SavedAddressesControl::set_value_edit_callback(ValueEditCallback callback)
    {
        m_valueEditCallback = std::move(callback);
    }

    void SavedAddressesControl::set_delete_callback(DeleteCallback callback)
    {
        m_deleteCallback = std::move(callback);
    }

    void SavedAddressesControl::set_pointer_scan_callback(PointerScanCallback callback)
    {
        m_pointerScanCallback = std::move(callback);
    }

    void SavedAddressesControl::set_view_in_disassembly_callback(ViewInDisassemblyCallback callback)
    {
        m_viewInDisassemblyCallback = std::move(callback);
    }

    void SavedAddressesControl::set_find_access_callback(FindAccessCallback callback)
    {
        m_findAccessCallback = std::move(callback);
    }

    int SavedAddressesControl::get_selected_index() const
    {
        return m_selectedLine;
    }

    void SavedAddressesControl::on_columns_resized()
    {
        update_virtual_size();
        Refresh(false);
    }

    bool SavedAddressesControl::is_click_on_checkbox(const int x) const
    {
        const int padding = m_header->get_column_padding();
        const int freezeWidth = m_header->get_freeze_width();

        const int checkboxX = padding + (freezeWidth - CHECKBOX_SIZE) / 2;
        const int checkboxEndX = checkboxX + CHECKBOX_SIZE;

        return x >= checkboxX && x < checkboxEndX;
    }

    int SavedAddressesControl::get_column_at_x(const int x) const
    {
        const int padding = m_header->get_column_padding();
        const int freezeWidth = m_header->get_freeze_width();
        const int addressWidth = m_header->get_address_width();
        const int typeWidth = m_header->get_type_width();

        int colStart = padding;

        if (x < colStart + freezeWidth)
        {
            return FREEZE_COLUMN;
        }
        colStart += freezeWidth + padding;

        if (x < colStart + addressWidth)
        {
            return ADDRESS_COLUMN;
        }
        colStart += addressWidth + padding;

        if (x < colStart + typeWidth)
        {
            return TYPE_COLUMN;
        }

        return VALUE_COLUMN;
    }

    void SavedAddressesControl::show_address_edit_dialog(const int lineIndex)
    {
        const auto saved = m_viewModel->get_saved_address_at(lineIndex);

        ValueEditDialog dialog(
            this,
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.dialog.editAddress")),
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.dialog.addressLabel")),
            wxString::FromUTF8(saved.addressStr)
        );

        if (dialog.ShowModal() == wxID_OK)
        {
            const wxString newAddressStr = dialog.get_value();
            try
            {
                const std::uint64_t newAddress = std::stoull(newAddressStr.ToStdString(), nullptr, 16);
                m_viewModel->set_saved_address_address(lineIndex, newAddress);
                Refresh(false);
            }
            catch (...)
            {
            }
        }
    }

    void SavedAddressesControl::show_value_edit_dialog(const int lineIndex)
    {
        const auto saved = m_viewModel->get_saved_address_at(lineIndex);

        ValueEditDialog dialog(
            this,
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.dialog.editValue")),
            wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.dialog.valueLabel")),
            wxString::FromUTF8(saved.value)
        );

        if (dialog.ShowModal() == wxID_OK)
        {
            const wxString newValue = dialog.get_value();
            m_viewModel->set_saved_address_value(lineIndex, newValue.ToStdString());
            Refresh(false);
        }
    }

    void SavedAddressesControl::show_type_combo_popup(const int lineIndex, const int x, const int y)
    {
        hide_type_combo();

        m_editingLine = lineIndex;
        const auto saved = m_viewModel->get_saved_address_at(lineIndex);

        const int typeWidth = m_header->get_type_width();

        m_typeCombo = new wxComboBox(this, wxID_ANY, wxEmptyString,
                                      wxPoint(x, y), wxSize(typeWidth, m_lineHeight),
                                      0, nullptr, wxCB_READONLY | wxCB_DROPDOWN);

        const auto valueTypeNames = m_viewModel->get_value_type_names();
        for (const auto& typeName : valueTypeNames)
        {
            m_typeCombo->Append(wxString::FromUTF8(typeName));
        }

        m_typeCombo->SetSelection(saved.valueTypeIndex);

        m_typeCombo->Bind(wxEVT_COMBOBOX, &SavedAddressesControl::on_type_combo_selection, this);
        m_typeCombo->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& focusEvent)
        {
            focusEvent.Skip();
            CallAfter([this]()
            {
                if (m_typeCombo && !m_typeCombo->HasFocus())
                {
                    hide_type_combo();
                    Refresh(false);
                }
            });
        });

        m_typeCombo->SetFocus();
    }

    void SavedAddressesControl::on_type_combo_selection([[maybe_unused]] wxCommandEvent& event)
    {
        const int editingLine = m_editingLine;
        wxComboBox* combo = m_typeCombo;

        if (editingLine >= 0 && combo)
        {
            const int newTypeIndex = combo->GetSelection();
            if (newTypeIndex != wxNOT_FOUND)
            {
                CallAfter([this, editingLine, newTypeIndex]()
                {
                    m_viewModel->set_saved_address_type(editingLine, newTypeIndex);
                    hide_type_combo();
                    Refresh(false);
                });
                return;
            }
        }
        hide_type_combo();
        Refresh(false);
    }

    void SavedAddressesControl::hide_type_combo()
    {
        if (m_typeCombo)
        {
            m_typeCombo->Hide();
            m_typeCombo->Destroy();
            m_typeCombo = nullptr;
        }
        m_editingLine = -1;
    }

    void SavedAddressesControl::on_paint([[maybe_unused]] wxPaintEvent& event)
    {
        wxAutoBufferedPaintDC dc(this);
        DoPrepareDC(dc);
        render(dc);
    }

    void SavedAddressesControl::on_size([[maybe_unused]] wxSizeEvent& event)
    {
        update_virtual_size();
        Refresh(false);
        event.Skip();
    }

    void SavedAddressesControl::on_mouse_left_down(const wxMouseEvent& event)
    {
        SetFocus();
        hide_type_combo();

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int y = event.GetY() + scrollY * m_lineHeight;
        const int lineIndex = get_line_at_y(y);

        if (lineIndex >= 0 && lineIndex < m_itemCount)
        {
            const int x = event.GetX() + scrollX * m_header->get_char_width();
            const int column = get_column_at_x(x);

            if (column == FREEZE_COLUMN && is_click_on_checkbox(x))
            {
                const auto saved = m_viewModel->get_saved_address_at(lineIndex);
                const bool newFrozenState = !saved.frozen;

                m_viewModel->set_saved_address_frozen(lineIndex, newFrozenState);

                if (m_freezeToggleCallback)
                {
                    m_freezeToggleCallback(lineIndex, newFrozenState);
                }

                Refresh(false);
            }
            else if (column == TYPE_COLUMN)
            {
                m_selectedLine = lineIndex;

                const int padding = m_header->get_column_padding();
                const int freezeWidth = m_header->get_freeze_width();
                const int addressWidth = m_header->get_address_width();
                const int comboX = padding + freezeWidth + padding + addressWidth + padding;
                const int comboY = get_y_for_line(lineIndex) - scrollY * m_lineHeight;

                show_type_combo_popup(lineIndex, comboX, comboY);
                Refresh(false);
            }
            else
            {
                m_selectedLine = lineIndex;

                if (m_selectionChangeCallback)
                {
                    m_selectionChangeCallback(m_selectedLine);
                }

                Refresh(false);
            }
        }
    }

    void SavedAddressesControl::on_mouse_left_dclick(const wxMouseEvent& event)
    {
        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int y = event.GetY() + scrollY * m_lineHeight;
        const int lineIndex = get_line_at_y(y);

        if (lineIndex >= 0 && lineIndex < m_itemCount)
        {
            const int x = event.GetX() + scrollX * m_header->get_char_width();
            const int column = get_column_at_x(x);

            m_selectedLine = lineIndex;

            switch (column)
            {
                case ADDRESS_COLUMN:
                    show_address_edit_dialog(lineIndex);
                    break;

                case VALUE_COLUMN:
                    show_value_edit_dialog(lineIndex);
                    break;

                case TYPE_COLUMN:
                    break;

                case FREEZE_COLUMN:
                    break;

                default:
                    break;
            }
        }
    }

    void SavedAddressesControl::on_mouse_right_down(wxMouseEvent& event)
    {
        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int y = event.GetY() + scrollY * m_lineHeight;
        const int lineIndex = get_line_at_y(y);

        if (lineIndex >= 0 && lineIndex < m_itemCount)
        {
            m_selectedLine = lineIndex;
            Refresh(false);

            const auto saved = m_viewModel->get_saved_address_at(m_selectedLine);

            wxMenu menu;
            menu.Append(1001, saved.frozen ?
                wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.unfreeze")) :
                wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.freeze")));
            menu.AppendSeparator();
            menu.Append(1002, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.copyAddress")));
            menu.Append(1003, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.copyValue")));
            menu.AppendSeparator();
            menu.Append(1005, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.pointerScan")));
            menu.Append(1006, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.viewInDisassembly")));
            menu.Append(1007, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.findAccess")));
            menu.AppendSeparator();
            menu.Append(1004, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.delete")));

            const int selection = GetPopupMenuSelectionFromUser(menu, event.GetPosition());
            switch (selection)
            {
                case 1001:
                    m_viewModel->set_saved_address_frozen(m_selectedLine, !saved.frozen);
                    if (m_freezeToggleCallback)
                    {
                        m_freezeToggleCallback(m_selectedLine, !saved.frozen);
                    }
                    Refresh(false);
                    break;

                case 1002:
                    if (wxTheClipboard->Open())
                    {
                        wxTheClipboard->SetData(new wxTextDataObject(saved.addressStr));
                        wxTheClipboard->Close();
                    }
                    break;

                case 1003:
                    if (wxTheClipboard->Open())
                    {
                        wxTheClipboard->SetData(new wxTextDataObject(saved.value));
                        wxTheClipboard->Close();
                    }
                    break;

                case 1004:
                    m_viewModel->remove_saved_address(m_selectedLine);
                    if (m_deleteCallback)
                    {
                        m_deleteCallback(m_selectedLine);
                    }
                    m_selectedLine = -1;
                    refresh_list();
                    break;

                case 1005:
                    if (m_pointerScanCallback)
                    {
                        m_pointerScanCallback(saved.address);
                    }
                    break;

                case 1006:
                    if (m_viewInDisassemblyCallback)
                    {
                        m_viewInDisassemblyCallback(saved.address);
                    }
                    break;

                case 1007:
                    if (m_findAccessCallback)
                    {
                        const auto sizeFromType = Scanner::get_value_type_size(
                            static_cast<Scanner::ValueType>(saved.valueTypeIndex));
                        m_findAccessCallback(saved.address, static_cast<std::uint32_t>(sizeFromType));
                    }
                    break;

                default:
                    break;
            }
        }
    }

    void SavedAddressesControl::on_mouse_wheel(const wxMouseEvent& event)
    {
        const int rotation = event.GetWheelRotation();
        const int delta = event.GetWheelDelta();
        const int lines = rotation / delta * 3;

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);
        Scroll(scrollX, scrollY - lines);

        sync_header_scroll();
    }

    void SavedAddressesControl::on_key_down(wxKeyEvent& event)
    {
        const int keyCode = event.GetKeyCode();

        switch (keyCode)
        {
            case WXK_UP:
                if (m_selectedLine > 0)
                {
                    --m_selectedLine;
                    ensure_line_visible(m_selectedLine);
                    if (m_selectionChangeCallback)
                    {
                        m_selectionChangeCallback(m_selectedLine);
                    }
                }
                else if (m_selectedLine == -1 && m_itemCount > 0)
                {
                    m_selectedLine = 0;
                    ensure_line_visible(m_selectedLine);
                    if (m_selectionChangeCallback)
                    {
                        m_selectionChangeCallback(m_selectedLine);
                    }
                }
                break;

            case WXK_DOWN:
                if (m_selectedLine + 1 < m_itemCount)
                {
                    ++m_selectedLine;
                    ensure_line_visible(m_selectedLine);
                    if (m_selectionChangeCallback)
                    {
                        m_selectionChangeCallback(m_selectedLine);
                    }
                }
                else if (m_selectedLine == -1 && m_itemCount > 0)
                {
                    m_selectedLine = 0;
                    ensure_line_visible(m_selectedLine);
                    if (m_selectionChangeCallback)
                    {
                        m_selectionChangeCallback(m_selectedLine);
                    }
                }
                break;

            case WXK_SPACE:
                if (m_selectedLine >= 0 && m_selectedLine < m_itemCount)
                {
                    const auto saved = m_viewModel->get_saved_address_at(m_selectedLine);
                    m_viewModel->set_saved_address_frozen(m_selectedLine, !saved.frozen);
                    if (m_freezeToggleCallback)
                    {
                        m_freezeToggleCallback(m_selectedLine, !saved.frozen);
                    }
                }
                break;

            case WXK_DELETE:
                if (m_selectedLine >= 0 && m_selectedLine < m_itemCount)
                {
                    m_viewModel->remove_saved_address(m_selectedLine);
                    if (m_deleteCallback)
                    {
                        m_deleteCallback(m_selectedLine);
                    }
                    m_selectedLine = -1;
                    refresh_list();
                }
                break;

            default:
                event.Skip();
                return;
        }

        Refresh(false);
    }

    void SavedAddressesControl::on_erase_background([[maybe_unused]] wxEraseEvent& event)
    {
    }

    void SavedAddressesControl::on_scrollwin(wxScrollWinEvent& event)
    {
        m_isScrolling = true;

        sync_header_scroll();

        if (m_scrollStopTimer)
        {
            m_scrollStopTimer->Start(150, wxTIMER_ONE_SHOT);
        }

        event.Skip();
    }

    void SavedAddressesControl::on_refresh_timer([[maybe_unused]] wxTimerEvent& event)
    {
        if (!m_isScrolling)
        {
            refresh_visible_items();
        }
    }

    void SavedAddressesControl::on_scroll_timer([[maybe_unused]] wxTimerEvent& event)
    {
        m_isScrolling = false;
        refresh_visible_items();
    }

    void SavedAddressesControl::sync_header_scroll() const
    {
        if (m_header)
        {
            int scrollX{};
            int scrollY{};
            GetViewStart(&scrollX, &scrollY);
            m_header->set_horizontal_scroll_offset(scrollX * m_header->get_char_width());
        }
    }

    void SavedAddressesControl::refresh_visible_items()
    {
        m_itemCount = m_viewModel->get_saved_addresses_count();

        if (m_itemCount == 0)
        {
            Refresh(false);
            return;
        }

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int clientHeight = GetClientSize().GetHeight();
        const int startLine = scrollY;
        const int visibleCount = (clientHeight / m_lineHeight) + 2;
        const int endLine = std::min(startLine + visibleCount, m_itemCount) - 1;

        if (startLine >= 0 && endLine >= startLine)
        {
            m_viewModel->refresh_saved_addresses_range(startLine, endLine);
        }

        Refresh(false);
    }

    void SavedAddressesControl::render(wxDC& dc)
    {
        render_background(dc);

        if (m_itemCount == 0)
        {
            return;
        }

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int clientHeight = GetClientSize().GetHeight();
        const int startLine = scrollY;
        const int endLine = std::min(
            startLine + (clientHeight / m_lineHeight) + 2,
            m_itemCount
        );

        render_lines(dc, startLine, endLine);
    }

    void SavedAddressesControl::render_background(wxDC& dc) const
    {
        dc.SetBackground(wxBrush(m_colors.background));
        dc.Clear();
    }

    void SavedAddressesControl::render_lines(wxDC& dc, const int startLine, const int endLine)
    {
        dc.SetFont(m_codeFont);

        for (int i = startLine; i < endLine; ++i)
        {
            const int y = get_y_for_line(i);
            render_line(dc, i, y);
        }
    }

    void SavedAddressesControl::render_checkbox(wxDC& dc, const int x, const int y, const bool checked, [[maybe_unused]] const bool hovered)
    {
        int flags{};
        if (checked)
        {
            flags |= wxCONTROL_CHECKED;
        }

        const wxRect checkboxRect(x, y, CHECKBOX_SIZE, CHECKBOX_SIZE);
        wxRendererNative::Get().DrawCheckBox(this, dc, checkboxRect, flags);
    }

    void SavedAddressesControl::render_line(wxDC& dc, const int lineIndex, const int y)
    {
        const auto saved = m_viewModel->get_saved_address_at(lineIndex);

        const int freezeWidth = m_header->get_freeze_width();
        const int addressWidth = m_header->get_address_width();
        const int typeWidth = m_header->get_type_width();
        const int valueWidth = m_header->get_value_width();
        const int padding = m_header->get_column_padding();

        const int totalWidth = freezeWidth + addressWidth + typeWidth + valueWidth + padding * 5;

        wxColour bgColor{};
        const bool isSelected = (lineIndex == m_selectedLine);

        if (isSelected)
        {
            bgColor = m_colors.selectedLine;
        }
        else if (lineIndex % 2 == 1)
        {
            bgColor = m_colors.backgroundAlt;
        }
        else
        {
            bgColor = m_colors.background;
        }

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(bgColor));
        dc.DrawRectangle(0, y, std::max(totalWidth, GetVirtualSize().GetWidth()), m_lineHeight);

        int x = padding;
        const int textY = y + (m_lineHeight - dc.GetCharHeight()) / 2;

        const int checkboxX = x + (freezeWidth - CHECKBOX_SIZE) / 2;
        const int checkboxY = y + (m_lineHeight - CHECKBOX_SIZE) / 2;
        render_checkbox(dc, checkboxX, checkboxY, saved.frozen, false);

        x += freezeWidth + padding;

        dc.SetPen(wxPen(m_colors.separator, 1));
        dc.DrawLine(x - padding / 2, y, x - padding / 2, y + m_lineHeight);

        dc.SetTextForeground(m_colors.address);
        dc.DrawText(saved.addressStr, x, textY);
        x += addressWidth + padding;

        dc.SetPen(wxPen(m_colors.separator, 1));
        dc.DrawLine(x - padding / 2, y, x - padding / 2, y + m_lineHeight);

        dc.SetTextForeground(m_colors.type);
        dc.DrawText(saved.valueType, x, textY);
        x += typeWidth + padding;

        dc.SetPen(wxPen(m_colors.separator, 1));
        dc.DrawLine(x - padding / 2, y, x - padding / 2, y + m_lineHeight);

        dc.SetTextForeground(saved.frozen ? m_colors.frozenValue : m_colors.value);
        dc.DrawText(saved.value, x, textY);
    }

    int SavedAddressesControl::get_line_at_y(const int y) const
    {
        return y / m_lineHeight;
    }

    int SavedAddressesControl::get_y_for_line(const int lineIndex) const
    {
        return lineIndex * m_lineHeight;
    }

    int SavedAddressesControl::get_visible_line_count() const
    {
        return GetClientSize().GetHeight() / m_lineHeight;
    }

    void SavedAddressesControl::update_virtual_size()
    {
        const int totalHeight = m_itemCount * m_lineHeight;

        const int freezeWidth = m_header->get_freeze_width();
        const int addressWidth = m_header->get_address_width();
        const int typeWidth = m_header->get_type_width();
        const int valueWidth = m_header->get_value_width();
        const int padding = m_header->get_column_padding();

        const int totalWidth = freezeWidth + addressWidth + typeWidth + valueWidth + padding * 5;

        SetVirtualSize(totalWidth, totalHeight);
    }

    void SavedAddressesControl::ensure_line_visible(const int lineIndex)
    {
        if (lineIndex < 0 || lineIndex >= m_itemCount)
        {
            return;
        }

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int visibleLines = get_visible_line_count();

        if (lineIndex < scrollY)
        {
            Scroll(scrollX, lineIndex);
        }
        else if (lineIndex >= scrollY + visibleLines)
        {
            Scroll(scrollX, lineIndex - visibleLines + 1);
        }

        sync_header_scroll();
    }
}

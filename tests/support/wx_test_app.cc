//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <wx/app.h>

class WxTestApp final : public wxApp
{
public:
    bool OnInit() override { return true; }
};

wxIMPLEMENT_APP_NO_MAIN(WxTestApp);

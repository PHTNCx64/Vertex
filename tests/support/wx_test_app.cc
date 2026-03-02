#include <wx/app.h>

class WxTestApp final : public wxApp
{
public:
    bool OnInit() override { return true; }
};

wxIMPLEMENT_APP_NO_MAIN(WxTestApp);

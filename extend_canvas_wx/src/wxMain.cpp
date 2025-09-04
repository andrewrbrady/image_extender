#include <wx/wx.h>
#include "WxMainFrame.hpp"
#include <wx/display.h>

class ExtendCanvasApp : public wxApp
{
public:
    bool OnInit() override
    {
        if (!wxApp::OnInit()) return false;
        wxInitAllImageHandlers();
        WxMainFrame* frame = new WxMainFrame(nullptr);

        // Force a large initial size on the primary display and maximize
        int displayIndex = 0;
        if (wxDisplay::GetCount() > 0) {
            wxDisplay d(displayIndex);
            wxRect ar = d.GetClientArea();
            // Use nearly full screen; then maximize to ensure width
            int w = (ar.GetWidth() * 95) / 100;
            int h = (ar.GetHeight() * 95) / 100;
            frame->SetSize(w, h);
            frame->Centre();
        }
        frame->Maximize(true);
        frame->Raise();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(ExtendCanvasApp);

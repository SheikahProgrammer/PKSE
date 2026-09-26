#ifndef UI_PANELS_EVENT_GALLERY_PANEL_H
#define UI_PANELS_EVENT_GALLERY_PANEL_H

namespace UI {
    class PKSEFramebuffer;
    class TrainerViewScreen;
}

namespace UI {
namespace Panels {
    void drawEventGallery(UI::TrainerViewScreen& screen, UI::PKSEFramebuffer& fb);
}
}

#endif // UI_PANELS_EVENT_GALLERY_PANEL_H
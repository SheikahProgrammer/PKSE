#ifndef UI_DIALOGS_CATCH_DIALOG_H
#define UI_DIALOGS_CATCH_DIALOG_H

namespace UI
{
    class PKSEFramebuffer;
    class TrainerViewScreen;

    namespace Dialogs
    {
        /// "How did you get it?": the ways the open game could have given the chosen species.
        void drawCatchChooser(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer);
    }
}

#endif

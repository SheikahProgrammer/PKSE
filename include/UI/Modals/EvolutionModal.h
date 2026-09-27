#ifndef UI_MODALS_EVOLUTION_MODAL_H
#define UI_MODALS_EVOLUTION_MODAL_H

namespace UI
{
    class PKSEFramebuffer;
    class TrainerViewScreen;

    namespace Modals
    {
        /**
         * PKForge-inspired Visual Evolution Card Modal.
         *
         * Features:
         * - Dual-pedestal stage with ground shadows and soft destination glow
         * - Animated breathing evolution chevrons (❯ ❯ ❯)
         * - Branch selection chips for multi-evolution lines (Eevee, Applin, Tyrogue, Kirlia)
         * - Authentic in-game requirement badge ("Level 36", "Use Fire Stone", "Trade")
         * - 6-Stat delta comparison grid (HP, ATK, DEF, SPA, SPD, SPE) with before/after and +/- deltas
         * - Ability and detail progression preview
         * - Full ancestor lineage display for fully-evolved Pokémon
         */
        void drawEvolutionModal(TrainerViewScreen &screen, PKSEFramebuffer &framebuffer);
    }
}

#endif // UI_MODALS_EVOLUTION_MODAL_H

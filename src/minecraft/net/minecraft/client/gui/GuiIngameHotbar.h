#ifndef C919_SOURCE_GUI_INGAME_HOTBAR_H
#define C919_SOURCE_GUI_INGAME_HOTBAR_H
#include "util/MCGameplayPlayer.h"

/* Required GlStateManager/RenderItem/Minecraft field dependencies. Native
   graphics identities are managed references; GL/DC resources stay external.
   This translates only GuiIngame.renderHotbarItem, not the GUI constructor,
   RenderItem, FontRenderer, or Minecraft classes. */
typedef struct {
    bool (*pushMatrix)(MCObject *context);
    bool (*translate)(MCObject *context,float x,float y,float z);
    bool (*scale)(MCObject *context,float x,float y,float z);
    bool (*renderItemAndEffectIntoGUI)(MCObject *context,MCObject *renderer,ItemStack *stack,int32_t x,int32_t y);
    bool (*popMatrix)(MCObject *context);
    MCObject *(*getFontRendererObj)(MCObject *context,MCObject *minecraft);
    bool (*renderItemOverlays)(MCObject *context,MCObject *renderer,MCObject *font,ItemStack *stack,int32_t x,int32_t y);
} GuiIngameHotbarDependencies;
typedef struct GuiIngameHotbar {
    MCObject object;
    MCObject *mc,*itemRenderer,*context;
    const GuiIngameHotbarDependencies *dependencies;
} GuiIngameHotbar;
/* Native allocation/binding adapter; not the original GuiIngame constructor. */
GuiIngameHotbar *GuiIngameHotbar_nativeNew(MCObjectHeap *,MCObject *minecraft,MCObject *itemRenderer,
    MCObject *context,const GuiIngameHotbarDependencies *);
bool GuiIngame_renderHotbarItem(GuiIngameHotbar *,int32_t index,int32_t x,int32_t y,float partialTicks,MCGameplayPlayer *);
#endif

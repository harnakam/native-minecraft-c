#include "client/gui/GuiIngameHotbar.h"
#include <string.h>

static void trace(MCObject *o,MCObjectVisitor v,void *c) {
    GuiIngameHotbar *g=(GuiIngameHotbar *)o;
    g->mc=v(g->mc,c); g->itemRenderer=v(g->itemRenderer,c); g->context=v(g->context,c);
}
static const MCObjectClass klass={"GuiIngame.renderHotbarItem",MCObjectHeap_plainClone,trace,NULL};
static bool complete(const GuiIngameHotbarDependencies *d) {
    return d && d->pushMatrix && d->translate && d->scale && d->renderItemAndEffectIntoGUI &&
        d->popMatrix && d->getFontRendererObj && d->renderItemOverlays;
}
GuiIngameHotbar *GuiIngameHotbar_nativeNew(MCObjectHeap *h,MCObject *mc,MCObject *renderer,MCObject *context,const GuiIngameHotbarDependencies *d) {
    if (!h || !complete(d) || !context || context->heap!=h || (mc && mc->heap!=h) || (renderer && renderer->heap!=h)) {
        MCObjectHeap_fail(h); return NULL;
    }
    GuiIngameHotbar *g=(GuiIngameHotbar *)MCObjectHeap_alloc(h,sizeof *g,&klass);
    if (g) { g->mc=mc; g->itemRenderer=renderer; g->context=context; g->dependencies=d; }
    return g;
}
static int32_t integer(uint32_t x) { int32_t s; memcpy(&s,&x,sizeof s); return s; }
static bool result(MCObjectHeap *h,bool ok) { if (!ok) MCObjectHeap_fail(h); return ok && !MCObjectHeap_failed(h); }
static bool references(GuiIngameHotbar *g,MCObjectHeap *h) {
    return result(h,g->context && g->context->heap==h && (!g->mc || g->mc->heap==h) &&
        (!g->itemRenderer || g->itemRenderer->heap==h));
}
static bool valid_array(ItemStackArray *a,MCObjectHeap *h) {
    if (!ItemStackArray_isInstance((MCObject *)a) || a->object.heap!=h) return false;
    size_t bytes=MCObjectHeap_objectSize((MCObject *)a);
    /* Java array lengths cannot outgrow their allocation. Check the native
       storage boundary before reading either length or an indexed element. */
    return bytes>=sizeof *a && a->length>=0 &&
        (size_t)a->length<=(bytes-sizeof *a)/sizeof *a->items;
}
bool GuiIngame_renderHotbarItem(GuiIngameHotbar *g,int32_t index,int32_t x,int32_t y,float partialTicks,MCGameplayPlayer *p) {
    MCObjectHeap *h=g ? g->object.heap : NULL;
    if (!g || g->object.klass!=&klass || !complete(g->dependencies) || !MCGameplayPlayer_isInstance((MCObject *)p) ||
        p->living.entity.object.heap!=h || !InventoryPlayer_isInstance((MCObject *)p->inventory) || p->inventory->object.heap!=h ||
        !valid_array(p->inventory->mainInventory,h) ||
        index<0 || index>=p->inventory->mainInventory->length || !references(g,h)) {
        MCObjectHeap_fail(h); return false;
    }
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return false;
    ItemStack *stack=p->inventory->mainInventory->items[index]; bool ok=true;
    const GuiIngameHotbarDependencies *d=g->dependencies;
    if (stack) {
        /* This scope forbids collection/adoption even if callbacks detach the
           saved local stack from every live field. No value copy is made. */
        if (!ItemStack_isInstance((MCObject *)stack) || stack->object.heap!=h) {
            MCObjectHeap_fail(h); MCObjectRootScope_end(&scope); return false;
        }
        float f=(float)stack->animationsToGo-partialTicks;
        if (f>0.0f) {
            ok=references(g,h) && result(h,d->pushMatrix(g->context));
            float f1=1.0f+f/5.0f;
            int32_t px=integer((uint32_t)x+8u),py=integer((uint32_t)y+12u);
            if (ok) ok=references(g,h) && result(h,d->translate(g->context,(float)px,(float)py,0.0f));
            if (ok) ok=references(g,h) && result(h,d->scale(g->context,1.0f/f1,(f1+1.0f)/2.0f,1.0f));
            if (ok) ok=references(g,h) && result(h,d->translate(g->context,(float)integer(0u-(uint32_t)px),(float)integer(0u-(uint32_t)py),0.0f));
        }
        if (ok) ok=references(g,h) && result(h,d->renderItemAndEffectIntoGUI(g->context,g->itemRenderer,stack,x,y));
        if (ok && f>0.0f) ok=references(g,h) && result(h,d->popMatrix(g->context));
        if (ok && references(g,h)) {
            /* Java captures the invocation receiver before evaluating the
               Minecraft/font argument. The getter may change current fields. */
            MCObject *overlayRenderer=g->itemRenderer;
            MCObject *font=d->getFontRendererObj(g->context,g->mc);
            ok=!MCObjectHeap_failed(h) && references(g,h) && (!font || font->heap==h);
            if (ok) ok=result(h,d->renderItemOverlays(g->context,overlayRenderer,font,stack,x,y));
            else MCObjectHeap_fail(h);
        } else ok=false;
    }
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(h);
}

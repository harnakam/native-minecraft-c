#include "renderer.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>

#define MC_GLYPH_CACHE 2048
typedef struct { int x, z; unsigned revision; GLuint list; bool used; } mesh_cache;
typedef struct { WCHAR code; GLuint list; int width; } glyph_cache;

struct mc_renderer {
    HWND window;
    HDC dc;
    HGLRC context;
    HFONT font;
    HGDIOBJ previous_font;
    int width, height, hide_calls, selected, glyph_count;
    bool hidden, focused, captured, paused, chat_open, suppress_char;
    bool inventory_open, creative_open, creative_allowed, catalog_allowed, workbench;
    uint64_t window_generation;
    bool inventory_blocked, cursor_present, drag_capture;
    int drag_button, drag_start;
    uint64_t drag_slots;
    int inventory_focus, creative_focus, mouse_x, mouse_y;
    unsigned creative_page;
    WCHAR surrogate;
    char chat[301], title[512];
    unsigned chat_units;
    mc_input pending;
    mesh_cache meshes[MC_MAX_CHUNKS];
    glyph_cache glyphs[MC_GLYPH_CACHE];
};

typedef struct { int x,y,width,height,cell,grid_y,hotbar_y; bool workbench; } inventory_layout;
static int inventory_slots(const mc_renderer *r) { return r->workbench ? 46 : 45; }
static inventory_layout inventory_geometry(const mc_renderer *r) {
    inventory_layout g;
    g.workbench=r->workbench;
    g.width=r->width<600 ? r->width-24 : 576; g.height=r->workbench ? 590 : r->creative_open ? 514 : 566;
    g.x=(r->width-g.width)/2; g.y=(r->height-g.height)/2;
    if (g.y<12) g.y=12;
    g.cell=(g.width-36)/9; if (g.cell>58) g.cell=58;
    if (r->workbench && g.cell>52) g.cell=52;
    g.grid_y=g.y+(r->creative_open ? 126 : 68+(r->workbench ? 3 : 2)*g.cell+34); g.hotbar_y=g.grid_y+(r->creative_open ? 4 : 3)*g.cell+15;
    return g;
}
static void inventory_slot_position(const inventory_layout *g,int slot,int *x,int *y) {
    if (g->workbench) {
        if (slot==0) { *x=g->x+18+7*g->cell; *y=g->y+68+g->cell; }
        else if (slot<10) { *x=g->x+18+(2+(slot-1)%3)*g->cell; *y=g->y+68+((slot-1)/3)*g->cell; }
        else if (slot<37) { *x=g->x+18+((slot-10)%9)*g->cell; *y=g->grid_y+((slot-10)/9)*g->cell; }
        else { *x=g->x+18+(slot-37)*g->cell; *y=g->hotbar_y; }
        return;
    }
    if (slot==0) { *x=g->x+18+8*g->cell; *y=g->y+68+g->cell/2; }
    else if (slot<5) { *x=g->x+18+(4+(slot-1)%2)*g->cell; *y=g->y+68+((slot-1)/2)*g->cell; }
    else if (slot<9) { *x=g->x+18+(slot-5)*g->cell; *y=g->y+68; }
    else if (slot<36) { *x=g->x+18+((slot-9)%9)*g->cell; *y=g->grid_y+((slot-9)/9)*g->cell; }
    else { *x=g->x+18+(slot-36)*g->cell; *y=g->hotbar_y; }
}
static bool inside(int x,int y,int left,int top,int width,int height) {
    return x>=left && y>=top && x<left+width && y<top+height;
}
static int inventory_hit(const mc_renderer *r,int x,int y) {
    inventory_layout g=inventory_geometry(r);
    if (inside(x,y,g.x+g.width-54,g.y+13,40,28)) return -13;
    if (r->catalog_allowed && inside(x,y,g.x+g.width-178,g.y+13,116,28)) return -10;
    if (r->creative_open) {
        if (inside(x,y,g.x+18,g.y+68,70,32)) return -11;
        if (inside(x,y,g.x+g.width-88,g.y+68,70,32)) return -12;
        for (int i=0;i<36;i++) if (inside(x,y,g.x+18+(i%9)*g.cell,g.grid_y+(i/9)*g.cell,g.cell-4,g.cell-4)) {
            unsigned index=r->creative_page*36u+(unsigned)i;
            return index<mc_item_creative_count() ? 1000+(int)index : -1;
        }
    }
    for (int slot=r->creative_open ? 36 : 0;slot<inventory_slots(r);slot++) {
        int sx,sy; inventory_slot_position(&g,slot,&sx,&sy);
        if (inside(x,y,sx,sy,g.cell-4,g.cell-4)) return slot;
    }
    return inside(x,y,g.x,g.y,g.width,g.height) ? -1 : -999;
}
static int inventory_neighbor(const mc_renderer *r,int key) {
    inventory_layout g=inventory_geometry(r); int x,y,best=r->inventory_focus;
    inventory_slot_position(&g,best,&x,&y); unsigned best_score=UINT_MAX;
    for (int i=0;i<inventory_slots(r);i++) {
        int nx,ny; inventory_slot_position(&g,i,&nx,&ny);
        int forward=key==VK_LEFT ? x-nx : key==VK_RIGHT ? nx-x : key==VK_UP ? y-ny : ny-y;
        int cross=key==VK_LEFT || key==VK_RIGHT ? abs(ny-y) : abs(nx-x);
        if (forward<=0) continue;
        unsigned score=(unsigned)forward+3u*(unsigned)cross;
        if (score<best_score) { best_score=score; best=i; }
    }
    return best;
}
static void inventory_activate(mc_renderer *r,int hit,int button,bool double_click) {
    if (!r->focused || r->paused || r->chat_open) return;
    if (hit==-13) r->pending.toggle_inventory=true;
    else if (hit==-10) r->pending.toggle_creative=true;
    else if (hit==-11 && r->creative_page) --r->creative_page;
    else if (hit==-12 && (r->creative_page+1u)*36u<mc_item_creative_count()) ++r->creative_page;
    else if (hit>=1000 && button==0) r->pending.creative_pick=hit-1000;
    else if (hit==-999 && !r->inventory_blocked && (button==0 || button==1)) {
        r->pending.inventory_click=true; r->pending.inventory_slot=-999; r->pending.inventory_button=button; r->pending.inventory_mode=0;
    } else if (hit>=0 && hit<inventory_slots(r) && !r->inventory_blocked) {
        if (r->creative_open) r->pending.select_slot=hit-36;
        else {
            r->inventory_focus=hit; r->pending.inventory_click=true; r->pending.inventory_slot=hit;
            r->pending.inventory_button=button;
            r->pending.inventory_mode=double_click ? 6 : button==2 ? 3 : (GetAsyncKeyState(VK_SHIFT)&0x8000) ? 1 : 0;
        }
    }
}
static void inventory_mouse_down(mc_renderer *r,int hit,int button,bool double_click) {
    if (!r->focused || r->paused || r->chat_open || r->inventory_blocked) return;
    if (!r->creative_open && r->cursor_present && hit>=1 && hit<inventory_slots(r) && !double_click &&
        !(GetAsyncKeyState(VK_SHIFT)&0x8000) && (button!=2 || r->creative_allowed)) {
        r->drag_capture=true; r->drag_button=button; r->drag_start=hit; r->drag_slots=UINT64_C(1)<<hit;
        SetCapture(r->window); return;
    }
    inventory_activate(r,hit,button,double_click);
}
static void inventory_mouse_up(mc_renderer *r,int button) {
    if (!r->drag_capture || r->drag_button!=button) return;
    uint64_t slots=r->drag_slots; int start=r->drag_start; r->drag_capture=false; ReleaseCapture();
    if (!r->focused || r->paused || r->chat_open || !r->inventory_open || r->inventory_blocked) return;
    if (slots && (slots&(slots-1))) {
        r->pending.inventory_drag=true; r->pending.inventory_drag_mode=(unsigned)button; r->pending.inventory_drag_slots=slots;
    } else inventory_activate(r,start,button,false);
}

static void release_cursor(mc_renderer *r) {
    if (!r->captured) return;
    ClipCursor(NULL);
    while (r->hide_calls > 0) { ShowCursor(TRUE); --r->hide_calls; }
    r->captured = false;
}

static void chat_character(mc_renderer *r, unsigned codepoint) {
    unsigned units = codepoint > 65535 ? 2u : 1u;
    if (codepoint < 32 || codepoint == 127 || codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff) || r->chat_units + units > 100) return;
    unsigned char utf8[4]; size_t size;
    if (codepoint < 128) { utf8[0] = (unsigned char)codepoint; size = 1; }
    else if (codepoint < 2048) { utf8[0] = (unsigned char)(0xc0u | (codepoint >> 6)); utf8[1] = (unsigned char)(0x80u | (codepoint & 63u)); size = 2; }
    else if (codepoint < 65536) { utf8[0] = (unsigned char)(0xe0u | (codepoint >> 12)); utf8[1] = (unsigned char)(0x80u | ((codepoint >> 6) & 63u)); utf8[2] = (unsigned char)(0x80u | (codepoint & 63u)); size = 3; }
    else { utf8[0] = (unsigned char)(0xf0u | (codepoint >> 18)); utf8[1] = (unsigned char)(0x80u | ((codepoint >> 12) & 63u)); utf8[2] = (unsigned char)(0x80u | ((codepoint >> 6) & 63u)); utf8[3] = (unsigned char)(0x80u | (codepoint & 63u)); size = 4; }
    size_t length = strlen(r->chat);
    if (length + size < sizeof(r->chat)) { memcpy(r->chat + length, utf8, size); r->chat[length + size] = '\0'; r->chat_units += units; }
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    mc_renderer *r = (mc_renderer *)(uintptr_t)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
        r = create->lpCreateParams;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)r);
    }
    if (!r) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
        case WM_CLOSE: r->pending.quit = true; return 0;
        case WM_SIZE: r->width = LOWORD(lparam); r->height = HIWORD(lparam); return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO *limits=(MINMAXINFO *)lparam; RECT minimum={0,0,560,620}; AdjustWindowRect(&minimum,WS_OVERLAPPEDWINDOW,FALSE);
            limits->ptMinTrackSize.x=minimum.right-minimum.left; limits->ptMinTrackSize.y=minimum.bottom-minimum.top; return 0;
        }
        case WM_ACTIVATE:
            r->focused = LOWORD(wparam) != WA_INACTIVE;
            if (!r->focused) {
                r->drag_capture=false; ReleaseCapture();
                release_cursor(r); r->paused = true;
                bool quit=r->pending.quit; memset(&r->pending,0,sizeof(r->pending));
                r->pending.quit=quit; r->pending.select_slot=-1; r->pending.creative_pick=-1;
            }
            return 0;
        case WM_KEYDOWN:
            if (!r->focused) return 0;
            if (wparam == VK_ESCAPE) {
                if (!(lparam & ((LPARAM)1 << 30))) {
                    if (r->chat_open) r->chat_open = false;
                    else if (r->paused) r->paused=false;
                    else if (r->inventory_open) r->pending.toggle_inventory=true;
                    else r->paused = !r->paused;
                    release_cursor(r);
                }
                return 0;
            }
            if (r->chat_open) {
                if (wparam == VK_RETURN) {
                    snprintf(r->pending.chat, sizeof(r->pending.chat), "%s", r->chat);
                    r->pending.chat_submit = r->chat[0] != '\0'; r->chat_open = false;
                }
                return 0;
            }
            if (lparam & ((LPARAM)1 << 30)) return 0;
            if (r->paused) return 0;
            if (wparam=='E' && !r->paused) {
                r->pending.toggle_inventory=true;
                if (!r->inventory_open) { r->inventory_open=true; r->inventory_focus=9; }
                release_cursor(r); return 0;
            }
            if (r->inventory_open) {
                int delta=wparam==VK_LEFT ? -1 : wparam==VK_RIGHT ? 1 : wparam==VK_UP ? -9 : wparam==VK_DOWN ? 9 :
                    wparam==VK_TAB ? ((GetAsyncKeyState(VK_SHIFT)&0x8000) ? -1 : 1) : 0;
                if (wparam=='C' && r->catalog_allowed) r->pending.toggle_creative=true;
                else if (wparam==VK_PRIOR && r->creative_page) --r->creative_page;
                else if (wparam==VK_NEXT && (r->creative_page+1u)*36u<mc_item_creative_count()) ++r->creative_page;
                else if (delta) {
                    if (r->creative_open) r->creative_focus=(r->creative_focus+delta+45)%45;
                    else if (wparam==VK_TAB) r->inventory_focus=(r->inventory_focus+delta+inventory_slots(r))%inventory_slots(r);
                    else r->inventory_focus=inventory_neighbor(r,(int)wparam);
                } else if (wparam>= '1' && wparam<='9') {
                    if (r->creative_open) r->pending.select_slot=(int)(wparam-'1');
                    else if (!r->cursor_present && !r->inventory_blocked) {
                        int hit=inventory_hit(r,r->mouse_x,r->mouse_y); if (hit<0 || hit>=inventory_slots(r)) hit=r->inventory_focus;
                        r->pending.inventory_click=true; r->pending.inventory_slot=hit; r->pending.inventory_button=(int)(wparam-'1'); r->pending.inventory_mode=2;
                    }
                } else if (wparam==VK_RETURN || wparam==VK_SPACE) {
                    int hit=r->creative_open ? r->creative_focus<36 ? 1000+(int)(r->creative_page*36u)+(int)r->creative_focus : r->creative_focus : r->inventory_focus;
                    inventory_activate(r,hit,wparam==VK_SPACE ? 1 : 0,false);
                } else if (wparam=='Q' && !r->inventory_blocked && !r->creative_open) {
                    int hit=inventory_hit(r,r->mouse_x,r->mouse_y); if (hit<0 || hit>=inventory_slots(r)) hit=r->inventory_focus;
                    r->pending.inventory_click=true; r->pending.inventory_slot=hit;
                    r->pending.inventory_button=(GetAsyncKeyState(VK_CONTROL)&0x8000) ? 1 : 0; r->pending.inventory_mode=4;
                }
                return 0;
            }
            if (wparam == 'T' && !r->paused) {
                r->chat_open = true; r->chat[0] = '\0'; r->chat_units = 0; r->surrogate = 0; r->suppress_char = true;
                release_cursor(r);
            } else if (!r->paused && wparam >= '1' && wparam <= '9') r->pending.select_slot = (int)(wparam - '1');
            else if (!r->paused && wparam == 'F') r->pending.toggle_flight = true;
            else if (!r->paused && wparam == 'Q') { r->pending.drop_item=true; r->pending.drop_all=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0; }
            return 0;
        case WM_CHAR:
            if (r->suppress_char) { r->suppress_char = false; return 0; }
            if (!r->chat_open) return 0;
            if (wparam == VK_BACK) {
                size_t length = strlen(r->chat);
                if (length) {
                    --length; while (length && ((unsigned char)r->chat[length] & 0xc0u) == 0x80u) --length;
                    unsigned units = (unsigned char)r->chat[length] >= 0xf0u ? 2u : 1u;
                    r->chat[length] = '\0'; if (r->chat_units >= units) r->chat_units -= units;
                }
                return 0;
            }
            if (wparam >= 0xd800 && wparam <= 0xdbff) { r->surrogate = (WCHAR)wparam; return 0; }
            if (wparam >= 0xdc00 && wparam <= 0xdfff && r->surrogate) {
                chat_character(r, 0x10000u + (((unsigned)r->surrogate - 0xd800u) << 10) + (unsigned)wparam - 0xdc00u); r->surrogate = 0;
            } else { r->surrogate = 0; chat_character(r, (unsigned)wparam); }
            return 0;
        case WM_UNICHAR:
            if (wparam == 0xffff) return TRUE;
            if (r->chat_open) chat_character(r, (unsigned)wparam);
            return 0;
        case WM_MOUSEMOVE:
            r->mouse_x=(int)(short)LOWORD(lparam); r->mouse_y=(int)(short)HIWORD(lparam);
            if (r->drag_capture) {
                int hit=inventory_hit(r,r->mouse_x,r->mouse_y);
                if (hit>=1 && hit<inventory_slots(r)) r->drag_slots|=UINT64_C(1)<<hit;
            }
            return 0;
        case WM_LBUTTONDBLCLK:
            if (r->inventory_open) inventory_mouse_down(r,inventory_hit(r,(int)(short)LOWORD(lparam),(int)(short)HIWORD(lparam)),0,true);
            return 0;
        case WM_MBUTTONDOWN:
            if (r->inventory_open) inventory_mouse_down(r,inventory_hit(r,(int)(short)LOWORD(lparam),(int)(short)HIWORD(lparam)),2,false);
            return 0;
        case WM_LBUTTONDOWN:
            if (r->inventory_open) { inventory_mouse_down(r,inventory_hit(r,(int)(short)LOWORD(lparam),(int)(short)HIWORD(lparam)),0,false); return 0; }
            if (!r->paused && !r->chat_open && r->focused) r->pending.break_block = true;
            return 0;
        case WM_RBUTTONDOWN:
            if (r->inventory_open) { inventory_mouse_down(r,inventory_hit(r,(int)(short)LOWORD(lparam),(int)(short)HIWORD(lparam)),1,false); return 0; }
            if (!r->paused && !r->chat_open && r->focused) r->pending.place_block = true;
            return 0;
        case WM_LBUTTONUP: inventory_mouse_up(r,0); return 0;
        case WM_RBUTTONUP: inventory_mouse_up(r,1); return 0;
        case WM_MBUTTONUP: inventory_mouse_up(r,2); return 0;
        case WM_CAPTURECHANGED: r->drag_capture=false; return 0;
        case WM_MOUSEWHEEL:
            if (r->inventory_open && r->creative_open && r->focused && !r->paused && !r->chat_open) {
                if (GET_WHEEL_DELTA_WPARAM(wparam)>0) { if (r->creative_page) --r->creative_page; }
                else if ((r->creative_page+1u)*36u<mc_item_creative_count()) ++r->creative_page;
                return 0;
            }
            if (!r->paused && !r->chat_open) {
                int direction = GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? -1 : 1;
                r->pending.select_slot = (r->selected + direction + 9) % 9;
            }
            return 0;
        default: return DefWindowProcW(window, message, wparam, lparam);
    }
}

mc_renderer *mc_renderer_open(bool hidden, char *error, size_t error_size) {
    mc_renderer *r = calloc(1, sizeof(*r));
    if (!r) { snprintf(error, error_size, "Out of memory"); return NULL; }
    r->hidden = hidden; r->width = 1280; r->height = 800; r->pending.select_slot = -1; r->pending.creative_pick=-1; r->inventory_focus=9;
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW type; memset(&type, 0, sizeof(type));
    type.style = CS_OWNDC|CS_DBLCLKS; type.lpfnWndProc = window_proc; type.hInstance = instance;
    type.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512)); type.lpszClassName = L"C919NativeClient";
    if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { snprintf(error, error_size, "Cannot register window class"); free(r); return NULL; }
    RECT rect = {0, 0, r->width, r->height}; AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    r->window = CreateWindowExW(0, type.lpszClassName, L"C919 | Native protocol 47 client", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, instance, r);
    if (!r->window) { snprintf(error, error_size, "Cannot create window"); mc_renderer_close(r); return NULL; }
    r->dc = GetDC(r->window);
    PIXELFORMATDESCRIPTOR format; memset(&format, 0, sizeof(format));
    format.nSize = sizeof(format); format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA; format.cColorBits = 24; format.cDepthBits = 24; format.iLayerType = PFD_MAIN_PLANE;
    int pixel_format = ChoosePixelFormat(r->dc, &format);
    if (!pixel_format || !SetPixelFormat(r->dc, pixel_format, &format)) { snprintf(error, error_size, "Cannot set OpenGL pixel format"); mc_renderer_close(r); return NULL; }
    r->context = wglCreateContext(r->dc);
    if (!r->context || !wglMakeCurrent(r->dc, r->context)) { snprintf(error, error_size, "Cannot create an OpenGL context"); mc_renderer_close(r); return NULL; }
    r->font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Yu Gothic UI");
    if (!r->font) { snprintf(error, error_size, "Cannot create HUD font"); mc_renderer_close(r); return NULL; }
    r->previous_font = SelectObject(r->dc, r->font);
    glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!hidden) { ShowWindow(r->window, SW_SHOW); SetForegroundWindow(r->window); SetFocus(r->window); r->focused = true; }
    printf("RENDERER OpenGL=%s device=%s\n", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));
    return r;
}

void mc_renderer_close(mc_renderer *r) {
    if (!r) return;
    release_cursor(r);
    if (r->context) {
        wglMakeCurrent(r->dc, r->context);
        for (int i = 0; i < MC_MAX_CHUNKS; ++i) if (r->meshes[i].list) glDeleteLists(r->meshes[i].list, 1);
        for (int i = 0; i < r->glyph_count; ++i) if (r->glyphs[i].list) glDeleteLists(r->glyphs[i].list, 1);
        wglMakeCurrent(NULL, NULL); wglDeleteContext(r->context);
    }
    if (r->previous_font && r->dc) SelectObject(r->dc, r->previous_font);
    if (r->font) DeleteObject(r->font);
    if (r->dc && r->window) ReleaseDC(r->window, r->dc);
    if (r->window) DestroyWindow(r->window);
    free(r);
}

void mc_renderer_poll(mc_renderer *r, mc_input *input) {
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
    *input = r->pending;
    input->inventory_generation_set=true; input->inventory_generation=r->window_generation;
    memset(&r->pending, 0, sizeof(r->pending)); r->pending.select_slot = -1; r->pending.creative_pick=-1;
    input->paused = r->paused; input->chat_open = r->chat_open;
    bool active = r->focused && !r->hidden && !r->paused && !r->chat_open && !r->inventory_open && r->width > 0 && r->height > 0;
    if (!active) { release_cursor(r); return; }
    RECT rect; GetClientRect(r->window, &rect);
    POINT upper = {rect.left, rect.top}, lower = {rect.right, rect.bottom};
    ClientToScreen(r->window, &upper); ClientToScreen(r->window, &lower);
    RECT screen = {upper.x, upper.y, lower.x, lower.y};
    POINT center = {(screen.left + screen.right) / 2, (screen.top + screen.bottom) / 2};
    if (!r->captured) {
        ClipCursor(&screen);
        do { ++r->hide_calls; } while (ShowCursor(FALSE) >= 0);
        SetCursorPos(center.x, center.y); r->captured = true;
    } else {
        POINT position; GetCursorPos(&position);
        input->look_x = (float)(position.x - center.x); input->look_y = (float)(position.y - center.y);
        ClipCursor(&screen); SetCursorPos(center.x, center.y);
    }
#define DOWN(key) ((GetAsyncKeyState(key) & 0x8000) != 0)
    input->forward = DOWN('W'); input->backward = DOWN('S'); input->left = DOWN('A'); input->right = DOWN('D');
    input->up = DOWN(VK_SPACE); input->down = DOWN(VK_SHIFT); input->sprint = DOWN(VK_CONTROL);
#undef DOWN
}

static void material_color(int id, int face, unsigned variation, float *red, float *green, float *blue) {
    float r = 0.67f, g = 0.61f, b = 0.48f;
    switch (id) {
        case 1: r = 0.54f; g = 0.59f; b = 0.62f; break;
        case 2: if (face == 1) { r = 0.37f; g = 0.65f; b = 0.35f; } else { r = 0.53f; g = 0.39f; b = 0.26f; } break;
        case 3: r = 0.54f; g = 0.39f; b = 0.28f; break;
        case 4: r = 0.46f; g = 0.51f; b = 0.54f; break;
        case 5: r = 0.77f; g = 0.62f; b = 0.40f; break;
        case 7: r = 0.25f; g = 0.28f; b = 0.31f; break;
        case 8: case 9: r = 0.20f; g = 0.48f; b = 0.68f; break;
        case 10: case 11: r = 0.95f; g = 0.39f; b = 0.11f; break;
        case 12: r = 0.88f; g = 0.82f; b = 0.60f; break;
        case 13: r = 0.55f; g = 0.53f; b = 0.50f; break;
        case 17: r = face < 2 ? 0.72f : 0.43f; g = face < 2 ? 0.58f : 0.32f; b = face < 2 ? 0.35f : 0.21f; break;
        case 18: r = 0.25f; g = 0.52f; b = 0.29f; break;
        case 20: r = 0.67f; g = 0.85f; b = 0.87f; break;
        case 45: r = 0.72f; g = 0.39f; b = 0.31f; break;
        case 49: r = 0.23f; g = 0.19f; b = 0.30f; break;
        case 79: r = 0.58f; g = 0.77f; b = 0.88f; break;
        case 80: r = 0.92f; g = 0.94f; b = 0.94f; break;
        default: break;
    }
    const float shade[6] = {0.52f, 1.0f, 0.80f, 0.87f, 0.72f, 0.92f};
    float light = shade[face] * (0.96f + (variation % 9u) * 0.008f);
    *red = r * light; *green = g * light; *blue = b * light;
}

static const int face_offset[6][3] = {{0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}};
static const float face_vertices[6][4][3] = {
    {{0,0,0},{1,0,0},{1,0,1},{0,0,1}}, {{0,1,1},{1,1,1},{1,1,0},{0,1,0}},
    {{1,0,0},{0,0,0},{0,1,0},{1,1,0}}, {{0,0,1},{1,0,1},{1,1,1},{0,1,1}},
    {{0,0,0},{0,0,1},{0,1,1},{0,1,0}}, {{1,0,1},{1,0,0},{1,1,0},{1,1,1}}
};

static unsigned mesh_revision(const mc_world *world, const mc_chunk *chunk) {
    unsigned signature = chunk->revision * 16777619u;
    for (int n = 0; n < 4; ++n) {
        int x = chunk->x + (n == 0 ? -1 : n == 1 ? 1 : 0);
        int z = chunk->z + (n == 2 ? -1 : n == 3 ? 1 : 0);
        for (int i = 0; i < world->count; ++i)
            if (world->chunks[i].x == x && world->chunks[i].z == z) { signature ^= (world->chunks[i].revision + (unsigned)n * 65537u + 1u) * 2166136261u; break; }
    }
    return signature;
}

static void build_mesh(mesh_cache *cache, const mc_world *world, const mc_chunk *chunk) {
    if (!cache->list) cache->list = glGenLists(1);
    glNewList(cache->list, GL_COMPILE); glBegin(GL_QUADS);
    for (int y = 0; y < 256; ++y) for (int z = 0; z < 16; ++z) for (int x = 0; x < 16; ++x) {
        unsigned index = (unsigned)(y * 256 + z * 16 + x);
        int id = chunk->blocks[index] >> 4;
        if (!id) continue;
        for (int face = 0; face < 6; ++face) {
            int nx = x + face_offset[face][0], ny = y + face_offset[face][1], nz = z + face_offset[face][2];
            int neighbor = 0;
            if (ny >= 0 && ny < 256) {
                if (nx >= 0 && nx < 16 && nz >= 0 && nz < 16) neighbor = chunk->blocks[ny * 256 + nz * 16 + nx] >> 4;
                else neighbor = mc_world_get(world, chunk->x * 16 + nx, ny, chunk->z * 16 + nz) >> 4;
            }
            if (neighbor) continue;
            float r, g, b; material_color(id, face, (unsigned)(x * 73 + y * 113 + z * 127), &r, &g, &b); glColor3f(r, g, b);
            for (int v = 0; v < 4; ++v)
                glVertex3f((float)(chunk->x * 16 + x) + face_vertices[face][v][0], (float)y + face_vertices[face][v][1], (float)(chunk->z * 16 + z) + face_vertices[face][v][2]);
        }
    }
    glEnd(); glEndList(); cache->revision = mesh_revision(world, chunk);
}

static void draw_world(mc_renderer *r, const mc_client *client) {
    for (int i = 0; i < MC_MAX_CHUNKS; ++i) if (r->meshes[i].used) {
        bool present = false;
        for (int j = 0; j < client->world.count; ++j) if (r->meshes[i].x == client->world.chunks[j].x && r->meshes[i].z == client->world.chunks[j].z) { present = true; break; }
        if (!present) r->meshes[i].used = false;
    }
    for (int i = 0; i < client->world.count; ++i) {
        const mc_chunk *chunk = &client->world.chunks[i]; mesh_cache *cache = NULL, *empty = NULL;
        double dx = chunk->x * 16.0 + 8 - client->x, dz = chunk->z * 16.0 + 8 - client->z;
        if (dx * dx + dz * dz > 144 * 144) continue;
        for (int j = 0; j < MC_MAX_CHUNKS; ++j) {
            if (r->meshes[j].used && r->meshes[j].x == chunk->x && r->meshes[j].z == chunk->z) { cache = &r->meshes[j]; break; }
            if (!r->meshes[j].used && !empty) empty = &r->meshes[j];
        }
        if (!cache && empty) { cache = empty; cache->used = true; cache->x = chunk->x; cache->z = chunk->z; cache->revision = ~mesh_revision(&client->world, chunk); }
        if (!cache) continue;
        if (cache->revision != mesh_revision(&client->world, chunk)) build_mesh(cache, &client->world, chunk);
        glCallList(cache->list);
    }
}

static void box(double x0, double y0, double z0, double x1, double y1, double z1, float red, float green, float blue) {
    glBegin(GL_QUADS);
    for (int face = 0; face < 6; ++face) {
        float light = face == 1 ? 1.0f : face < 2 ? 0.55f : 0.80f;
        glColor3f(red * light, green * light, blue * light);
        for (int v = 0; v < 4; ++v) glVertex3d(face_vertices[face][v][0] ? x1 : x0, face_vertices[face][v][1] ? y1 : y0, face_vertices[face][v][2] ? z1 : z0);
    }
    glEnd();
}

static glyph_cache *get_glyph(mc_renderer *r, WCHAR code) {
    for (int i = 0; i < r->glyph_count; ++i) if (r->glyphs[i].code == code) return &r->glyphs[i];
    if (r->glyph_count >= MC_GLYPH_CACHE) return NULL;
    glyph_cache *glyph = &r->glyphs[r->glyph_count];
    glyph->code = code; glyph->list = glGenLists(1); glyph->width = 8;
    GetCharWidth32W(r->dc, code, code, &glyph->width);
    if (!wglUseFontBitmapsW(r->dc, code, 1, glyph->list)) { glDeleteLists(glyph->list, 1); glyph->list = 0; return NULL; }
    ++r->glyph_count;
    return glyph;
}

static void text(mc_renderer *r, int x, int y, int max_width, const char *value, float red, float green, float blue) {
    WCHAR wide[1024];
    int count = MultiByteToWideChar(CP_UTF8, 0, value, -1, wide, (int)(sizeof(wide) / sizeof(wide[0])));
    if (!count) return;
    glColor3f(red, green, blue); glRasterPos2i(x, y + 16);
    int column = 0;
    for (int i = 0; i < count - 1; ++i) {
        glyph_cache *glyph = get_glyph(r, wide[i]);
        if (!glyph) glyph = get_glyph(r, L'?');
        if (!glyph) continue;
        if (wide[i] == L'\n' || (max_width > 0 && column + glyph->width > max_width)) { y += 21; column = 0; glRasterPos2i(x, y + 16); }
        if (wide[i] == L'\n') continue;
        glCallList(glyph->list); column += glyph->width;
    }
}

/* One bounded line, preserving UTF-16 glyph boundaries and a visible ellipsis. */
static void text_line(mc_renderer *r,int x,int y,int max_width,const char *value,float red,float green,float blue) {
    WCHAR wide[1024]; int count=MultiByteToWideChar(CP_UTF8,0,value,-1,wide,1024);
    if (!count || max_width<=0) return;
    int visible=0,used=0; bool truncated=false;
    while (visible<count-1) {
        glyph_cache *glyph=get_glyph(r,wide[visible]); if (!glyph) glyph=get_glyph(r,L'?');
        int width=glyph ? glyph->width : 0;
        if (wide[visible]==L'\n' || used+width>max_width) { truncated=true; break; }
        used+=width; ++visible;
    }
    glyph_cache *dot=get_glyph(r,L'.'); int dots=dot ? 3*dot->width : 0;
    if (truncated) while (visible && used+dots>max_width) {
        glyph_cache *glyph=get_glyph(r,wide[--visible]); if (glyph) used-=glyph->width;
    }
    if (visible && wide[visible-1]>=0xd800 && wide[visible-1]<=0xdbff) --visible;
    glColor3f(red,green,blue); glRasterPos2i(x,y+16);
    for (int i=0;i<visible;i++) { glyph_cache *glyph=get_glyph(r,wide[i]); if (!glyph) glyph=get_glyph(r,L'?'); if (glyph) glCallList(glyph->list); }
    if (truncated && dot && dots<=max_width) { glCallList(dot->list); glCallList(dot->list); glCallList(dot->list); }
}

static void panel(float x, float y, float width, float height, float red, float green, float blue, float alpha) {
    glColor4f(red, green, blue, alpha); glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + width, y); glVertex2f(x + width, y + height); glVertex2f(x, y + height); glEnd();
}

static void block_icon(int x, int y, int id) {
    const float points[7][2] = {{0,-10},{11,-4},{11,8},{0,14},{-11,8},{-11,-4},{0,2}};
    const int polygons[3][4] = {{0,1,6,5},{5,6,3,4},{6,1,2,3}};
    for (int p = 0; p < 3; ++p) {
        float r, g, b; material_color(id, p == 0 ? 1 : p == 1 ? 4 : 3, 5, &r, &g, &b); glColor3f(r,g,b); glBegin(GL_QUADS);
        for (int v = 0; v < 4; ++v) glVertex2f((float)x + points[polygons[p][v]][0], (float)y + points[polygons[p][v]][1]);
        glEnd();
    }
}

typedef struct { bool present,tagged; int32_t id,count,damage; } slot_render;
static slot_render render_stack(const ItemStack *s) {
    return (slot_render){s!=NULL,s && s->stackTagCompound,s ? ItemStack_registryId(s->item) : -1,s ? s->stackSize : 0,s ? s->itemDamage : 0};
}
static void slot_icon_view(mc_renderer *r,slot_render slot,int x,int y,int width,bool focused,bool blocked) {
    if (focused) panel((float)(x-2),(float)(y-2),(float)(width+4),(float)(width+4),0.75f,0.87f,0.78f,1);
    panel((float)x,(float)y,(float)width,(float)width,blocked ? 0.10f : 0.13f,blocked ? 0.12f : 0.18f,blocked ? 0.13f : 0.20f,1);
    if (!slot.present) return;
    uint16_t state;
    if (mc_item_block_state(slot.id,slot.damage,&state)) block_icon(x+width/2,y+width/2,(int)(state>>4));
    else {
        const char *name=mc_item_name(slot.id),*last=strrchr(name,' '); if (last) name=last+1;
        char caption[6]; size_t count=strlen(name); if (count>5) count=5;
        while (count && ((unsigned char)name[count]&0xc0u)==0x80u) --count;
        memcpy(caption,name,count); caption[count]=0;
        text_line(r,x+5,y+width/2-11,width-10,caption,0.87f,0.91f,0.95f);
    }
    if (slot.tagged) panel((float)(x+width-8),(float)(y+5),3,3,0.93f,0.74f,0.34f,1);
    if (slot.count!=1) {
        char number[16]; snprintf(number,sizeof(number),"%d",slot.count); int pixels=0;
        for (const char *p=number;*p;p++) { glyph_cache *glyph=get_glyph(r,(WCHAR)*p); if (glyph) pixels+=glyph->width; }
        text_line(r,x+width-3-pixels,y+width-22,pixels,number,1,slot.count<1 ? 0.3f : 1,slot.count<1 ? 0.3f : 0.94f);
    }
}

static void slot_icon(mc_renderer *r,const ItemStack *slot,int x,int y,int width,bool focused,bool blocked) {
    slot_icon_view(r,render_stack(slot),x,y,width,focused,blocked);
}

static void draw_inventory(mc_renderer *r,const mc_client *c) {
    inventory_layout g=inventory_geometry(r); char line[512];
    panel(0,0,(float)r->width,(float)r->height,0.02f,0.035f,0.05f,0.76f);
    panel((float)g.x,(float)g.y,(float)g.width,(float)g.height,0.06f,0.095f,0.11f,1);
    text_line(r,g.x+18,g.y+16,g.width-(r->catalog_allowed ? 218 : 90),
        r->workbench ? c->window_title : r->creative_open ? "CREATIVE CATALOG" : "PLAYER INVENTORY",0.97f,0.98f,0.94f);
    panel((float)(g.x+g.width-54),(float)(g.y+13),40,28,0.18f,0.24f,0.26f,1); text(r,g.x+g.width-43,g.y+17,22,"E",1,1,1);
    if (r->catalog_allowed) {
        panel((float)(g.x+g.width-178),(float)(g.y+13),116,28,0.17f,0.28f,0.29f,1);
        text(r,g.x+g.width-170,g.y+17,101,r->creative_open ? "Inventory (C)" : "Creative (C)",0.91f,0.96f,0.92f);
    }
    int hover=inventory_hit(r,r->mouse_x,r->mouse_y);
    ItemStack *tooltip=NULL; slot_render creative={0};
    bool blocked=!mc_client_inventory_ready(c) || r->paused || !r->focused;
    if (r->creative_open) {
        text(r,g.x+g.width/2-73,g.y+74,150,"Page Up / Down",0.76f,0.84f,0.84f);
        panel((float)(g.x+18),(float)(g.y+68),70,32,0.16f,0.23f,0.25f,1); text(r,g.x+28,g.y+74,50,"Prev",r->creative_page ? 0.94f : 0.46f,0.86f,0.81f);
        panel((float)(g.x+g.width-88),(float)(g.y+68),70,32,0.16f,0.23f,0.25f,1); text(r,g.x+g.width-78,g.y+74,50,"Next",0.94f,0.86f,0.81f);
        snprintf(line,sizeof(line),"%u / %u   |   %u item variants",r->creative_page+1,(mc_item_creative_count()+35)/36,mc_item_creative_count());
        text(r,g.x+18,g.y+103,g.width-36,line,0.71f,0.80f,0.80f);
        for (int i=0;i<36;i++) {
            unsigned index=r->creative_page*36u+(unsigned)i; int16_t id,damage;
            slot_render item={0};
            if (mc_item_creative_at(index,&id,&damage)) item=(slot_render){true,false,id,mc_item_stack_limit(id),damage};
            int x=g.x+18+(i%9)*g.cell,y=g.grid_y+(i/9)*g.cell;
            slot_icon_view(r,item,x,y,g.cell-4,r->creative_focus==i || hover==1000+(int)index,blocked);
            if (item.present && damage) { snprintf(line,sizeof(line),"%d",damage); text(r,x+3,y+1,24,line,0.78f,0.85f,0.95f); }
            if (hover==1000+(int)index || (hover<0 && r->creative_focus==i)) creative=item;
        }
    } else {
        if (r->workbench) {
            text_line(r,g.x+18+2*g.cell,g.y+46,3*g.cell,"Crafting 3 x 3",0.69f,0.81f,0.81f);
            text_line(r,g.x+18+6*g.cell,g.y+68+g.cell,g.cell-4,"->",0.75f,0.85f,0.80f);
        } else {
            text_line(r,g.x+18,g.y+46,4*g.cell,"Armor",0.69f,0.81f,0.81f);
            text_line(r,g.x+18+4*g.cell,g.y+46,5*g.cell-4,"Crafting 2 x 2",0.69f,0.81f,0.81f);
            text_line(r,g.x+18+7*g.cell,g.y+68+g.cell/2,g.cell-4,"->",0.75f,0.85f,0.80f);
        }
        text_line(r,g.x+18,g.grid_y-24,g.width-36,"Storage   |   Shift transfer / 1-9 swap / Q drop",0.69f,0.81f,0.81f);
        for (int slot=0;slot<(r->workbench ? 37 : 36);slot++) {
            int x,y; inventory_slot_position(&g,slot,&x,&y);
            ItemStack *item=mc_client_window_slot(c,slot);
            slot_icon(r,item,x,y,g.cell-4,slot==r->inventory_focus || slot==hover,blocked);
            if (slot==hover || (hover<0 && slot==r->inventory_focus)) tooltip=item;
        }
    }
    int hotbar_start=r->workbench ? 37 : 36;
    for (int slot=hotbar_start;slot<inventory_slots(r);slot++) {
        int x,y; inventory_slot_position(&g,slot,&x,&y);
        bool focus=r->creative_open ? slot-36==mc_client_selected(c) || r->creative_focus==slot : slot==r->inventory_focus;
        ItemStack *item=mc_client_window_slot(c,slot);
        slot_icon(r,item,x,y,g.cell-4,focus || slot==hover,blocked);
        snprintf(line,sizeof(line),"%d",slot-hotbar_start+1); text(r,x+3,y+1,20,line,0.80f,0.87f,0.84f);
        if (slot==hover || (hover<0 && focus)) tooltip=item;
    }
    if (tooltip || creative.present) {
        char name[256]; if (tooltip) mc_client_slot_name(tooltip,name,sizeof name); else snprintf(name,sizeof name,"%s",mc_item_name((int16_t)creative.id));
        text_line(r,g.x+18,g.y+g.height-81,g.width-36,name,0.91f,0.92f,0.86f);
        slot_render info=tooltip ? render_stack(tooltip) : creative;
        snprintf(line,sizeof(line),"Count %d  |  damage / variant %d%s",info.count,info.damage,info.tagged ? "  |  custom data" : "");
        text_line(r,g.x+18,g.y+g.height-60,g.width-36,line,0.75f,0.83f,0.83f);
    } else {
        text_line(r,g.x+18,g.y+g.height-81,g.width-36,"Enter: left click   Space: right click",0.80f,0.86f,0.83f);
        text_line(r,g.x+18,g.y+g.height-60,g.width-36,"Drag: distribute   Q: drop   Tab / arrows: focus",0.75f,0.83f,0.83f);
    }
    const char *status=r->paused ? "Controls paused: Esc resumes inventory interaction" : c->inventory_status[0] ? c->inventory_status : "Waiting for server inventory";
    text_line(r,g.x+18,g.y+g.height-34,g.width-36,status,blocked ? 0.96f : 0.64f,blocked ? 0.77f : 0.83f,blocked ? 0.53f : 0.76f);
    if (mc_client_cursor(c)) {
        int x=r->mouse_x+12,y=r->mouse_y+12; if (x+52>r->width) x=r->width-52; if (y+52>r->height) y=r->height-52;
        slot_icon(r,mc_client_cursor(c),x,y,48,true,false);
    }
}

static void draw_players(mc_renderer *r, const mc_client *c) {
    for (int i = 0; i < MC_CLIENT_PLAYERS; ++i) {
        const mc_remote_player *p = &c->players[i];
        if (!p->active || p->id == c->entity_id) continue;
        double dx = p->x - c->x, dz = p->z - c->z;
        if (dx * dx + dz * dz > 128 * 128) continue;
        glPushMatrix(); glTranslated(p->x, p->y, p->z); glRotatef(-p->yaw, 0, 1, 0);
        float green = 0.40f + (p->uuid[0] % 20) * 0.015f;
        box(-0.22,0.65,-0.13,0.22,1.40,0.13,0.24f,green,0.66f);
        box(-0.23,1.40,-0.23,0.23,1.86,0.23,0.86f,0.70f,0.52f);
        box(-0.40,0.68,-0.12,-0.22,1.38,0.12,0.86f,0.70f,0.52f); box(0.22,0.68,-0.12,0.40,1.38,0.12,0.86f,0.70f,0.52f);
        box(-0.21,0,-0.12,-0.02,0.65,0.12,0.25f,0.29f,0.36f); box(0.02,0,-0.12,0.21,0.65,0.12,0.25f,0.29f,0.36f);
        glPopMatrix();
        if (dx * dx + dz * dz < 32 * 32) {
            glColor3f(1,1,1); glRasterPos3d(p->x - 0.30, p->y + 2.15, p->z);
            for (const char *letter = p->name; *letter; ++letter) { glyph_cache *glyph = get_glyph(r, (WCHAR)(unsigned char)*letter); if (glyph) glCallList(glyph->list); }
        }
    }
}

static void draw_target(const mc_client *c) {
    int x, y, z, face;
    if (!mc_client_ray(c, &x, &y, &z, &face)) return;
    const int edges[12][2] = {{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
    glDisable(GL_CULL_FACE); glLineWidth(2.0f); glColor3f(0.95f,0.94f,0.75f); glBegin(GL_LINES);
    for (int i = 0; i < 12; ++i) for (int p = 0; p < 2; ++p) {
        int v = edges[i][p]; glVertex3d(x + ((v & 1) ? 1.003 : -0.003), y + ((v & 2) ? 1.003 : -0.003), z + ((v & 4) ? 1.003 : -0.003));
    }
    glEnd(); glLineWidth(1); glEnable(GL_CULL_FACE);
}
static void draw_items(mc_renderer *r,const mc_client *c) {
    double time=mc_time_ms()/1000.0;
    for (unsigned i=0;i<MC_CLIENT_ITEMS;i++) {
        const mc_client_item *entry=&c->items[i]; EntityItem *e=mc_client_graph_item(&c->gameplay,entry->eid);
        if (!entry->active || !entry->metadata_ready || !e) continue;
        ItemStack *stack=EntityItem_getEntityItem(e); if (!stack) continue;
        double dx=e->posX-c->x,dz=e->posZ-c->z; if (dx*dx+dz*dz>96*96) continue;
        glPushMatrix(); glTranslated(e->posX,e->posY+0.16+sin(time*2+e->entityId)*0.025,e->posZ); glRotated(time*55+(double)e->entityId*13,0,1,0);
        uint16_t state; unsigned count=stack->stackSize>48 ? 5 : stack->stackSize>32 ? 4 : stack->stackSize>16 ? 3 : stack->stackSize>1 ? 2 : 1;
        if (mc_item_block_state(ItemStack_registryId(stack->item),stack->itemDamage,&state)) {
            float red,green,blue; material_color(state>>4,1,0,&red,&green,&blue);
            for (unsigned n=0;n<count;n++) { double offset=n*0.035; box(-0.12+offset,-0.12,-0.12+offset,0.12+offset,0.12,0.12+offset,red,green,blue); }
        } else {
            unsigned color=(unsigned)ItemStack_registryId(stack->item)*2654435761u;
            float red=0.4f+(color&255)/640.0f,green=0.4f+((color>>8)&255)/640.0f,blue=0.4f+((color>>16)&255)/640.0f;
            for (unsigned n=0;n<count;n++) { double offset=n*0.025; box(-0.13+offset,-0.13,-0.025+offset,0.13+offset,0.13,0.025+offset,red,green,blue); }
        }
        if (stack->stackTagCompound) box(-0.025,0.10,-0.025,0.025,0.16,0.025,0.95f,0.73f,0.30f);
        glPopMatrix();
        if (dx*dx+dz*dz<12*12) {
            const char *name=mc_item_name(ItemStack_registryId(stack->item)); glColor3f(0.97f,0.98f,0.91f); glRasterPos3d(e->posX-0.15,e->posY+0.5,e->posZ);
            for (unsigned n=0;n<12 && name[n];n++) { glyph_cache *glyph=get_glyph(r,(WCHAR)(unsigned char)name[n]); if (glyph) glCallList(glyph->list); }
        }
    }
}

static void draw_held_map(mc_renderer *r,const mc_client *c) {
    ItemStack *held=mc_client_player_slot(c,36+mc_client_selected(c));
    if (!held || ItemStack_registryId(held->item)!=358 || c->inventory_open) return;
    const mc_map_info *map=mc_maps_find_const(mc_client_maps(c),held->itemDamage);
    int left=r->width-184,top=16; float pixel=1.125f; char label[64];
    panel((float)left,(float)top,168,194,0.055f,0.08f,0.09f,0.94f);
    snprintf(label,sizeof(label),"Map %d%s",held->itemDamage,map ? "" : " | waiting");
    text_line(r,left+12,top+8,144,label,0.96f,0.96f,0.89f);
    if (!map) { text_line(r,left+12,top+70,144,"Waiting for server pixels",0.76f,0.83f,0.81f); return; }
    panel((float)(left+12),(float)(top+34),144,144,0.73f,0.72f,0.67f,1);
    glBegin(GL_QUADS);
    for (unsigned z=0;z<MC_MAP_SIDE;z++) for (unsigned x=0;x<MC_MAP_SIDE;x++) {
        uint8_t rgb[3]; if (!map->colors[z*MC_MAP_SIDE+x] || !mc_map_pixel_rgb(map->colors[z*MC_MAP_SIDE+x],rgb)) continue;
        glColor3ub(rgb[0],rgb[1],rgb[2]); float sx=left+12+x*pixel,sy=top+34+z*pixel;
        glVertex2f(sx,sy); glVertex2f(sx+pixel,sy); glVertex2f(sx+pixel,sy+pixel); glVertex2f(sx,sy+pixel);
    }
    glEnd();
    for (size_t i=0;i<map->icon_count;i++) {
        const mc_map_icon *icon=&map->icons[i];
        float x=left+12+(64+icon->x/2.0f)*pixel,y=top+34+(64+icon->z/2.0f)*pixel;
        glPushMatrix(); glTranslatef(x,y,0); glRotatef(icon->direction*22.5f,0,0,1);
        glColor3f(icon->type==1 ? 0.2f : 1,icon->type==1 ? 0.85f : 0.94f,icon->type>1 ? 0.2f : 0.9f);
        glBegin(GL_TRIANGLES); glVertex2f(0,-5); glVertex2f(3,3); glVertex2f(-3,3); glEnd(); glPopMatrix();
    }
}

static void draw_hud(mc_renderer *r, const mc_client *c) {
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_FOG); glEnable(GL_BLEND);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, r->width, r->height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    panel(16,16,290,64,0.055f,0.08f,0.09f,0.89f);
    text(r,30,25,260,"C919  /  NATIVE CLIENT",0.94f,0.96f,0.94f);
    char line[512];
    snprintf(line,sizeof(line),"%.32s:%u  |  Protocol 47",c->host,(unsigned)c->port);
    text(r,30,49,260,line,0.61f,0.74f,0.73f);
    int panel_x = r->width > 660 ? r->width - 332 : 16;
    int panel_y = r->width > 660 ? 16 : 88;
    panel((float)panel_x,(float)panel_y,316,64,0.055f,0.08f,0.09f,0.89f);
    text(r,panel_x+14,panel_y+9,286,c->status,c->failed||c->disconnected?1.0f:0.68f,c->failed||c->disconnected?0.56f:0.91f,c->failed||c->disconnected?0.50f:0.77f);
    snprintf(line,sizeof(line),"XYZ %.1f  %.1f  %.1f  |  %d chunks",c->x,c->y,c->z,c->world.count);
    text(r,panel_x+14,panel_y+33,286,line,0.76f,0.83f,0.83f);
    if (c->joined && c->positioned && c->world.count) {
        int center_x=r->width/2, center_y=r->height/2;
        glColor4f(0.98f,0.99f,0.94f,0.95f); glLineWidth(2); glBegin(GL_LINES);
        glVertex2i(center_x-7,center_y); glVertex2i(center_x-2,center_y); glVertex2i(center_x+2,center_y); glVertex2i(center_x+7,center_y);
        glVertex2i(center_x,center_y-7); glVertex2i(center_x,center_y-2); glVertex2i(center_x,center_y+2); glVertex2i(center_x,center_y+7); glEnd(); glLineWidth(1);
    }
    int slot_width = r->width < 700 ? 42 : 56;
    int bar_x=(r->width-9*slot_width)/2, bar_y=r->height-90;
    panel((float)(bar_x-8),(float)(bar_y-7),(float)(9*slot_width+16),63,0.055f,0.08f,0.09f,0.90f);
    for (int i=0;i<9;++i) {
        int x=bar_x+i*slot_width;
        if (i==mc_client_selected(c)) {
            panel((float)x,(float)bar_y,(float)(slot_width-3),49,0.35f,0.65f,0.63f,0.96f);
            panel((float)(x+2),(float)(bar_y+2),(float)(slot_width-7),45,0.10f,0.20f,0.20f,1);
        } else panel((float)x,(float)bar_y,(float)(slot_width-3),49,0.14f,0.19f,0.20f,0.95f);
        slot_icon(r,mc_client_player_slot(c,36+i),x+3,bar_y+3,slot_width-9,false,false);
        snprintf(line,sizeof(line),"%d",i+1); text(r,x+5,bar_y+1,20,line,0.81f,0.86f,0.83f);
    }
    char selected_name[256]; mc_client_slot_name(mc_client_player_slot(c,36+mc_client_selected(c)),selected_name,sizeof(selected_name));
    snprintf(line,sizeof(line),"%s  |  %s",selected_name,c->gamemode==1?(c->flying?"Creative / flying":"Creative / walking"):"Server game mode");
    text(r,bar_x,bar_y-31,9*slot_width,line,0.98f,0.97f,0.89f);
    text_line(r,16,r->height-27,r->width-32,"WASD move   Q / Ctrl-Q drop   E inventory   Mouse edit   T chat   Esc cursor",0.90f,0.93f,0.92f);
    int chat_width = r->width < 900 ? r->width-32 : 720;
    int chat_y=bar_y-69-c->chat_count*22;
    for (int i=MC_CLIENT_CHAT_LINES-c->chat_count;i<MC_CLIENT_CHAT_LINES;++i) {
        panel(16,(float)chat_y,(float)chat_width,21,0.045f,0.065f,0.08f,0.75f);
        text(r,25,chat_y+1,chat_width-18,c->chat[i],0.94f,0.95f,0.92f); chat_y+=22;
    }
    if (r->chat_open) {
        panel(16,(float)(bar_y-63),(float)chat_width,31,0.06f,0.11f,0.13f,0.96f);
        snprintf(line,sizeof(line),"> %s%s",r->chat,((GetTickCount()/450)&1)?"|":"");
        text(r,25,bar_y-58,chat_width-18,line,0.99f,0.99f,0.96f);
    }
    bool loading=!c->joined||!c->positioned||!c->world.count;
    if (c->failed || c->disconnected || loading || r->paused) {
        panel(0,0,(float)r->width,(float)r->height,0.035f,0.06f,0.075f,r->paused?0.58f:0.30f);
        int width=r->width<600?r->width-48:560, x=(r->width-width)/2, y=r->height/2-76;
        panel((float)x,(float)y,(float)width,156,0.06f,0.10f,0.115f,0.96f);
        const char *title=c->failed?"Connection error":c->disconnected?"Disconnected":r->paused?"Controls paused":"Joining world";
        text(r,x+25,y+20,width-50,title,0.97f,0.98f,0.94f);
        if (r->paused&&!c->failed&&!c->disconnected) {
            text(r,x+25,y+55,width-50,"Esc resumes movement. Multiplayer connection stays active.",0.75f,0.84f,0.84f);
            text(r,x+25,y+112,width-50,"Close this window to leave the server.",0.59f,0.71f,0.72f);
        } else {
            text(r,x+25,y+55,width-50,c->status,0.78f,0.85f,0.84f);
            if (loading&&!c->failed&&!c->disconnected) { snprintf(line,sizeof(line),"Received %u packets / %u terrain chunks",c->packets_received,c->chunks_received); text(r,x+25,y+112,width-50,line,0.59f,0.74f,0.72f); }
            else text(r,x+25,y+112,width-50,"Close the window; restart with the correct host and port.",0.59f,0.71f,0.72f);
        }
    }
    if (!c->failed && !c->disconnected) draw_held_map(r,c);
    if (c->inventory_open && !c->failed && !c->disconnected) draw_inventory(r,c);
    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE);
}

static void draw_graph_error(mc_renderer *r,const mc_client *c) {
    /* This native error surface must remain usable without borrowing a failed
       graph. It reads only the client's scalar status and window state. */
    r->inventory_open=false; r->creative_open=false; r->creative_allowed=false; r->catalog_allowed=false;
    r->drag_capture=false; r->cursor_present=false; r->inventory_blocked=true; r->chat_open=false; r->paused=true;
    ReleaseCapture(); release_cursor(r);
    bool quit=r->pending.quit; memset(&r->pending,0,sizeof r->pending);
    r->pending.quit=quit; r->pending.select_slot=-1; r->pending.creative_pick=-1;
    const char *status=c->failed ? c->status : "Cannot borrow source client graph";
    char title[512]; WCHAR wide[512];
    snprintf(title,sizeof title,"C919 | %s | %.180s:%u",status,c->host,(unsigned)c->port);
    MultiByteToWideChar(CP_UTF8,0,title,-1,wide,512); SetWindowTextW(r->window,wide);
    snprintf(r->title,sizeof r->title,"%s",title);
    glViewport(0,0,r->width,r->height); glClearColor(0.035f,0.06f,0.075f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_FOG); glEnable(GL_BLEND);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,r->width,r->height,0,-1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    int width=r->width<600 ? r->width-48 : 560,x=(r->width-width)/2,y=r->height/2-76;
    panel((float)x,(float)y,(float)width,156,0.06f,0.10f,0.115f,1);
    text(r,x+25,y+20,width-50,"Connection error",0.97f,0.98f,0.94f);
    text_line(r,x+25,y+55,width-50,status,1.0f,0.65f,0.57f);
    text(r,x+25,y+112,width-50,"Close the window to leave the server.",0.59f,0.71f,0.72f);
    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glFlush();
    if (!r->hidden) SwapBuffers(r->dc);
}

void mc_renderer_draw(mc_renderer *r, const mc_client *c) {
    if (r->width<1||r->height<1) return;
    MCObjectRootScope frame={0};
    if (!MCObjectRootScope_begin(&frame,c->gameplay.heap)) { draw_graph_error(r,c); return; }
    r->selected=mc_client_selected(c);
    if (r->window_generation!=c->window_generation) {
        r->drag_capture=false; ReleaseCapture(); r->pending.inventory_click=false; r->pending.inventory_drag=false;
        r->pending.creative_pick=-1; r->pending.toggle_creative=false; r->inventory_focus=c->window_id ? 10 : 9;
        r->window_generation=c->window_generation;
    }
    r->workbench=c->window_id!=0;
    r->inventory_open=c->inventory_open; r->creative_open=c->creative_open; r->creative_allowed=c->gamemode==1;
    r->catalog_allowed=c->gamemode==1 && !c->window_id;
    r->inventory_blocked=!mc_client_inventory_ready(c);
    r->cursor_present=mc_client_cursor(c)!=NULL;
    char title[512]; snprintf(title,sizeof(title),"C919 | %s | %.180s:%u",c->status,c->host,(unsigned)c->port);
    if (strcmp(title,r->title)) { WCHAR wide[512]; MultiByteToWideChar(CP_UTF8,0,title,-1,wide,512); SetWindowTextW(r->window,wide); snprintf(r->title,sizeof(r->title),"%s",title); }
    glViewport(0,0,r->width,r->height); glClearColor(0.65f,0.80f,0.84f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    double near_plane=0.08, half_height=near_plane*tan(70.0*0.0174532925199433/2), half_width=half_height*r->width/r->height;
    glFrustum(-half_width,half_width,-half_height,half_height,near_plane,192);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glRotatef(c->pitch,1,0,0); glRotatef(c->yaw+180,0,1,0); glTranslated(-c->x,-c->y-1.62,-c->z);
    GLfloat fog_color[]={0.65f,0.80f,0.84f,1}; glFogfv(GL_FOG_COLOR,fog_color); glFogi(GL_FOG_MODE,GL_LINEAR); glFogf(GL_FOG_START,56); glFogf(GL_FOG_END,128); glEnable(GL_FOG);
    if (c->positioned) { draw_world(r,c); draw_players(r,c); draw_items(r,c); draw_target(c); }
    draw_hud(r,c);
    glFlush();
    if (!r->hidden) SwapBuffers(r->dc);
    MCObjectRootScope_end(&frame);
}

bool mc_renderer_screenshot(mc_renderer *r, const char *path, char *error, size_t error_size) {
    if (r->width<1||r->height<1||r->width>8192||r->height>8192) { snprintf(error,error_size,"Invalid framebuffer size"); return false; }
    size_t row=(size_t)r->width*3, size=row*(size_t)r->height;
    unsigned char *pixels=malloc(size);
    if (!pixels) { snprintf(error,error_size,"Cannot allocate framebuffer readback"); return false; }
    while (glGetError()!=GL_NO_ERROR) {}
    glFinish(); glReadBuffer(r->hidden?GL_BACK:GL_FRONT); glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,r->width,r->height,GL_RGB,GL_UNSIGNED_BYTE,pixels);
    if (glGetError()!=GL_NO_ERROR) { free(pixels); snprintf(error,error_size,"OpenGL framebuffer readback failed"); return false; }
    FILE *file=fopen(path,"wb");
    if (!file) { free(pixels); snprintf(error,error_size,"Cannot write screenshot file"); return false; }
    bool ok=fprintf(file,"P6\n%d %d\n255\n",r->width,r->height)>0;
    for (int y=r->height-1;y>=0&&ok;--y) ok=fwrite(pixels+(size_t)y*row,1,row,file)==row;
    if (fclose(file)!=0) ok=false;
    free(pixels);
    if (!ok) snprintf(error,error_size,"Screenshot write failed");
    return ok;
}

#else
struct mc_renderer { int unused; };
mc_renderer *mc_renderer_open(bool hidden, char *error, size_t error_size) { (void)hidden; snprintf(error,error_size,"The native renderer requires Windows; use --headless on this platform"); return NULL; }
void mc_renderer_close(mc_renderer *r) { (void)r; }
void mc_renderer_poll(mc_renderer *r, mc_input *input) { (void)r; (void)input; }
void mc_renderer_draw(mc_renderer *r, const mc_client *client) { (void)r; (void)client; }
bool mc_renderer_screenshot(mc_renderer *r, const char *path, char *error, size_t error_size) { (void)r; (void)path; snprintf(error,error_size,"The native renderer requires Windows"); return false; }
#endif

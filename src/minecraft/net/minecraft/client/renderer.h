#ifndef C919_RENDERER_H
#define C919_RENDERER_H
#include "client.h"
typedef struct mc_renderer mc_renderer;
mc_renderer *mc_renderer_open(bool hidden, char *error, size_t error_size);
void mc_renderer_close(mc_renderer *renderer);
void mc_renderer_poll(mc_renderer *renderer, mc_input *input);
void mc_renderer_draw(mc_renderer *renderer, const mc_client *client);
bool mc_renderer_screenshot(mc_renderer *renderer, const char *path, char *error, size_t error_size);
#endif

// SPDX-License-Identifier: GPL-3.0-only
#ifndef PAP_UI_H
#define PAP_UI_H
#include "platform.h"
void ui_title(Game *,int,unsigned);
void ui_courses(Game *,int,unsigned);
void ui_setup(Game *,const char *,const char *,const char *,int);
void ui_pause(Game *,int);
void ui_graphics(Game *,int);
void ui_over(Game *);
void ui_error(Game *,const char *);
void ui_save_warning(Game *);
void ui_race(Game *);
#endif

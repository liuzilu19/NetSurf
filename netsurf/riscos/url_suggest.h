/*
 * Copyright 2010 Stephen Fryatt <stevef@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * URL Suggestion Menu (interface).
 */

#ifndef _NETSURF_RISCOS_URL_SUGGEST_H_
#define _NETSURF_RISCOS_URL_SUGGEST_H_

#include "oslib/wimp.h"

#define URL_SUGGEST_MAX_URLS 16

bool ro_gui_url_suggest_init(wimp_menu *menu);
bool ro_gui_url_suggest_prepare_menu(void);

#endif

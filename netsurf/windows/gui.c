/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include "content/urldb.h"
#include "content/fetch.h"
#include "content/fetchers/resource.h"
#include "css/utils.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/mouse.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "desktop/plotters.h"
#include "desktop/save_complete.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "render/html.h"
#include "utils/url.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include "windows/about.h"
#include "windows/gui.h"
#include "windows/findfile.h"
#include "windows/font.h"
#include "windows/localhistory.h"
#include "windows/plot.h"
#include "windows/prefs.h"
#include "windows/resourceid.h"
#include "windows/schedule.h"

#include "windbg.h"

char *default_stylesheet_url;
char *adblock_stylesheet_url;
char *quirks_stylesheet_url;
char *options_file_location;

struct gui_window *input_window = NULL;
struct gui_window *search_current_window;
struct gui_window *window_list = NULL;
HWND font_hwnd;

static char default_page[] = "http://www.netsurf-browser.org/welcome/";
static HICON hIcon, hIconS;
static int open_windows = 0;

static const char windowclassname_main[] = "nswsmainwindow";
static const char windowclassname_drawable[] = "nswsdrawablewindow";

#define NSWS_THROBBER_WIDTH 24
#define NSWS_URL_ENTER (WM_USER)

struct gui_window {
	/* The front's private data connected to a browser window */
	/* currently 1<->1 gui_window<->windows window [non-tabbed] */
	struct browser_window *bw; /** the browser_window */

	HWND main; /**< handle to the actual window */
	HWND toolbar; /**< toolbar handle */
	HWND urlbar; /**< url bar handle */
	HWND throbber; /** throbber handle */
	HWND drawingarea; /**< drawing area handle */
	HWND statusbar; /**< status bar handle */
	HWND vscroll; /**< vertical scrollbar handle */
	HWND hscroll; /**< horizontal scrollbar handle */

	HMENU mainmenu; /**< the main menu */
	HMENU rclick; /**< the right-click menu */
	struct nsws_localhistory *localhistory;	/**< handle to local history window */
	int width; /**< width of window */
	int height; /**< height of drawing area */

	int toolbuttonc; /**< number of toolbar buttons */
	int toolbuttonsize; /**< width, height of buttons */
	bool throbbing; /**< whether currently throbbing */

	struct browser_mouse *mouse; /**< mouse state */

	HACCEL acceltable; /**< accelerators */

	float scale; /**< scale of content */

	int scrollx; /**< current scroll location */
	int scrolly; /**< current scroll location */

	RECT *fullscreen; /**< memorize non-fullscreen area */
	RECT redraw; /**< Area needing redraw. */
	int requestscrollx, requestscrolly; /**< scolling requested. */
	struct gui_window *next, *prev; /**< global linked list */
};

static struct nsws_pointers nsws_pointer;

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

LRESULT CALLBACK nsws_window_urlbar_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK nsws_window_toolbar_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK nsws_window_event_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK nsws_window_drawable_event_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

HINSTANCE hinstance;

void gui_multitask(void)
{
/*	LOG(("gui_multitask")); */
}

void gui_poll(bool active)
{
	MSG Msg; /* message from system */
	BOOL bRet; /* message fetch result */
	int timeout; /* timeout in miliseconds */
	UINT timer_id = 0;

	/* run the scheduler and discover how long to wait for the next event */
	timeout = schedule_run();

	/* if active set timeout so message is not waited for */
	if (active)
		timeout = 0;

	if (timeout == 0) {
		bRet = PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE);
	} else {
		if (timeout > 0) {
			/* set up a timer to ensure we get woken */
			timer_id = SetTimer(NULL, 0, timeout, NULL);
		}

		/* wait for a message */
		bRet = GetMessage(&Msg, NULL, 0, 0);

		/* if a timer was sucessfully created remove it */
		if (timer_id != 0) {
			KillTimer(NULL, timer_id);
		}
	}


	if (bRet > 0) {
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
}

/* obtain gui window structure from windows window handle */
static struct gui_window *
nsws_get_gui_window(HWND hwnd)
{
	struct gui_window *gw = NULL;
	HWND phwnd = hwnd;

	/* scan the window hierachy for gui window */
	while (phwnd != NULL) {
		gw = GetProp(phwnd, TEXT("GuiWnd"));
		if (gw != NULL)
			break;
		phwnd = GetParent(phwnd);
	}

	if (gw == NULL) {
		/* try again looking for owner windows instead */
		phwnd = hwnd;
		while (phwnd != NULL) {
			gw = GetProp(phwnd, TEXT("GuiWnd"));
			if (gw != NULL)
				break;
			phwnd = GetWindow(phwnd, GW_OWNER);
		}
	}

	return gw;
}

bool
nsws_window_go(HWND hwnd, const char *url)
{
	struct gui_window * gw;

	gw = nsws_get_gui_window(hwnd);
	if (gw == NULL)
		return false;

	browser_window_go(gw->bw, url, 0, true);

	return true;
}

/**
 * callback for url bar events
 */
LRESULT CALLBACK
nsws_window_urlbar_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct gui_window *gw;
	WNDPROC urlproc;
	HFONT hFont;

	LOG_WIN_MSG(hwnd, msg, wparam, lparam);

	gw = nsws_get_gui_window(hwnd);

	urlproc = (WNDPROC)GetProp(hwnd, TEXT("OrigMsgProc"));

	/* override messages */
	switch (msg) {
	case WM_CHAR:
		if (wparam == 13) {
			SendMessage(gw->main, WM_COMMAND, IDC_MAIN_LAUNCH_URL, 0);
			return 0;
		}
		break;

	case WM_DESTROY:
		hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
		if (hFont != NULL) {
			LOG(("Destroyed font object"));
			DeleteObject(hFont); 	
		}


	case WM_NCDESTROY:
		/* remove properties if window is being destroyed */
		RemoveProp(hwnd, TEXT("GuiWnd"));
		RemoveProp(hwnd, TEXT("OrigMsgProc"));
		break;
	}

	if (urlproc == NULL) {
		/* the original toolbar procedure is not available */
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	/* chain to the next handler */
	return CallWindowProc(urlproc, hwnd, msg, wparam, lparam);
}

/* calculate the dimensions of the url bar relative to the parent toolbar */
static void
urlbar_dimensions(HWND hWndParent,
		  int toolbuttonsize,
		  int buttonc,
		  int *x,
		  int *y,
		  int *width,
		  int *height)
{
	RECT rc;
	const int cy_edit = 23;

	GetClientRect(hWndParent, &rc);
	*x = (toolbuttonsize + 1) * (buttonc + 1) + (NSWS_THROBBER_WIDTH>>1);
	*y = ((((rc.bottom - 1) - cy_edit) >> 1) * 2) / 3;
	*width = (rc.right - 1) - *x - (NSWS_THROBBER_WIDTH>>1) - NSWS_THROBBER_WIDTH;
	*height = cy_edit;
}


static LRESULT
nsws_window_toolbar_command(struct gui_window *gw,
		    int notification_code,
		    int identifier,
		    HWND ctrl_window)
{
	LOG(("notification_code %d identifier %d ctrl_window %p",
	     notification_code, identifier,  ctrl_window));

	switch(identifier) {

	case IDC_MAIN_URLBAR:
		switch (notification_code) {
		case EN_CHANGE:
			LOG(("EN_CHANGE"));
			break;

		case EN_ERRSPACE:
			LOG(("EN_ERRSPACE"));
			break;

		case EN_HSCROLL:
			LOG(("EN_HSCROLL"));
			break;

		case EN_KILLFOCUS:
			LOG(("EN_KILLFOCUS"));
			break;

		case EN_MAXTEXT:
			LOG(("EN_MAXTEXT"));
			break;

		case EN_SETFOCUS:
			LOG(("EN_SETFOCUS"));
			break;

		case EN_UPDATE:
			LOG(("EN_UPDATE"));
			break;

		case EN_VSCROLL:
			LOG(("EN_VSCROLL"));
			break;

		default:
			LOG(("Unknown notification_code"));
			break;
		}
		break;

	default:
		return 1; /* unhandled */

	}
	return 0; /* control message handled */
}

/**
 * callback for toolbar events
 */
LRESULT CALLBACK
nsws_window_toolbar_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct gui_window *gw;
	int urlx, urly, urlwidth, urlheight;
	WNDPROC toolproc;

	LOG_WIN_MSG(hwnd, msg, wparam, lparam);

	gw = nsws_get_gui_window(hwnd);

	switch (msg) {
	case WM_SIZE:

		urlbar_dimensions(hwnd,
				  gw->toolbuttonsize,
				  gw->toolbuttonc,
				  &urlx, &urly, &urlwidth, &urlheight);

		/* resize url */
		if (gw->urlbar != NULL) {
			MoveWindow(gw->urlbar, urlx, urly, urlwidth, urlheight, true);
		}

		/* move throbber */
		if (gw->throbber != NULL) {
			MoveWindow(gw->throbber,
				   LOWORD(lparam) - NSWS_THROBBER_WIDTH - 4, 8,
				   NSWS_THROBBER_WIDTH, NSWS_THROBBER_WIDTH,
				   true);
		}
		break;

	case WM_COMMAND:
		if (nsws_window_toolbar_command(gw,
						HIWORD(wparam),
						LOWORD(wparam),
						(HWND)lparam) == 0)
			return 0;
		break;
	}

	/* remove properties if window is being destroyed */
	if (msg == WM_NCDESTROY) {
		RemoveProp(hwnd, TEXT("GuiWnd"));
		toolproc = (WNDPROC)RemoveProp(hwnd, TEXT("OrigMsgProc"));
	} else {
		toolproc = (WNDPROC)GetProp(hwnd, TEXT("OrigMsgProc"));
	}

	if (toolproc == NULL) {
		/* the original toolbar procedure is not available */
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	/* chain to the next handler */
	return CallWindowProc(toolproc, hwnd, msg, wparam, lparam);

}

/**
 * update state of forward/back buttons/menu items when page changes
 */
static void nsws_window_update_forward_back(struct gui_window *w)
{
	if (w->bw == NULL)
		return;

	bool forward = history_forward_available(w->bw->history);
	bool back = history_back_available(w->bw->history);

	if (w->mainmenu != NULL) {
		EnableMenuItem(w->mainmenu, IDM_NAV_FORWARD,
			       (forward ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(w->mainmenu, IDM_NAV_BACK,
			       (back ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(w->rclick, IDM_NAV_FORWARD,
			       (forward ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(w->rclick, IDM_NAV_BACK,
			       (back ? MF_ENABLED : MF_GRAYED));
	}

	if (w->toolbar != NULL) {
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) IDM_NAV_FORWARD,
			    MAKELONG((forward ? TBSTATE_ENABLED :
				      TBSTATE_INDETERMINATE), 0));
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) IDM_NAV_BACK,
			    MAKELONG((back ? TBSTATE_ENABLED :
				      TBSTATE_INDETERMINATE), 0));
	}
}

static void nsws_update_edit(struct gui_window *w)
{
	bool paste, copy, del;
	if (GetFocus() == w->urlbar) {
		DWORD i, ii;
		SendMessage(w->urlbar, EM_GETSEL, (WPARAM)&i, (LPARAM)&ii);
		paste = true;
		copy = (i != ii);
		del = (i != ii);

	} else if ((w->bw != NULL) && (w->bw->sel != NULL)){
		paste = (w->bw->paste_callback != NULL);
		copy = w->bw->sel->defined;
		del = ((w->bw->sel->defined) &&
		       (w->bw->caret_callback != NULL));
	} else {
		paste = false;
		copy = false;
		del = false;
	}
	EnableMenuItem(w->mainmenu,
		       IDM_EDIT_PASTE,
		       (paste ? MF_ENABLED : MF_GRAYED));

	EnableMenuItem(w->rclick,
		       IDM_EDIT_PASTE,
		       (paste ? MF_ENABLED : MF_GRAYED));

	EnableMenuItem(w->mainmenu,
		       IDM_EDIT_COPY,
		       (copy ? MF_ENABLED : MF_GRAYED));

	EnableMenuItem(w->rclick,
		       IDM_EDIT_COPY,
		       (copy ? MF_ENABLED : MF_GRAYED));

	if (del == true) {
		EnableMenuItem(w->mainmenu, IDM_EDIT_CUT, MF_ENABLED);
		EnableMenuItem(w->mainmenu, IDM_EDIT_DELETE, MF_ENABLED);
		EnableMenuItem(w->rclick, IDM_EDIT_CUT, MF_ENABLED);
		EnableMenuItem(w->rclick, IDM_EDIT_DELETE, MF_ENABLED);
	} else {
		EnableMenuItem(w->mainmenu, IDM_EDIT_CUT, MF_GRAYED);
		EnableMenuItem(w->mainmenu, IDM_EDIT_DELETE, MF_GRAYED);
		EnableMenuItem(w->rclick, IDM_EDIT_CUT, MF_GRAYED);
		EnableMenuItem(w->rclick, IDM_EDIT_DELETE, MF_GRAYED);
	}
}

static bool
nsws_ctx_menu(struct gui_window *w, HWND hwnd, int x, int y)
{
	RECT rc; /* client area of window */
	POINT pt = { x, y }; /* location of mouse click */

	/* Get the bounding rectangle of the client area. */
	GetClientRect(hwnd, &rc);

	/* Convert the mouse position to client coordinates. */
	ScreenToClient(hwnd, &pt);

	/* If the position is in the client area, display a shortcut menu. */
	if (PtInRect(&rc, pt)) {
		ClientToScreen(hwnd, &pt);
		nsws_update_edit(w);
		TrackPopupMenu(GetSubMenu(w->rclick, 0),
			       TPM_CENTERALIGN | TPM_TOPALIGN,
			       x,
			       y,
			       0,
			       hwnd,
			       NULL);

		return true;
	}

	/* Return false if no menu is displayed. */
	return false;
}

/**
 * set accelerators
 */
static void nsws_window_set_accels(struct gui_window *w)
{
	int i, nitems = 13;
	ACCEL accels[nitems];
	for (i = 0; i < nitems; i++)
		accels[i].fVirt = FCONTROL | FVIRTKEY;
	accels[0].key = 0x51; /* Q */
	accels[0].cmd = IDM_FILE_QUIT;
	accels[1].key = 0x4E; /* N */
	accels[1].cmd = IDM_FILE_OPEN_WINDOW;
	accels[2].key = VK_LEFT;
	accels[2].cmd = IDM_NAV_BACK;
	accels[3].key = VK_RIGHT;
	accels[3].cmd = IDM_NAV_FORWARD;
	accels[4].key = VK_UP;
	accels[4].cmd = IDM_NAV_HOME;
	accels[5].key = VK_BACK;
	accels[5].cmd = IDM_NAV_STOP;
	accels[6].key = VK_SPACE;
	accels[6].cmd = IDM_NAV_RELOAD;
	accels[7].key = 0x4C; /* L */
	accels[7].cmd = IDM_FILE_OPEN_LOCATION;
	accels[8].key = 0x57; /* w */
	accels[8].cmd = IDM_FILE_CLOSE_WINDOW;
	accels[9].key = 0x41; /* A */
	accels[9].cmd = IDM_EDIT_SELECT_ALL;
	accels[10].key = VK_F8;
	accels[10].cmd = IDM_VIEW_SOURCE;
	accels[11].key = VK_RETURN;
	accels[11].fVirt = FVIRTKEY;
	accels[11].cmd = IDC_MAIN_LAUNCH_URL;
	accels[12].key = VK_F11;
	accels[12].fVirt = FVIRTKEY;
	accels[12].cmd = IDM_VIEW_FULLSCREEN;

	w->acceltable = CreateAcceleratorTable(accels, nitems);
}

/**
 * set window icons
 */
static void nsws_window_set_ico(struct gui_window *w)
{
	char ico[PATH_MAX];

	nsws_find_resource(ico, "NetSurf32.ico", "windows/res/NetSurf32.ico");
	LOG(("setting ico as %s", ico));
	hIcon = LoadImage(NULL, ico, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);

	if (hIcon != NULL)
		SendMessage(w->main, WM_SETICON, ICON_BIG, (LPARAM) hIcon);
	nsws_find_resource(ico, "NetSurf16.ico", "windows/res/NetSurf16.ico");
	LOG(("setting ico as %s", ico));
	hIconS = LoadImage(NULL, ico, IMAGE_ICON, 16, 16, LR_LOADFROMFILE);

	if (hIconS != NULL)
		SendMessage(w->main, WM_SETICON, ICON_SMALL, (LPARAM)hIconS);
}


/**
 * creation of throbber
 */
static HWND
nsws_window_throbber_create(struct gui_window *w)
{
	HWND hwnd;
	char avi[PATH_MAX];

	hwnd = CreateWindow(ANIMATE_CLASS,
			    "",
			    WS_CHILD | WS_VISIBLE | ACS_TRANSPARENT,
			    w->width - NSWS_THROBBER_WIDTH - 4,
			    8,
			    NSWS_THROBBER_WIDTH,
			    NSWS_THROBBER_WIDTH,
			    w->main,
			    (HMENU) IDC_MAIN_THROBBER,
			    hinstance,
			    NULL);

	nsws_find_resource(avi, "throbber.avi", "windows/res/throbber.avi");
	LOG(("setting throbber avi as %s", avi));
	Animate_Open(hwnd, avi);
	if (w->throbbing)
		Animate_Play(hwnd, 0, -1, -1);
	else
		Animate_Seek(hwnd, 0);
	ShowWindow(hwnd, SW_SHOWNORMAL);
	return hwnd;
}

static HIMAGELIST
nsws_set_imagelist(HWND hwnd, UINT msg, int resid, int bsize, int bcnt)
{
	HIMAGELIST hImageList;
	HBITMAP hScrBM;

	hImageList = ImageList_Create(bsize, bsize, ILC_COLOR24 | ILC_MASK, 0, bcnt);
	hScrBM = LoadImage(hinstance, MAKEINTRESOURCE(resid),
			   IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	ImageList_AddMasked(hImageList, hScrBM, 0xcccccc);
	DeleteObject(hScrBM);

	SendMessage(hwnd, msg, (WPARAM)0, (LPARAM)hImageList);
	return hImageList;
}

/** create a urlbar and message handler
 *
 * Create an Edit control for enerting urls
 */
static HWND
nsws_window_urlbar_create(struct gui_window *gw, HWND hwndparent)
{
	int urlx, urly, urlwidth, urlheight;
	HWND hwnd;
	WNDPROC	urlproc;
	HFONT hFont;

	urlbar_dimensions(hwndparent,
			  gw->toolbuttonsize,
			  gw->toolbuttonc,
			  &urlx, &urly, &urlwidth, &urlheight);

	/* Create the edit control */
	hwnd = CreateWindowEx(0L,
			      TEXT("Edit"),
			      NULL,
			      WS_CHILD | WS_BORDER | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
			      urlx,
			      urly,
			      urlwidth,
			      urlheight,
			      hwndparent,
			      (HMENU)IDC_MAIN_URLBAR,
			      hinstance,
			      0);

	if (hwnd == NULL) {
		return NULL;
	}

	/* set the gui window associated with this control */
	SetProp(hwnd, TEXT("GuiWnd"), (HANDLE)gw);

	/* subclass the message handler */
	urlproc = (WNDPROC)SetWindowLongPtr(hwnd,
					    GWLP_WNDPROC,
					    (LONG_PTR)nsws_window_urlbar_callback);

	/* save the real handler  */
	SetProp(hwnd, TEXT("OrigMsgProc"), (HANDLE)urlproc);

	hFont = CreateFont(urlheight - 4, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
	if (hFont != NULL) {
		LOG(("Setting font object"));
		SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, 0);
	}

	LOG(("Created url bar hwnd:%p, x:%d, y:%d, w:%d, h:%d", hwnd,urlx, urly, urlwidth,  urlheight));

	return hwnd;
}

/* create a toolbar add controls and message handler */
static HWND
nsws_window_toolbar_create(struct gui_window *gw, HWND hWndParent)
{
	HWND hWndToolbar;
	/* Toolbar buttons */
	TBBUTTON tbButtons[] = {
		{0, IDM_NAV_BACK, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
		{1, IDM_NAV_FORWARD, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
		{2, IDM_NAV_HOME, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
		{3, IDM_NAV_RELOAD, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
		{4, IDM_NAV_STOP, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0},
	};
	WNDPROC	toolproc;

	/* Create the toolbar window and subclass its message handler. */
	hWndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, "Toolbar",
				     WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT,
				     0, 0, 0, 0,
				     hWndParent, NULL, HINST_COMMCTRL, NULL);

	if (!hWndToolbar) {
		return NULL;
	}

	/* set the gui window associated with this toolbar */
	SetProp(hWndToolbar, TEXT("GuiWnd"), (HANDLE)gw);

	/* subclass the message handler */
	toolproc = (WNDPROC)SetWindowLongPtr(hWndToolbar,
					     GWLP_WNDPROC,
					     (LONG_PTR)nsws_window_toolbar_callback);

	/* save the real handler  */
	SetProp(hWndToolbar, TEXT("OrigMsgProc"), (HANDLE)toolproc);

	/* remember how many buttons are being created */
	gw->toolbuttonc = sizeof(tbButtons) / sizeof(TBBUTTON);

	/* Create the standard image list and assign to toolbar. */
	nsws_set_imagelist(hWndToolbar, TB_SETIMAGELIST, IDR_TOOLBAR_BITMAP, gw->toolbuttonsize, gw->toolbuttonc);

	/* Create the disabled image list and assign to toolbar. */
	nsws_set_imagelist(hWndToolbar, TB_SETDISABLEDIMAGELIST, IDR_TOOLBAR_BITMAP_GREY, gw->toolbuttonsize, gw->toolbuttonc);

	/* Create the hot image list and assign to toolbar. */
	nsws_set_imagelist(hWndToolbar, TB_SETHOTIMAGELIST, IDR_TOOLBAR_BITMAP_HOT, gw->toolbuttonsize, gw->toolbuttonc);

	/* Add buttons. */
	SendMessage(hWndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
	SendMessage(hWndToolbar, TB_ADDBUTTONS, (WPARAM)gw->toolbuttonc, (LPARAM)&tbButtons);

	gw->urlbar = nsws_window_urlbar_create(gw, hWndToolbar);

	gw->throbber = nsws_window_throbber_create(gw);

	return hWndToolbar;
}




static LRESULT nsws_drawable_mousemove(struct gui_window *gw, int x, int y)
{
	bool shift = ((GetKeyState(VK_SHIFT) & 0x8000) == 0x8000);
	bool ctrl = ((GetKeyState(VK_CONTROL) & 0x8000) == 0x8000);
	bool alt = ((GetKeyState(VK_MENU) & 0x8000) == 0x8000);

	if ((gw == NULL) ||
	    (gw->mouse == NULL) ||
	    (gw->bw == NULL))
		return 0;

	/* scale co-ordinates */
	x = (x + gw->scrollx) / gw->bw->scale;
	y = (y + gw->scrolly) / gw->bw->scale;

	/* if mouse button held down and pointer moved more than
	 * minimum distance drag is happening */
	if (((gw->mouse->state & (BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_PRESS_2)) != 0) &&
	    (abs(x - gw->mouse->pressed_x) >= 5) &&
	    (abs(y - gw->mouse->pressed_y) >= 5)) {

		LOG(("Drag start state 0x%x", gw->mouse->state));

		if ((gw->mouse->state & BROWSER_MOUSE_PRESS_1) != 0) {
			browser_window_mouse_click(gw->bw, BROWSER_MOUSE_DRAG_1,
						   gw->mouse->pressed_x,
						   gw->mouse->pressed_y);
			gw->mouse->state &= ~BROWSER_MOUSE_PRESS_1;
			gw->mouse->state |= BROWSER_MOUSE_HOLDING_1 |
				BROWSER_MOUSE_DRAG_ON;
		}
		else if ((gw->mouse->state & BROWSER_MOUSE_PRESS_2) != 0) {
			browser_window_mouse_click(gw->bw, BROWSER_MOUSE_DRAG_2,
						   gw->mouse->pressed_x,
						   gw->mouse->pressed_y);
			gw->mouse->state &= ~BROWSER_MOUSE_PRESS_2;
			gw->mouse->state |= BROWSER_MOUSE_HOLDING_2 |
				BROWSER_MOUSE_DRAG_ON;
		}
	}

	if (((gw->mouse->state & BROWSER_MOUSE_MOD_1) != 0) && !shift)
		gw->mouse->state &= ~BROWSER_MOUSE_MOD_1;
	if (((gw->mouse->state & BROWSER_MOUSE_MOD_2) != 0) && !ctrl)
		gw->mouse->state &= ~BROWSER_MOUSE_MOD_2;
	if (((gw->mouse->state & BROWSER_MOUSE_MOD_3) != 0) && !alt)
		gw->mouse->state &= ~BROWSER_MOUSE_MOD_3;


	browser_window_mouse_track(gw->bw, gw->mouse->state, x, y);

	return 0;
}

static LRESULT
nsws_drawable_mousedown(struct gui_window *gw,
			int x, int y,
			browser_mouse_state button)
{
	if ((gw == NULL) ||
	    (gw->mouse == NULL) ||
	    (gw->bw == NULL)) {
		nsws_localhistory_close(gw);
		return 0;
	}

	gw->mouse->state = button;
	if ((GetKeyState(VK_SHIFT) & 0x8000) == 0x8000)
		gw->mouse->state |= BROWSER_MOUSE_MOD_1;
	if ((GetKeyState(VK_CONTROL) & 0x8000) == 0x8000)
		gw->mouse->state |= BROWSER_MOUSE_MOD_2;
	if ((GetKeyState(VK_MENU) & 0x8000) == 0x8000)
		gw->mouse->state |= BROWSER_MOUSE_MOD_3;

	gw->mouse->pressed_x = (x + gw->scrollx) / gw->bw->scale;
	gw->mouse->pressed_y = (y + gw->scrolly) / gw->bw->scale;

	LOG(("mouse click bw %p, state %x, x %f, y %f",gw->bw,
	     gw->mouse->state,
	     (x + gw->scrollx) / gw->bw->scale,
	     (y + gw->scrolly) / gw->bw->scale));

	browser_window_mouse_click(gw->bw, gw->mouse->state,
				   (x + gw->scrollx) / gw->bw->scale ,
				   (y + gw->scrolly) / gw->bw->scale);

	return 0;
}

static LRESULT
nsws_drawable_mouseup(struct gui_window *gw,
		      int x,
		      int y,
		      browser_mouse_state press,
		      browser_mouse_state click)
{
	bool shift = ((GetKeyState(VK_SHIFT) & 0x8000) == 0x8000);
	bool ctrl = ((GetKeyState(VK_CONTROL) & 0x8000) == 0x8000);
	bool alt = ((GetKeyState(VK_MENU) & 0x8000) == 0x8000);

	if ((gw == NULL) ||
	    (gw->mouse == NULL) ||
	    (gw->bw == NULL))
		return 0;

	LOG(("state 0x%x, press 0x%x", gw->mouse->state, press));
	if ((gw->mouse->state & press) != 0) {
		gw->mouse->state &= ~press;
		gw->mouse->state |= click;
	}

	if (((gw->mouse->state & BROWSER_MOUSE_MOD_1) != 0) && !shift)
		gw->mouse->state &= ~BROWSER_MOUSE_MOD_1;
	if (((gw->mouse->state & BROWSER_MOUSE_MOD_2) != 0) && !ctrl)
		gw->mouse->state &= ~BROWSER_MOUSE_MOD_2;
	if (((gw->mouse->state & BROWSER_MOUSE_MOD_3) != 0) && !alt)
		gw->mouse->state &= ~BROWSER_MOUSE_MOD_3;

	if ((gw->mouse->state & click) != 0) {
		LOG(("mouse click bw %p, state 0x%x, x %f, y %f",gw->bw,
					   gw->mouse->state,
					   (x + gw->scrollx) / gw->bw->scale,
					   (y + gw->scrolly) / gw->bw->scale));

		browser_window_mouse_click(gw->bw,
					   gw->mouse->state,
					   (x + gw->scrollx) / gw->bw->scale,
					   (y + gw->scrolly) / gw->bw->scale);
	} else {
		browser_window_mouse_drag_end(gw->bw,
					      0,
					      (x + gw->scrollx) / gw->bw->scale,
					      (y + gw->scrolly) / gw->bw->scale);
	}

	gw->mouse->state = 0;
	return 0;
}

static LRESULT
nsws_drawable_paint(struct gui_window *gw, HWND hwnd)
{
	struct rect clip;
	PAINTSTRUCT ps;

	BeginPaint(hwnd, &ps);

	if (gw != NULL) {

		plot_hdc = ps.hdc;

		clip.x0 = ps.rcPaint.left;
		clip.y0 = ps.rcPaint.top;
		clip.x1 = ps.rcPaint.right;
		clip.y1 = ps.rcPaint.bottom;

		browser_window_redraw(gw->bw,
				      -gw->scrollx / gw->bw->scale,
				      -gw->scrolly / gw->bw->scale,
				      &clip);

	}

	EndPaint(hwnd, &ps);

	return 0;
}

static LRESULT
nsws_drawable_hscroll(struct gui_window *gw, HWND hwnd, WPARAM wparam)
{
	SCROLLINFO si;
	int mem;

	LOG(("HSCROLL %d", gw->requestscrollx));

	if (gw->requestscrollx != 0)
		return 0;

	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	GetScrollInfo(hwnd, SB_HORZ, &si);
	mem = si.nPos;
	switch (LOWORD(wparam))	{
	case SB_LINELEFT:
		si.nPos -= 30;
		break;

	case SB_LINERIGHT:
		si.nPos += 30;
		break;

	case SB_PAGELEFT:
		si.nPos -= gw->width;
		break;

	case SB_PAGERIGHT:
		si.nPos += gw->width;
		break;

	case SB_THUMBTRACK:
		si.nPos = si.nTrackPos;
		break;

	default:
		break;
	}

	si.fMask = SIF_POS;

	if ((gw->bw != NULL) &&
	    (gw->bw->current_content != NULL)) {
		si.nPos = MIN(si.nPos,
			      content_get_width(gw->bw->current_content) *
			      gw->bw->scale - gw->width);
	}
	si.nPos = MAX(si.nPos, 0);
	SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
	GetScrollInfo(hwnd, SB_HORZ, &si);
	if (si.nPos != mem) {
		gui_window_set_scroll(gw,
				      gw->scrollx + gw->requestscrollx + si.nPos - mem,
				      gw->scrolly);
	}

	return 0;
}

static LRESULT
nsws_drawable_vscroll(struct gui_window *gw, HWND hwnd, WPARAM wparam)
{
	SCROLLINFO si;
	int mem;

	LOG(("VSCROLL %d", gw->requestscrolly));

	if (gw->requestscrolly != 0)
		return 0;

	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	GetScrollInfo(hwnd, SB_VERT, &si);
	mem = si.nPos;
	switch (LOWORD(wparam))	{
	case SB_TOP:
		si.nPos = si.nMin;
		break;

	case SB_BOTTOM:
		si.nPos = si.nMax;
		break;

	case SB_LINEUP:
		si.nPos -= 30;
		break;

	case SB_LINEDOWN:
		si.nPos += 30;
		break;

	case SB_PAGEUP:
		si.nPos -= gw->height;
		break;

	case SB_PAGEDOWN:
		si.nPos += gw->height;
		break;

	case SB_THUMBTRACK:
		si.nPos = si.nTrackPos;
		break;

	default:
		break;
	}
	si.fMask = SIF_POS;
	if ((gw->bw != NULL) &&
	    (gw->bw->current_content != NULL)) {
		si.nPos = MIN(si.nPos,
			      content_get_height(gw->bw->current_content) *
			      gw->bw->scale - gw->height);
	}
	si.nPos = MAX(si.nPos, 0);
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
	GetScrollInfo(hwnd, SB_VERT, &si);
	if (si.nPos != mem) {
		gui_window_set_scroll(gw, gw->scrollx, gw->scrolly +
				      gw->requestscrolly + si.nPos - mem);
	}

	return 0;
}

static LRESULT
nsws_drawable_key(struct gui_window *gw, HWND hwnd, WPARAM wparam)
{
	if (GetFocus() != hwnd)
		return 0 ;

	uint32_t i;
	bool shift = ((GetKeyState(VK_SHIFT) & 0x8000) == 0x8000);
	bool capslock = ((GetKeyState(VK_CAPITAL) & 1) == 1);

	switch(wparam) {
	case VK_LEFT:
		i = KEY_LEFT;
		if (shift)
			SendMessage(hwnd, WM_HSCROLL,
				    MAKELONG(SB_LINELEFT, 0), 0);
		break;

	case VK_RIGHT:
		i = KEY_RIGHT;
		if (shift)
			SendMessage(hwnd, WM_HSCROLL,
				    MAKELONG(SB_LINERIGHT, 0), 0);
		break;

	case VK_UP:
		i = KEY_UP;
		if (shift)
			SendMessage(hwnd, WM_VSCROLL,
				    MAKELONG(SB_LINEUP, 0), 0);
		break;

	case VK_DOWN:
		i = KEY_DOWN;
		if (shift)
			SendMessage(hwnd, WM_VSCROLL,
				    MAKELONG(SB_LINEDOWN, 0), 0);
		break;

	case VK_HOME:
		i = KEY_LINE_START;
		if (shift)
			SendMessage(hwnd, WM_HSCROLL,
				    MAKELONG(SB_PAGELEFT, 0), 0);
		break;

	case VK_END:
		i = KEY_LINE_END;
		if (shift)
			SendMessage(hwnd, WM_HSCROLL,
				    MAKELONG(SB_PAGERIGHT, 0), 0);
		break;

	case VK_DELETE:
		i = KEY_DELETE_RIGHT;
		break;

	case VK_NEXT:
		i = wparam;
		SendMessage(hwnd, WM_VSCROLL, MAKELONG(SB_PAGEDOWN, 0),
			    0);
		break;

	case VK_PRIOR:
		i = wparam;
		SendMessage(hwnd, WM_VSCROLL, MAKELONG(SB_PAGEUP, 0),
			    0);
		break;

	default:
		i = wparam;
		break;
	}

	if ((i >= 'A') && (i <= 'Z') &&
	    (((!capslock) && (!shift)) ||
	     ((capslock) && (shift))))
		i += 'a' - 'A';

	if (gw != NULL)
		browser_window_key_press(gw->bw, i);

	return 0;
}

static LRESULT
nsws_drawable_wheel(struct gui_window *gw, HWND hwnd, WPARAM wparam)
{
	int i, z = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
	int key = LOWORD(wparam);
	DWORD command;
	unsigned int newmessage = WM_VSCROLL;
	if (key == MK_SHIFT) {
		command = (z > 0) ? SB_LINERIGHT : SB_LINELEFT;
		newmessage = WM_HSCROLL;
	} else
		/* add MK_CONTROL -> zoom */
		command = (z > 0) ? SB_LINEUP : SB_LINEDOWN;
	z = (z < 0) ? -1 * z : z;
	for (i = 0; i < z; i++)
		SendMessage(hwnd, newmessage, MAKELONG(command, 0), 0);

	return 0;
}

static LRESULT
nsws_drawable_resize(struct gui_window *gw)
{
	browser_window_reformat(gw->bw, gw->width, gw->height);
	return 0;
}

/* Called when activity occours within the drawable window. */
LRESULT CALLBACK
nsws_window_drawable_event_callback(HWND hwnd,
				    UINT msg,
				    WPARAM wparam,
				    LPARAM lparam)
{
	struct gui_window *gw;

	LOG_WIN_MSG(hwnd, msg, wparam, lparam);

	gw = nsws_get_gui_window(hwnd);
	if (gw == NULL) {
		LOG(("Unable to find gui window structure for hwnd %p", hwnd));
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	switch(msg) {

	case WM_MOUSEMOVE:
		return nsws_drawable_mousemove(gw,
					       GET_X_LPARAM(lparam),
					       GET_Y_LPARAM(lparam));

	case WM_LBUTTONDOWN:
		nsws_drawable_mousedown(gw,
					GET_X_LPARAM(lparam),
					GET_Y_LPARAM(lparam),
					BROWSER_MOUSE_PRESS_1);
		SetFocus(hwnd);
		nsws_localhistory_close(gw);
		return 0;
		break;

	case WM_RBUTTONDOWN:
		nsws_drawable_mousedown(gw,
					GET_X_LPARAM(lparam),
					GET_Y_LPARAM(lparam),
					BROWSER_MOUSE_PRESS_2);
		SetFocus(hwnd);
		return 0;
		break;

	case WM_LBUTTONUP:
		return nsws_drawable_mouseup(gw,
					     GET_X_LPARAM(lparam),
					     GET_Y_LPARAM(lparam),
					     BROWSER_MOUSE_PRESS_1,
					     BROWSER_MOUSE_CLICK_1);

	case WM_RBUTTONUP:
		return nsws_drawable_mouseup(gw,
					     GET_X_LPARAM(lparam),
					     GET_Y_LPARAM(lparam),
					     BROWSER_MOUSE_PRESS_2,
					     BROWSER_MOUSE_CLICK_2);

	case WM_ERASEBKGND: /* ignore as drawable window is redrawn on paint */
		return 0;

	case WM_PAINT: /* redraw the exposed part of the window */
		return nsws_drawable_paint(gw, hwnd);

	case WM_KEYDOWN:
		return nsws_drawable_key(gw, hwnd, wparam);

	case WM_SIZE:
		return nsws_drawable_resize(gw);

	case WM_HSCROLL:
		return nsws_drawable_hscroll(gw, hwnd, wparam);

	case WM_VSCROLL:
		return nsws_drawable_vscroll(gw, hwnd, wparam);

	case WM_MOUSEWHEEL:
		return nsws_drawable_wheel(gw, hwnd, wparam);

	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static LRESULT
nsws_window_resize(struct gui_window *gw,
		   HWND hwnd,
		   WPARAM wparam,
		   LPARAM lparam)
{
	int x, y;
	RECT rstatus, rtool;

	if ((gw->toolbar == NULL) ||
	    (gw->urlbar == NULL) ||
	    (gw->statusbar == NULL))
		return 0;

	SendMessage(gw->statusbar, WM_SIZE, wparam, lparam);
	SendMessage(gw->toolbar, WM_SIZE, wparam, lparam);

	GetClientRect(gw->toolbar, &rtool);
	GetWindowRect(gw->statusbar, &rstatus);
	gui_window_get_scroll(gw, &x, &y);
	gw->width = LOWORD(lparam);
	gw->height = HIWORD(lparam) - (rtool.bottom - rtool.top) - (rstatus.bottom - rstatus.top);

	if (gw->drawingarea != NULL) {
		MoveWindow(gw->drawingarea,
			   0,
			   rtool.bottom,
			   gw->width,
			   gw->height,
			   true);
	}
	nsws_window_update_forward_back(gw);

	gui_window_set_scroll(gw, x, y);

	if (gw->toolbar != NULL) {
		SendMessage(gw->toolbar, TB_SETSTATE,
			    (WPARAM) IDM_NAV_STOP,
			    MAKELONG(TBSTATE_INDETERMINATE, 0));
	}

	return 0;
}


static LRESULT
nsws_window_command(HWND hwnd,
		    struct gui_window *gw,
		    int notification_code,
		    int identifier,
		    HWND ctrl_window)
{
	LOG(("notification_code %x identifier %x ctrl_window %p",
	     notification_code, identifier,  ctrl_window));

	switch(identifier) {

	case IDM_FILE_QUIT:
	{
		struct gui_window *w;
		w = window_list;
		while (w != NULL) {
			PostMessage(w->main, WM_CLOSE, 0, 0);
			w = w->next;
		}
		break;
	}

	case IDM_FILE_OPEN_LOCATION:
		SetFocus(gw->urlbar);
		break;

	case IDM_FILE_OPEN_WINDOW:
		browser_window_create(NULL, gw->bw, NULL, false, false);
		break;

	case IDM_FILE_CLOSE_WINDOW:
		PostMessage(gw->main, WM_CLOSE, 0, 0);
		break;

	case IDM_FILE_SAVE_PAGE:
		break;

	case IDM_FILE_SAVEAS_TEXT:
		break;

	case IDM_FILE_SAVEAS_PDF:
		break;

	case IDM_FILE_SAVEAS_POSTSCRIPT:
		break;

	case IDM_FILE_PRINT_PREVIEW:
		break;

	case IDM_FILE_PRINT:
		break;

	case IDM_EDIT_CUT:
		OpenClipboard(gw->main);
		EmptyClipboard();
		CloseClipboard();
		if (GetFocus() == gw->urlbar) {
			SendMessage(gw->urlbar, WM_CUT, 0, 0);
		} else if (gw->bw != NULL) {
			browser_window_key_press(gw->bw, KEY_CUT_SELECTION);
		}
		break;

	case IDM_EDIT_COPY:
		OpenClipboard(gw->main);
		EmptyClipboard();
		CloseClipboard();
		if (GetFocus() == gw->urlbar) {
			SendMessage(gw->urlbar, WM_COPY, 0, 0);
		} else if (gw->bw != NULL) {
			gui_copy_to_clipboard(gw->bw->sel);
		}
		break;

	case IDM_EDIT_PASTE: {
		OpenClipboard(gw->main);
		HANDLE h = GetClipboardData(CF_TEXT);
		if (h != NULL) {
			char *content = GlobalLock(h);
			LOG(("pasting %s\n", content));
			GlobalUnlock(h);
		}
		CloseClipboard();
		if (GetFocus() == gw->urlbar)
			SendMessage(gw->urlbar, WM_PASTE, 0, 0);
		else
			gui_paste_from_clipboard(gw, 0, 0);
		break;
	}

	case IDM_EDIT_DELETE:
		if (GetFocus() == gw->urlbar)
			SendMessage(gw->urlbar, WM_CUT, 0, 0);
		else
			browser_window_key_press(gw->bw, KEY_DELETE_RIGHT);
		break;

	case IDM_EDIT_SELECT_ALL:
		if (GetFocus() == gw->urlbar)
			SendMessage(gw->urlbar, EM_SETSEL, 0, -1);
		else
			selection_select_all(gw->bw->sel);
		break;

	case IDM_EDIT_SEARCH:
		break;

	case IDM_EDIT_PREFERENCES:
		nsws_prefs_dialog_init(hinstance, gw->main);
		break;

	case IDM_NAV_BACK:
		if ((gw->bw != NULL) &&
		    (history_back_available(gw->bw->history))) {
			history_back(gw->bw, gw->bw->history);
		}
		nsws_window_update_forward_back(gw);
		break;

	case IDM_NAV_FORWARD:
		if ((gw->bw != NULL) &&
		    (history_forward_available(gw->bw->history))) {
			history_forward(gw->bw, gw->bw->history);
		}
		nsws_window_update_forward_back(gw);
		break;

	case IDM_NAV_HOME:
		browser_window_go(gw->bw, default_page, 0, true);
		break;

	case IDM_NAV_STOP:
		browser_window_stop(gw->bw);
		break;

	case IDM_NAV_RELOAD:
		browser_window_reload(gw->bw, true);
		break;

	case IDM_NAV_LOCALHISTORY:
		nsws_localhistory_init(gw);
		break;

	case IDM_NAV_GLOBALHISTORY:
		break;

	case IDM_VIEW_ZOOMPLUS: {
		int x, y;
		gui_window_get_scroll(gw, &x, &y);
		if (gw->bw != NULL) {
			browser_window_set_scale(gw->bw, gw->bw->scale * 1.1, true);
			browser_window_reformat(gw->bw, gw->width, gw->height);
		}
		gui_window_redraw_window(gw);
		gui_window_set_scroll(gw, x, y);
		break;
	}

	case IDM_VIEW_ZOOMMINUS: {
		int x, y;
		gui_window_get_scroll(gw, &x, &y);
		if (gw->bw != NULL) {
			browser_window_set_scale(gw->bw,
						 gw->bw->scale * 0.9, true);
			browser_window_reformat(gw->bw, gw->width, gw->height);
		}
		gui_window_redraw_window(gw);
		gui_window_set_scroll(gw, x, y);
		break;
	}

	case IDM_VIEW_ZOOMNORMAL: {
		int x, y;
		gui_window_get_scroll(gw, &x, &y);
		if (gw->bw != NULL) {
			browser_window_set_scale(gw->bw, 1.0, true);
			browser_window_reformat(gw->bw, gw->width, gw->height);
		}
		gui_window_redraw_window(gw);
		gui_window_set_scroll(gw, x, y);
		break;
	}

	case IDM_VIEW_SOURCE:
		break;

	case IDM_VIEW_SAVE_WIN_METRICS: {
		RECT r;
		GetWindowRect(gw->main, &r);
		option_window_x = r.left;
		option_window_y = r.top;
		option_window_width = r.right - r.left;
		option_window_height = r.bottom - r.top;
		options_write(options_file_location);
		break;
	}

	case IDM_VIEW_FULLSCREEN: {
		RECT rdesk;
		if (gw->fullscreen == NULL) {
			HWND desktop = GetDesktopWindow();
			gw->fullscreen = malloc(sizeof(RECT));
			if ((desktop == NULL) ||
			    (gw->fullscreen == NULL)) {
				warn_user("NoMemory", 0);
				break;
			}
			GetWindowRect(desktop, &rdesk);
			GetWindowRect(gw->main, gw->fullscreen);
			DeleteObject(desktop);
			SetWindowLong(gw->main, GWL_STYLE, 0);
			SetWindowPos(gw->main, HWND_TOPMOST, 0, 0,
				     rdesk.right - rdesk.left,
				     rdesk.bottom - rdesk.top,
				     SWP_SHOWWINDOW);
		} else {
			SetWindowLong(gw->main, GWL_STYLE,
				      WS_OVERLAPPEDWINDOW |
				      WS_HSCROLL | WS_VSCROLL |
				      WS_CLIPCHILDREN |
				      WS_CLIPSIBLINGS | CS_DBLCLKS);
			SetWindowPos(gw->main, HWND_TOPMOST,
				     gw->fullscreen->left,
				     gw->fullscreen->top,
				     gw->fullscreen->right -
				     gw->fullscreen->left,
				     gw->fullscreen->bottom -
				     gw->fullscreen->top,
				     SWP_SHOWWINDOW | SWP_FRAMECHANGED);
			free(gw->fullscreen);
			gw->fullscreen = NULL;
		}
		break;
	}

	case IDM_VIEW_DOWNLOADS:
		break;

	case IDM_VIEW_TOGGLE_DEBUG_RENDERING:
		html_redraw_debug = !html_redraw_debug;
		if (gw->bw != NULL) {
			browser_window_reformat(gw->bw, gw->width, gw->height);
		}
		break;

	case IDM_VIEW_DEBUGGING_SAVE_BOXTREE:
		break;

	case IDM_VIEW_DEBUGGING_SAVE_DOMTREE:
		break;

	case IDM_HELP_CONTENTS:
		nsws_window_go(hwnd,
			       "http://www.netsurf-browser.org/documentation/");
		break;

	case IDM_HELP_GUIDE:
		nsws_window_go(hwnd,
			       "http://www.netsurf-browser.org/documentation/guide");
		break;

	case IDM_HELP_INFO:
		nsws_window_go(hwnd,
			       "http://www.netsurf-browser.org/documentation/info");
		break;

	case IDM_HELP_ABOUT:
		nsws_about_dialog_init(hinstance, gw->main);
		break;

	case IDC_MAIN_LAUNCH_URL:
	{
		if (GetFocus() != gw->urlbar)
			break;
		int len = SendMessage(gw->urlbar, WM_GETTEXTLENGTH, 0, 0);
		char addr[len + 1];
		SendMessage(gw->urlbar, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)addr);
		LOG(("launching %s\n", addr));
		browser_window_go(gw->bw, addr, 0, true);
		break;
	}


	default:
		return 1; /* unhandled */

	}
	return 0; /* control message handled */
}


/**
 * callback for window events generally
 */
LRESULT CALLBACK
nsws_window_event_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct gui_window *gw;
		RECT rmain;

	LOG_WIN_MSG(hwnd, msg, wparam, lparam);

	/* deal with window creation as a special case */
	if (msg == WM_CREATE) {
		/* To cause all the component child windows to be
		 * re-sized correctly a WM_SIZE message of the actual
		 * created size must be sent. 
		 *
		 * The message must be posted here because the actual
		 * size values of the component windows are not known
		 * until after the WM_CREATE message is dispatched.
		 */
		GetClientRect(hwnd, &rmain);
		PostMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rmain.right, rmain.bottom));
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}


	gw = nsws_get_gui_window(hwnd);
	if (gw == NULL) {
		LOG(("Unable to find gui window structure for hwnd %p", hwnd));
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	switch(msg) {


	case WM_CONTEXTMENU:
		if (nsws_ctx_menu(gw, hwnd, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
			return 0;
		break;

	case WM_COMMAND:
		if (nsws_window_command(hwnd, gw, HIWORD(wparam), LOWORD(wparam), (HWND)lparam) == 0)
			return 0;
		break;

	case WM_SIZE:
		return nsws_window_resize(gw, hwnd, wparam, lparam);

	case WM_NCDESTROY:
		RemoveProp(hwnd, TEXT("GuiWnd"));
		browser_window_destroy(gw->bw);
		if (--open_windows <= 0) {
			netsurf_quit = true;
		}
		break;

	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}


static void create_local_windows_classes(void) {
	WNDCLASSEX w;

	/* main window */
	w.cbSize	= sizeof(WNDCLASSEX);
	w.style		= 0;
	w.lpfnWndProc	= nsws_window_event_callback;
	w.cbClsExtra	= 0;
	w.cbWndExtra	= 0;
	w.hInstance	= hinstance;
	w.hIcon		= LoadIcon(hinstance, MAKEINTRESOURCE(IDR_NETSURF_ICON));
	w.hCursor	= NULL;
	w.hbrBackground	= (HBRUSH)(COLOR_MENU + 1);
	w.lpszMenuName	= NULL;
	w.lpszClassName = windowclassname_main;
	w.hIconSm	= LoadIcon(hinstance, MAKEINTRESOURCE(IDR_NETSURF_ICON));
	RegisterClassEx(&w);

	/* drawable area */
	w.lpfnWndProc	= nsws_window_drawable_event_callback;
	w.hIcon		= NULL;
	w.lpszMenuName	= NULL;
	w.lpszClassName = windowclassname_drawable;
	w.hIconSm	= NULL;

	RegisterClassEx(&w);

}

/**
 * creation of status bar
 */
static HWND nsws_window_statusbar_create(struct gui_window *w)
{
	HWND hwnd = CreateWindowEx(0,
				   STATUSCLASSNAME,
				   NULL,
				   WS_CHILD | WS_VISIBLE,
				   0, 0, 0, 0,
				   w->main,
				   (HMENU)IDC_MAIN_STATUSBAR,
				   hinstance,
				   NULL);
	SendMessage(hwnd, SB_SETTEXT, 0, (LPARAM)"NetSurf");
	return hwnd;
}

static css_fixed get_window_dpi(HWND hwnd)
{
	HDC hdc = GetDC(hwnd);
	int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
	css_fixed fix_dpi = INTTOFIX(96);

	if (dpi > 10) {
		fix_dpi = INTTOFIX(dpi);
	}

	ReleaseDC(hwnd, hdc);

	LOG(("FIX DPI %x", fix_dpi));

	return fix_dpi;
}

/**
 * creation of a new full browser window
 */
static HWND nsws_window_create(struct gui_window *gw)
{
	HWND hwnd;
	INITCOMMONCONTROLSEX icc;

	LOG(("GUI window %p", gw));

	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_BAR_CLASSES | ICC_WIN95_CLASSES;
#if WINVER > 0x0501
	icc.dwICC |= ICC_STANDARD_CLASSES;
#endif
	InitCommonControlsEx(&icc);

	gw->mainmenu = LoadMenu(hinstance, MAKEINTRESOURCE(IDR_MENU_MAIN));
	gw->rclick = LoadMenu(hinstance, MAKEINTRESOURCE(IDR_MENU_CONTEXT));

	LOG(("creating window for hInstance %p", hinstance));
	hwnd = CreateWindowEx(0,
			      windowclassname_main,
			      "NetSurf Browser",
			      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CS_DBLCLKS,
			      CW_USEDEFAULT,
			      CW_USEDEFAULT,
			      gw->width,
			      gw->height,
			      NULL,
			      gw->mainmenu,
			      hinstance,
			      NULL);

	if (hwnd == NULL) {
		LOG(("Window create failed"));
		return NULL;
	}

	/* set the gui window associated with this browser */
	SetProp(hwnd, TEXT("GuiWnd"), (HANDLE)gw);

	nscss_screen_dpi = get_window_dpi(hwnd);

	if ((option_window_width >= 100) &&
	    (option_window_height >= 100) &&
	    (option_window_x >= 0) &&
	    (option_window_y >= 0)) {
		LOG(("Setting Window position %d,%d %d,%d",
		     option_window_x, option_window_y,
		     option_window_width, option_window_height));
		SetWindowPos(hwnd, HWND_TOP,
			     option_window_x, option_window_y,
			     option_window_width, option_window_height,
			     SWP_SHOWWINDOW);
	}

	nsws_window_set_accels(gw);
	nsws_window_set_ico(gw);

	return hwnd;
}

/**
 * create a new gui_window to contain a browser_window
 * \param bw the browser_window to connect to the new gui_window
 */
struct gui_window *
gui_create_browser_window(struct browser_window *bw,
			  struct browser_window *clone,
			  bool new_tab)
{
	struct gui_window *gw;

	LOG(("Creating gui window for browser window %p", bw));

	gw = calloc(1, sizeof(struct gui_window));

	if (gw == NULL) {
		return NULL;
	}

	/* connect gui window to browser window */
	gw->bw = bw;

	gw->width = 800;
	gw->height = 600;
	gw->toolbuttonsize = 24;
	gw->requestscrollx = 0;
	gw->requestscrolly = 0;
	gw->localhistory = NULL;

	gw->mouse = malloc(sizeof(struct browser_mouse));
	if (gw->mouse == NULL) {
		free(gw);
		LOG(("Unable to allocate mouse state"));
		return NULL;
	}
	gw->mouse->gui = gw;
	gw->mouse->state = 0;
	gw->mouse->pressed_x = 0;
	gw->mouse->pressed_y = 0;

	/* add window to list */
	if (window_list != NULL)
		window_list->prev = gw;
	gw->next = window_list;
	window_list = gw;

	switch(bw->browser_window_type) {
	case BROWSER_WINDOW_NORMAL:
		gw->main = nsws_window_create(gw);
		gw->toolbar = nsws_window_toolbar_create(gw, gw->main);
		gw->statusbar = nsws_window_statusbar_create(gw);

		gw->drawingarea = CreateWindow(windowclassname_drawable,
					       NULL,
					       WS_VISIBLE | WS_CHILD,
					       0, 0, 0, 0,
					       gw->main,
					       NULL,
					       hinstance,
					       NULL);
		LOG(("BROWSER_WINDOW_NORMAL: main:%p toolbar:%p statusbar %p drawingarea %p", gw->main, gw->toolbar, gw->statusbar, gw->drawingarea));


		/* set the gui window associated with this toolbar */
		SetProp(gw->drawingarea, TEXT("GuiWnd"), (HANDLE)gw);

		font_hwnd = gw->drawingarea;
		input_window = gw;
		open_windows++;
		ShowWindow(gw->main, SW_SHOWNORMAL);
		break;

	case BROWSER_WINDOW_FRAME:
		gw->drawingarea = CreateWindow(windowclassname_drawable,
					       NULL,
					       WS_VISIBLE | WS_CHILD,
					       0,  0,  0,  0,
					       bw->parent->window->drawingarea,
					       NULL,
					       hinstance,
					       NULL);
		/* set the gui window associated with this toolbar */
		SetProp(gw->drawingarea, TEXT("GuiWnd"), (HANDLE)gw);

		ShowWindow(gw->drawingarea, SW_SHOWNORMAL);
		LOG(("create frame"));
		break;

	case BROWSER_WINDOW_FRAMESET:
		LOG(("create frameset"));
		break;

	case BROWSER_WINDOW_IFRAME:
		LOG(("create iframe"));
		gw->drawingarea = CreateWindow(windowclassname_drawable,
					       NULL,
					       WS_VISIBLE | WS_CHILD,
					       0, 0, 0, 0,
					       bw->parent->window->drawingarea,
					       NULL,
					       hinstance,
					       NULL);

		/* set the gui window associated with this toolbar */
		SetProp(gw->drawingarea, TEXT("GuiWnd"), (HANDLE)gw);

		ShowWindow(gw->drawingarea, SW_SHOWNORMAL);
		break;

	default:
		LOG(("unhandled type"));
	}

	return gw;
}




HICON nsws_window_get_ico(bool large)
{
	return large ? hIcon : hIconS;
}




/**
 * cache pointers for quick swapping
 */
static void nsws_window_init_pointers(void)
{
	nsws_pointer.hand = LoadCursor(NULL, IDC_HAND);
	nsws_pointer.ibeam = LoadCursor(NULL, IDC_IBEAM);
	nsws_pointer.cross = LoadCursor(NULL, IDC_CROSS);
	nsws_pointer.sizeall = LoadCursor(NULL, IDC_SIZEALL);
	nsws_pointer.sizewe = LoadCursor(NULL, IDC_SIZEWE);
	nsws_pointer.sizens = LoadCursor(NULL, IDC_SIZENS);
	nsws_pointer.sizenesw = LoadCursor(NULL, IDC_SIZENESW);
	nsws_pointer.sizenwse = LoadCursor(NULL, IDC_SIZENWSE);
	nsws_pointer.wait = LoadCursor(NULL, IDC_WAIT);
	nsws_pointer.appstarting = LoadCursor(NULL, IDC_APPSTARTING);
	nsws_pointer.no = LoadCursor(NULL, IDC_NO);
	nsws_pointer.help = LoadCursor(NULL, IDC_HELP);
	nsws_pointer.arrow = LoadCursor(NULL, IDC_ARROW);
}



HWND gui_window_main_window(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->main;
}

HWND gui_window_toolbar(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->toolbar;
}

HWND gui_window_urlbar(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->urlbar;
}

HWND gui_window_statusbar(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->statusbar;
}

HWND gui_window_drawingarea(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->drawingarea;
}

struct nsws_localhistory *gui_window_localhistory(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->localhistory;
}

void gui_window_set_localhistory(struct gui_window *w,
				 struct nsws_localhistory *l)
{
	if (w != NULL)
		w->localhistory = l;
}

RECT *gui_window_redraw_rect(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return &(w->redraw);
}

int gui_window_width(struct gui_window *w)
{
	if (w == NULL)
		return 0;
	return w->width;
}

int gui_window_height(struct gui_window *w)
{
	if (w == NULL)
		return 0;
	return w->height;
}

int gui_window_scrollingx(struct gui_window *w)
{
	if (w == NULL)
		return 0;
	return w->requestscrollx;
}

int gui_window_scrollingy(struct gui_window *w)
{
	if (w == NULL)
		return 0;
	return w->requestscrolly;
}

struct gui_window *gui_window_iterate(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->next;
}

struct browser_window *gui_window_browser_window(struct gui_window *w)
{
	if (w == NULL)
		return NULL;
	return w->bw;
}

/**
 * window cleanup code
 */
void gui_window_destroy(struct gui_window *w)
{
	if (w == NULL)
		return;

	if (w->prev != NULL)
		w->prev->next = w->next;
	else
		window_list = w->next;

	if (w->next != NULL)
		w->next->prev = w->prev;

	DestroyAcceleratorTable(w->acceltable);

	free(w);
	w = NULL;
}

/**
 * set window title
 * \param title the [url]
 */
void gui_window_set_title(struct gui_window *w, const char *title)
{
	if (w == NULL)
		return;
	LOG(("%p, title %s", w, title));
	char *fulltitle = malloc(strlen(title) +
				 SLEN("  -  NetSurf") + 1);
	if (fulltitle == NULL) {
		warn_user("NoMemory", 0);
		return;
	}
	strcpy(fulltitle, title);
	strcat(fulltitle, "  -  NetSurf");
	SendMessage(w->main, WM_SETTEXT, 0, (LPARAM)fulltitle);
	free(fulltitle);
}

/**
 * redraw the whole window
 */
void gui_window_redraw_window(struct gui_window *gw)
{
	/* LOG(("gw:%p", gw)); */
	if (gw == NULL)
		return;

	RedrawWindow(gw->drawingarea, NULL, NULL, RDW_INVALIDATE | RDW_NOERASE);
}

void gui_window_update_box(struct gui_window *gw,
			   const union content_msg_data *data)
{
	/* LOG(("gw:%p %f,%f %f,%f", gw, data->redraw.x, data->redraw.y, data->redraw.width, data->redraw.height)); */

	if (gw == NULL)
		return;

	RECT redrawrect;

	redrawrect.left = (long)data->redraw.x - (gw->scrollx / gw->bw->scale);
	redrawrect.top = (long)data->redraw.y - (gw->scrolly / gw->bw->scale);
	redrawrect.right =(long)(data->redraw.x + data->redraw.width);
	redrawrect.bottom = (long)(data->redraw.y + data->redraw.height);

	RedrawWindow(gw->drawingarea, &redrawrect, NULL, RDW_INVALIDATE | RDW_NOERASE);

}

bool gui_window_get_scroll(struct gui_window *w, int *sx, int *sy)
{
	LOG(("get scroll"));
	if (w == NULL)
		return false;
	*sx = w->scrollx;
	*sy = w->scrolly;

	return true;
}

/**
 * scroll the window
 * \param sx the new 'absolute' scroll location
 * \param sy the new 'absolute' scroll location
 */
void gui_window_set_scroll(struct gui_window *w, int sx, int sy)
{
	SCROLLINFO si;
	POINT p;

	if ((w == NULL) ||
	    (w->bw == NULL) ||
	    (w->bw->current_content == NULL))
		return;

	/* limit scale range */
	if (abs(w->bw->scale - 0.0) < 0.00001)
		w->bw->scale = 1.0;

	w->requestscrollx = sx - w->scrollx;
	w->requestscrolly = sy - w->scrolly;

	/* set the vertical scroll offset */
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = (content_get_height(w->bw->current_content) * w->bw->scale) - 1;
	si.nPage = w->height;
	si.nPos = MAX(w->scrolly + w->requestscrolly, 0);
	si.nPos = MIN(si.nPos, content_get_height(w->bw->current_content) * w->bw->scale - w->height);
	SetScrollInfo(w->drawingarea, SB_VERT, &si, TRUE);
	LOG(("SetScrollInfo VERT min:%d max:%d page:%d pos:%d", si.nMin, si.nMax, si.nPage, si.nPos));

	/* set the horizontal scroll offset */
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = (content_get_width(w->bw->current_content) * w->bw->scale) -1;
	si.nPage = w->width;
	si.nPos = MAX(w->scrollx + w->requestscrollx, 0);
	si.nPos = MIN(si.nPos, content_get_width(w->bw->current_content) * w->bw->scale - w->width);
	SetScrollInfo(w->drawingarea, SB_HORZ, &si, TRUE);
	LOG(("SetScrollInfo HORZ min:%d max:%d page:%d pos:%d", si.nMin, si.nMax, si.nPage, si.nPos));

	/* Set caret position */
	GetCaretPos(&p);
	HideCaret(w->drawingarea);
	SetCaretPos(p.x - w->requestscrollx, p.y - w->requestscrolly);
	ShowCaret(w->drawingarea);

	RECT r, redraw;
	r.top = 0;
	r.bottom = w->height + 1;
	r.left = 0;
	r.right = w->width + 1;
	ScrollWindowEx(w->drawingarea, - w->requestscrollx, - w->requestscrolly, &r, NULL, NULL, &redraw, SW_INVALIDATE);
	w->scrolly += w->requestscrolly;
	w->scrollx += w->requestscrollx;
	w->requestscrollx = 0;
	w->requestscrolly = 0;

}

void gui_window_scroll_visible(struct gui_window *w, int x0, int y0,
			       int x1, int y1)
{
	LOG(("scroll visible %s:(%p, %d, %d, %d, %d)", __func__, w, x0,
	     y0, x1, y1));
}

void gui_window_position_frame(struct gui_window *w, int x0, int y0,
			       int x1, int y1)
{
	if (w == NULL)
		return;
	LOG(("position frame %s: %d, %d, %d, %d", w->bw->name,
	     x0, y0, x1, y1));
	MoveWindow(w->drawingarea, x0, y0, x1-x0, y1-y0, true);
}

void gui_window_get_dimensions(struct gui_window *w, int *width, int *height,
			       bool scaled)
{
	if (w == NULL)
		return;

	LOG(("get dimensions %p w=%d h=%d", w, w->width, w->height));

	*width = w->width;
	*height = w->height;
}

void gui_window_update_extent(struct gui_window *w)
{

}

/**
 * set the status bar message
 */
void gui_window_set_status(struct gui_window *w, const char *text)
{
	if (w == NULL)
		return;
	SendMessage(w->statusbar, WM_SETTEXT, 0, (LPARAM)text);
}

/**
 * set the pointer shape
 */
void gui_window_set_pointer(struct gui_window *w, gui_pointer_shape shape)
{
	if (w == NULL)
		return;

	LOG(("shape %d", shape));
	switch (shape) {
	case GUI_POINTER_POINT: /* link */
	case GUI_POINTER_MENU:
		SetCursor(nsws_pointer.hand);
		break;

	case GUI_POINTER_CARET: /* input */
		SetCursor(nsws_pointer.ibeam);
		break;

	case GUI_POINTER_CROSS:
		SetCursor(nsws_pointer.cross);
		break;

	case GUI_POINTER_MOVE:
		SetCursor(nsws_pointer.sizeall);
		break;

	case GUI_POINTER_RIGHT:
	case GUI_POINTER_LEFT:
		SetCursor(nsws_pointer.sizewe);
		break;

	case GUI_POINTER_UP:
	case GUI_POINTER_DOWN:
		SetCursor(nsws_pointer.sizens);
		break;

	case GUI_POINTER_RU:
	case GUI_POINTER_LD:
		SetCursor(nsws_pointer.sizenesw);
		break;

	case GUI_POINTER_RD:
	case GUI_POINTER_LU:
		SetCursor(nsws_pointer.sizenwse);
		break;

	case GUI_POINTER_WAIT:
		SetCursor(nsws_pointer.wait);
		break;

	case GUI_POINTER_PROGRESS:
		SetCursor(nsws_pointer.appstarting);
		break;

	case GUI_POINTER_NO_DROP:
	case GUI_POINTER_NOT_ALLOWED:
		SetCursor(nsws_pointer.no);
		break;

	case GUI_POINTER_HELP:
		SetCursor(nsws_pointer.help);
		break;

	default:
		SetCursor(nsws_pointer.arrow);
		break;
	}
}

struct nsws_pointers *nsws_get_pointers(void)
{
	return &nsws_pointer;
}

void gui_window_hide_pointer(struct gui_window *w)
{
}

void gui_window_set_url(struct gui_window *w, const char *url)
{
	if (w == NULL)
		return;
	SendMessage(w->urlbar, WM_SETTEXT, 0, (LPARAM) url);
}


void gui_window_start_throbber(struct gui_window *w)
{
	if (w == NULL)
		return;
	nsws_window_update_forward_back(w);

	if (w->mainmenu != NULL) {
		EnableMenuItem(w->mainmenu, IDM_NAV_STOP, MF_ENABLED);
		EnableMenuItem(w->mainmenu, IDM_NAV_RELOAD, MF_GRAYED);
	}
	if (w->rclick != NULL) {
		EnableMenuItem(w->rclick, IDM_NAV_STOP, MF_ENABLED);
		EnableMenuItem(w->rclick, IDM_NAV_RELOAD, MF_GRAYED);
	}
	if (w->toolbar != NULL) {
		SendMessage(w->toolbar, TB_SETSTATE, (WPARAM) IDM_NAV_STOP,
			    MAKELONG(TBSTATE_ENABLED, 0));
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) IDM_NAV_RELOAD,
			    MAKELONG(TBSTATE_INDETERMINATE, 0));
	}
	w->throbbing = true;
	Animate_Play(w->throbber, 0, -1, -1);
}

void gui_window_stop_throbber(struct gui_window *w)
{
	if (w == NULL)
		return;
	nsws_window_update_forward_back(w);
	if (w->mainmenu != NULL) {
		EnableMenuItem(w->mainmenu, IDM_NAV_STOP, MF_GRAYED);
		EnableMenuItem(w->mainmenu, IDM_NAV_RELOAD, MF_ENABLED);
	}
	if (w->rclick != NULL) {
		EnableMenuItem(w->rclick, IDM_NAV_STOP, MF_GRAYED);
		EnableMenuItem(w->rclick, IDM_NAV_RELOAD, MF_ENABLED);
	}
	if (w->toolbar != NULL) {
		SendMessage(w->toolbar, TB_SETSTATE, (WPARAM) IDM_NAV_STOP,
			    MAKELONG(TBSTATE_INDETERMINATE, 0));
		SendMessage(w->toolbar, TB_SETSTATE,
			    (WPARAM) IDM_NAV_RELOAD,
			    MAKELONG(TBSTATE_ENABLED, 0));
	}
	w->throbbing = false;
	Animate_Stop(w->throbber);
	Animate_Seek(w->throbber, 0);
}

/**
 * place caret in window
 */
void gui_window_place_caret(struct gui_window *w, int x, int y, int height)
{
	if (w == NULL)
		return;
	CreateCaret(w->drawingarea, (HBITMAP)NULL, 1, height * w->bw->scale);
	SetCaretPos(x * w->bw->scale - w->scrollx,
		    y * w->bw->scale - w->scrolly);
	ShowCaret(w->drawingarea);
}

/**
 * clear window caret
 */
void
gui_window_remove_caret(struct gui_window *w)
{
	if (w == NULL)
		return;
	HideCaret(w->drawingarea);
}

void
gui_window_set_icon(struct gui_window *g, hlcache_handle *icon)
{
}

void
gui_window_set_search_ico(hlcache_handle *ico)
{
}

bool
save_complete_gui_save(const char *path,
		       const char *filename,
		       size_t len,
		       const char *sourcedata,
		       content_type type)
{
	return false;
}

int
save_complete_htmlSaveFileFormat(const char *path,
				 const char *filename,
				 xmlDocPtr cur,
				 const char *encoding,
				 int format)
{
	return 0;
}


void gui_window_new_content(struct gui_window *w)
{
}

bool gui_window_scroll_start(struct gui_window *w)
{
	return true;
}

bool gui_window_box_scroll_start(struct gui_window *w,
				 int x0, int y0, int x1, int y1)
{
	return true;
}

bool gui_window_frame_resize_start(struct gui_window *w)
{
	LOG(("resize frame\n"));
	return true;
}

void gui_window_save_link(struct gui_window *g, const char *url,
			  const char *title)
{
}

void gui_window_set_scale(struct gui_window *w, float scale)
{
	if (w == NULL)
		return;
	w->scale = scale;
	LOG(("%.2f\n", scale));
}

void gui_drag_save_object(gui_save_type type, hlcache_handle *c,
			  struct gui_window *w)
{
}

void gui_drag_save_selection(struct selection *s, struct gui_window *w)
{
}

void gui_start_selection(struct gui_window *w)
{
}

void gui_clear_selection(struct gui_window *w)
{
}

void gui_paste_from_clipboard(struct gui_window *w, int x, int y)
{
	HANDLE clipboard_handle;
	char *content;

	clipboard_handle = GetClipboardData(CF_TEXT);
	if (clipboard_handle != NULL) {
		content = GlobalLock(clipboard_handle);
		LOG(("pasting %s", content));
		GlobalUnlock(clipboard_handle);
	}
}

bool gui_empty_clipboard(void)
{
	return false;
}

bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
	HANDLE hnew;
	char *new, *original;
	HANDLE h = GetClipboardData(CF_TEXT);
	if (h == NULL)
		original = (char *)"";
	else
		original = GlobalLock(h);

	size_t len = strlen(original) + 1;
	hnew = GlobalAlloc(GHND, length + len);
	new = (char *)GlobalLock(hnew);
	snprintf(new, length + len, "%s%s", original, text);

	if (h != NULL) {
		GlobalUnlock(h);
		EmptyClipboard();
	}
	GlobalUnlock(hnew);
	SetClipboardData(CF_TEXT, hnew);
	return true;
}

bool gui_commit_clipboard(void)
{
	return false;
}

static bool
gui_selection_traverse_handler(const char *text,
			       size_t length,
			       struct box *box,
			       void *handle,
			       const char *space_text,
			       size_t space_length)
{
	bool add_space = box != NULL ? box->space != 0 : false;

	if (space_text != NULL && space_length > 0) {
		if (!gui_add_to_clipboard(space_text, space_length, false)) {
			return false;
		}
	}

	if (!gui_add_to_clipboard(text, length, add_space))
		return false;

	return true;
}

bool gui_copy_to_clipboard(struct selection *s)
{
	if ((s->defined) && (s->bw != NULL) && (s->bw->window != NULL) &&
	    (s->bw->window->main != NULL)) {
		OpenClipboard(s->bw->window->main);
		EmptyClipboard();
		if (selection_traverse(s, gui_selection_traverse_handler,
				       NULL)) {
			CloseClipboard();
			return true;
		}
	}
	return false;
}


void gui_create_form_select_menu(struct browser_window *bw,
				 struct form_control *control)
{
}

void gui_launch_url(const char *url)
{
}

void gui_cert_verify(const char *url, const struct ssl_cert_info *certs,
		     unsigned long num,
		     nserror (*cb)(bool proceed, void *pw), void *cbpw)
{
	cb(false, cbpw);
}


void gui_quit(void)
{
	LOG(("gui_quit"));
}

char* gui_get_resource_url(const char *filename)
{
	return NULL;
}

static void gui_init(int argc, char** argv)
{
	char buf[PATH_MAX], sbuf[PATH_MAX];
	int len;
	struct browser_window *bw;
	const char *addr = NETSURF_HOMEPAGE;

	LOG(("argc %d, argv %p", argc, argv));

	/* set up stylesheet urls */
	getcwd(sbuf, PATH_MAX);
	len = strlen(sbuf);
	strncat(sbuf, "windows/res/default.css", PATH_MAX - len);
	nsws_find_resource(buf, "default.css", sbuf);
	default_stylesheet_url = path_to_url(buf);
	LOG(("Using '%s' as Default CSS URL", default_stylesheet_url));

	getcwd(sbuf, PATH_MAX);
	len = strlen(sbuf);
	strncat(sbuf, "windows/res/quirks.css", PATH_MAX - len);
	nsws_find_resource(buf, "quirks.css", sbuf);
	quirks_stylesheet_url = path_to_url(buf);
	LOG(("Using '%s' as quirks stylesheet url", quirks_stylesheet_url ));


	create_local_windows_classes();

	option_target_blank = false;

	nsws_window_init_pointers();
	LOG(("argc %d, argv %p", argc, argv));

	/* ensure homepage option has a default */
	if (option_homepage_url == NULL || option_homepage_url[0] == '\0')
		option_homepage_url = strdup(default_page);

	/* If there is a url specified on the command line use it */
	if (argc > 1)
		addr = argv[1];
	else
		addr = option_homepage_url;

	LOG(("calling browser_window_create"));
	bw = browser_window_create(addr, 0, 0, true, false);

}

void gui_stdout(void)
{
	/* mwindows compile flag normally invalidates stdout unless
	   already redirected */
	if (_get_osfhandle(fileno(stdout)) == -1) {
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
	}
}

/* OS program entry point */
int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hLastInstance, LPSTR lpcli, int ncmd)
{
	char **argv = NULL;
	int argc = 0, argctemp = 0;
	size_t len;
	LPWSTR *argvw;
	char options[PATH_MAX];
	char messages[PATH_MAX];

	if (SLEN(lpcli) > 0) {
		argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	}

	hinstance = hInstance;
	setbuf(stderr, NULL);

	/* Construct a unix style argc/argv */
	argv = malloc(sizeof(char *) * argc);
	while (argctemp < argc) {
		len = wcstombs(NULL, argvw[argctemp], 0) + 1;
		if (len > 0) {
			argv[argctemp] = malloc(len);
		}

		if (argv[argctemp] != NULL) {
			wcstombs(argv[argctemp], argvw[argctemp], len);
			/* alter windows-style forward slash flags to
			 * hyphen flags.
			 */
			if (argv[argctemp][0] == '/')
				argv[argctemp][0] = '-';
		}
		argctemp++;
	}

	/* load browser messages */
	nsws_find_resource(messages, "messages", "./windows/res/messages");

	/* load browser options */
	nsws_find_resource(options, "preferences", "~/.netsurf/preferences");
	options_file_location = strdup(options);

	/* initialise netsurf */
	netsurf_init(&argc, &argv, options, messages);

	gui_init(argc, argv);

	netsurf_main_loop();

	netsurf_exit();

	return 0;
}

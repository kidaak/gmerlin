/*****************************************************************
 
  x11_window.c
 
  Copyright (c) 2003-2004 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <stdio.h>
#include <string.h>
#include <X11/Xatom.h>

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#include <x11_window.h>

#include <stdlib.h>

#include <time.h>
#include <sys/time.h>


#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */

// #define FULLSCREEN_MODE_OLD    0
#define FULLSCREEN_MODE_NET_FULLSCREEN (1<<0)
#define FULLSCREEN_MODE_NET_ABOVE      (1<<1)

#define FULLSCREEN_MODE_WIN_LAYER      (1<<2)


#define IDLE_MAX 10

static char bm_no_data[] = { 0,0,0,0, 0,0,0,0 };


/* Screensaver detection */

static void check_screensaver(x11_window_t * w)
  {
  char * env;
  
  /* Check for gnome */
  env = getenv("GNOME_DESKTOP_SESSION_ID");
  if(env)
    {
    w->screensaver_mode = SCREENSAVER_MODE_GNOME;
    return;
    }

  /* Check for KDE */
  env = getenv("KDE_FULL_SESSION");
  if(env && !strcmp(env, "true"))
    {
    w->screensaver_mode = SCREENSAVER_MODE_KDE;
    return;
    }

  /* TODO: xfce4 */
  
  }

static void disable_screensaver(x11_window_t * w)
  {
  int interval, prefer_blank, allow_exp;

#if HAVE_XDPMS
  int nothing;
#endif // HAVE_XDPMS
  
  if(w->screensaver_disabled)
    return;

#if HAVE_XDPMS
  if(DPMSQueryExtension(w->dpy, &nothing, &nothing))
    {
    BOOL onoff;
    CARD16 state;
    
    DPMSInfo(w->dpy, &state, &onoff);
    if(onoff)
      {
      w->dpms_disabled = 1;
      DPMSDisable(w->dpy);       // monitor powersave off
      }
    }
#endif // HAVE_XDPMS
    
  switch(w->screensaver_mode)
    {
    case SCREENSAVER_MODE_XLIB:
      XGetScreenSaver(w->dpy, &w->screensaver_saved_timeout,
                      &interval, &prefer_blank,
                      &allow_exp);
      if(w->screensaver_saved_timeout)
        w->screensaver_was_enabled = 1;
      else
        w->screensaver_was_enabled = 0;
      XSetScreenSaver(w->dpy, 0, interval, prefer_blank, allow_exp);
      break;
    case SCREENSAVER_MODE_GNOME:
      break;
    case SCREENSAVER_MODE_KDE:
      w->screensaver_was_enabled =
        (system
             ("dcop kdesktop KScreensaverIface isEnabled 2>/dev/null | sed 's/1/true/g' | grep true 2>/dev/null >/dev/null")
         == 0);
      
      if(w->screensaver_was_enabled)
        system("dcop kdesktop KScreensaverIface enable false > /dev/null");
      break;
    }
  w->screensaver_disabled = 1;
  }

static void enable_screensaver(x11_window_t * w)
  {
  int dummy, interval, prefer_blank, allow_exp;

  if(!w->screensaver_disabled)
    return;

#if HAVE_XDPMS
  if(w->dpms_disabled)
    {
    if(DPMSQueryExtension(w->dpy, &dummy, &dummy))
      {
      if(DPMSEnable(w->dpy))
        {
        // DPMS does not seem to be enabled unless we call DPMSInfo
        BOOL onoff;
        CARD16 state;

        DPMSForceLevel(w->dpy, DPMSModeOn);      
        DPMSInfo(w->dpy, &state, &onoff);
        }

      }
    w->dpms_disabled = 0;
    }
#endif // HAVE_XDPMS
  
  w->screensaver_disabled = 0;
  
  
  if(!w->screensaver_was_enabled)
    return;
  
  switch(w->screensaver_mode)
    {
    case SCREENSAVER_MODE_XLIB:
      XGetScreenSaver(w->dpy, &dummy, &interval, &prefer_blank,
                      &allow_exp);
      XSetScreenSaver(w->dpy, w->screensaver_saved_timeout, interval, prefer_blank,
                      allow_exp);
      break;
    case SCREENSAVER_MODE_GNOME:
      break;
    case SCREENSAVER_MODE_KDE:
      break;
    }
  }

static void ping_screensaver(x11_window_t * w)
  {
  struct timeval tm;
  gettimeofday(&tm, (struct timezone *)0);

  if(tm.tv_sec - w->screensaver_last_ping_time < 40) // 40 Sec interval
    {
    return;
    }

  w->screensaver_last_ping_time = tm.tv_sec;
  
  switch(w->screensaver_mode)
    {
    case SCREENSAVER_MODE_XLIB:
      break;
    case SCREENSAVER_MODE_GNOME:
      system("gnome-screensaver-command --poke > /dev/null 2> /dev/null");
      break;
    case SCREENSAVER_MODE_KDE:
      break;
    }
  }



static int
wm_check_capability(Display *dpy, Window root, Atom list, Atom wanted)
  {
  Atom            type;
  int             format;
  unsigned int    i;
  unsigned long   nitems, bytesafter;
  unsigned char   *args;
  unsigned long   *ldata;
  int             retval = 0;
  
  if (Success != XGetWindowProperty
      (dpy, root, list, 0, 16384, False,
       AnyPropertyType, &type, &format, &nitems, &bytesafter, &args))
    return 0;
  if (type != XA_ATOM)
    return 0;
  ldata = (unsigned long*)args;
  for (i = 0; i < nitems; i++)
    {
    if (ldata[i] == wanted)
      retval = 1;

    }
  XFree(ldata);
  return retval;
  }

static void
netwm_set_state(x11_window_t * w, Window win, int action, Atom state)
  {
  /* Setting _NET_WM_STATE by XSendEvent works only, if the window
     is already mapped!! */
  
  XEvent e;
  memset(&e,0,sizeof(e));
  e.xclient.type = ClientMessage;
  e.xclient.message_type = w->_NET_WM_STATE;
  e.xclient.display = w->dpy;
  e.xclient.window = win;
  e.xclient.send_event = True;
  e.xclient.format = 32;
  e.xclient.data.l[0] = action;
  e.xclient.data.l[1] = state;
  
  XSendEvent(w->dpy, w->root, False,
             SubstructureRedirectMask, &e);
  }

#if 0
static void
netwm_set_fullscreen(x11_window_t * w, Window win)
  {
  long                  propvalue[2];
    
  propvalue[0] = w->_NET_WM_STATE_FULLSCREEN;
  propvalue[1] = 0;

  XChangeProperty (w->dpy, win, 
		   w->_NET_WM_STATE, XA_ATOM, 
		       32, PropModeReplace, 
		       (unsigned char *)propvalue, 1);
  XFlush(w->dpy);



  }
#endif

#if 0
static void
netwm_set_layer(x11_window_t * w, Window win, int layer)
  {
  XEvent e;
  
  memset(&e,0,sizeof(e));
  e.xclient.type = ClientMessage;
  e.xclient.message_type = w->_NET_WM_STATE;
  e.xclient.display = w->dpy;
  e.xclient.window = win;
  e.xclient.format = 32;
  e.xclient.data.l[0] = operation;
  e.xclient.data.l[1] = state;
  
  XSendEvent(w->dpy, w->root, False,
             SubstructureRedirectMask, &e);
  
  }
#endif

static int get_fullscreen_mode(x11_window_t * w)
  {
  int ret = 0;

  Atom            type;
  int             format;
  unsigned int    i;
  unsigned long   nitems, bytesafter;
  unsigned char   *args;
  unsigned long   *ldata;
  if (Success != XGetWindowProperty
      (w->dpy, w->root, w->_NET_SUPPORTED, 0, (65536 / sizeof(long)), False,
       AnyPropertyType, &type, &format, &nitems, &bytesafter, &args))
    return 0;
  if (type != XA_ATOM)
    return 0;
  ldata = (unsigned long*)args;
  for (i = 0; i < nitems; i++)
    {
    if (ldata[i] == w->_NET_WM_STATE_FULLSCREEN)
      {
      ret |= FULLSCREEN_MODE_NET_FULLSCREEN;
      }
    if (ldata[i] == w->_NET_WM_STATE_ABOVE)
      {
      ret |= FULLSCREEN_MODE_NET_ABOVE;
      }

    
    }
  XFree(ldata);
  
  if(wm_check_capability(w->dpy, w->root, w->WIN_PROTOCOLS,
                         w->WIN_LAYER))
    {
    ret |= FULLSCREEN_MODE_WIN_LAYER;
    }
  
  return ret;
  }

static void init_atoms(x11_window_t * w)
  {
  w->WM_DELETE_WINDOW         = XInternAtom(w->dpy, "WM_DELETE_WINDOW", False);

  w->WIN_PROTOCOLS             = XInternAtom(w->dpy, "WIN_PROTOCOLS", False);
  w->WM_PROTOCOLS             = XInternAtom(w->dpy, "WM_PROTOCOLS", False);
  w->WIN_LAYER              = XInternAtom(w->dpy, "WIN_LAYER", False);

  w->_NET_SUPPORTED           = XInternAtom(w->dpy, "_NET_SUPPORTED", False);
  w->_NET_WM_STATE            = XInternAtom(w->dpy, "_NET_WM_STATE", False);
  w->_NET_WM_STATE_FULLSCREEN = XInternAtom(w->dpy, "_NET_WM_STATE_FULLSCREEN",
                                            False);
  
  w->_NET_WM_STATE_ABOVE = XInternAtom(w->dpy, "_NET_WM_STATE_ABOVE",
                                       False);

  w->_NET_MOVERESIZE_WINDOW   = XInternAtom(w->dpy, "_NET_MOVERESIZE_WINDOW",
                                            False);

  }


/* MWM decorations */

#define MWM_HINTS_DECORATIONS (1L << 1)
#define MWM_HINTS_FUNCTIONS     (1L << 0)

#define MWM_FUNC_ALL                 (1L<<0)
#define PROP_MOTIF_WM_HINTS_ELEMENTS 5
typedef struct
  {
  CARD32 flags;
  CARD32 functions;
  CARD32 decorations;
  INT32 inputMode;
    CARD32 status;
  } PropMotifWmHints;

static
int mwm_set_decorations(x11_window_t * w, Window win, int set)
  {
  PropMotifWmHints motif_hints;
  Atom hintsatom;
  
  /* setup the property */
  motif_hints.flags = MWM_HINTS_DECORATIONS | MWM_HINTS_FUNCTIONS;
  motif_hints.decorations = set;
  motif_hints.functions   = set ? MWM_FUNC_ALL : 0;
  
  /* get the atom for the property */
  hintsatom = XInternAtom(w->dpy, "_MOTIF_WM_HINTS", False);
  
  XChangeProperty(w->dpy, win, hintsatom, hintsatom, 32, PropModeReplace,
                  (unsigned char *) &motif_hints, PROP_MOTIF_WM_HINTS_ELEMENTS);
  return 1;
  }

static void set_decorations(x11_window_t * w, Window win, int decorations)
  {
  mwm_set_decorations(w, win, decorations);
  }

static void set_min_size(x11_window_t * w, Window win, int width, int height)
  {
  XSizeHints * h;
  h = XAllocSizeHints();

  h->flags = PMinSize;
  h->min_width = width;
  h->min_height = height;

  XSetWMNormalHints(w->dpy, win, h);
  
  XFree(h);
  }

int x11_window_create(x11_window_t * w, Window parent,
                      Display * dpy, Visual * visual, int depth,
                      int width, int height, const char * default_title)
  {
//  int i;
  /* Stuff for making the cursor */
  XColor black, dummy;
  Atom wm_protocols[1];
  
  XSetWindowAttributes attr;

#ifdef HAVE_LIBXINERAMA
  int foo,bar;
#endif
  
  w->dpy = dpy;
  
  w->root = DefaultRootWindow (w->dpy);

  w->parent = parent;
  if(w->parent == None)
    w->parent = w->root;
  else
    {
    Window root_return;
    int x_return;
    int y_return;
    unsigned int width_return;
    unsigned int height_return;
    unsigned int border_width_return;
    unsigned int depth_return;

    XGetGeometry(w->dpy, w->parent, &root_return,
                 &x_return, &y_return, &width_return,
                 &height_return, &border_width_return,
                 &depth_return);
    width  = width_return;
    height = height_return;
    
    w->is_embedded = 1;
    }
  
  w->window_width = width;
  w->window_height = height;

  init_atoms(w);

  check_screensaver(w);
  
  /* Get xinerama screens */

#ifdef HAVE_LIBXINERAMA
  if (XineramaQueryExtension(w->dpy,&foo,&bar) &&
      XineramaIsActive(w->dpy))
    {
    w->xinerama = XineramaQueryScreens(dpy,&(w->nxinerama));
    }
#endif

  /* Setup event mask */

  w->event_mask = StructureNotifyMask | PointerMotionMask | ExposureMask;
  
  w->colormap = XCreateColormap(w->dpy, RootWindow(w->dpy, w->screen),
                                visual,
                                AllocNone);

  /* Setup protocols */

  wm_protocols[0] = w->WM_DELETE_WINDOW;
  
  /* Create normal window */
  
  attr.backing_store = NotUseful;
  attr.border_pixel = 0;
  attr.background_pixel = 0;
  attr.colormap = w->colormap;
  attr.event_mask = w->event_mask;
  
  w->normal_window = XCreateWindow(w->dpy, w->parent,
                                   0 /* x */,
                                   0 /* y */,
                                   width, height,
                                   0 /* border_width */, depth,
                                   InputOutput, visual,
                                   (CWBackingStore | CWEventMask |
                                    CWBorderPixel | CWBackPixel | CWColormap),
                                   &attr);
  /* Create GC */
  
  w->gc = XCreateGC(w->dpy, w->normal_window, 0, NULL);

  if(!w->is_embedded)
    {
    set_decorations(w, w->normal_window, 1);
    XSetWMProtocols(w->dpy, w->normal_window, wm_protocols, 1);
    
    if(w->min_width && w->min_height)
      {
      set_min_size(w, w->normal_window, w->min_width, w->min_height);
      }
    }
  
  /* The fullscreen window will be created with the same size for now */

//  attr.override_redirect = True;
  w->fullscreen_window = XCreateWindow (w->dpy, w->root,
                                        0 /* x */,
                                        0 /* y */,
                                        width, height,
                                        0 /* border_width */, depth,
                                        InputOutput, visual,
                                        (CWBackingStore |CWEventMask |
                                         CWBorderPixel | CWBackPixel |
                                         CWColormap), &attr);

  set_decorations(w, w->fullscreen_window, 0);
  XSetWMProtocols(w->dpy, w->fullscreen_window, wm_protocols, 1);
  
  w->current_window = w->normal_window;
  
  /* Create colormap and fullscreen cursor */

  w->screen = DefaultScreen(w->dpy);
  
  w->fullscreen_cursor_pixmap =
    XCreateBitmapFromData(w->dpy, w->fullscreen_window,
                          bm_no_data, 8, 8);
  
  XAllocNamedColor(w->dpy, w->colormap, "black", &black, &dummy);
  w->fullscreen_cursor=
    XCreatePixmapCursor(w->dpy, w->fullscreen_cursor_pixmap,
                        w->fullscreen_cursor_pixmap,
                        &black, &black, 0, 0);

  w->black = BlackPixel(w->dpy, w->screen);

  /* Check, which fullscreen modes we have */

  w->fullscreen_mode = get_fullscreen_mode(w);

  x11_window_set_title(w, default_title);
  
  return 1;
  }

static void get_window_coords(x11_window_t * w,
                              Window win,
                              int * x, int * y, int * width,
                              int * height)
  {
  Window root_return;
  Window parent_return;
  Window * children_return;
  unsigned int nchildren_return;
  int x_return, y_return;
  unsigned int width_return, height_return;
  unsigned int border_width_return;
  unsigned int depth_return;
  //  Window child_return;
  
  XGetGeometry(w->dpy, win, &root_return, &x_return, &y_return,
               &width_return, &height_return,
               &border_width_return, &depth_return);
  
  XQueryTree(w->dpy, win, &root_return, &parent_return,
             &children_return, &nchildren_return);

  if(nchildren_return)
    {
    XFree(children_return);
    }

  *x = x_return;
  *y = y_return;
  *width  = width_return;
  *height = height_return;
    
  if(parent_return != root_return)
    {
    XGetGeometry(w->dpy, parent_return, &root_return,
                 &x_return, &y_return,
                 &width_return, &height_return,
                 &border_width_return, &depth_return);
    *x = x_return;
    *y = y_return;
    
    //    XTranslateCoordinates(w->dpy, parent_return, root_return,
    //                          x_return, y_return,
    //                          x,
    //                          y,
    //                          &child_return);
    }

  
  }

void x11_window_handle_event(x11_window_t * w, XEvent*evt)
  {
  w->do_delete = 0;

  if(!evt || (evt->type != MotionNotify))
    {
    w->idle_counter++;
    if(w->idle_counter == IDLE_MAX)
      {
      if(!w->pointer_hidden)
        {
        XDefineCursor(w->dpy, w->normal_window, w->fullscreen_cursor);
        XDefineCursor(w->dpy, w->fullscreen_window, w->fullscreen_cursor);
        XFlush(w->dpy);
        w->pointer_hidden = 1;
        }

      if(w->disable_screensaver)
        ping_screensaver(w);
      w->idle_counter = 0;
      }
    }
  if(!evt)
    return;
  
  switch(evt->type)
    {
    case ConfigureNotify:
      if(w->current_window != w->normal_window)
        break;
      //      w->window_width  = evt->xconfigure.width;
      //      w->window_height = evt->xconfigure.height;
      //      w->window_x = evt->xconfigure.x;
      //      w->window_y = evt->xconfigure.y;
      
      get_window_coords(w, w->normal_window,
                        &(w->window_x), &(w->window_y),
                        &(w->window_width), &(w->window_height));

      break;
    case MotionNotify:
      w->idle_counter = 0;
      if(w->pointer_hidden)
        {
        XDefineCursor(w->dpy, w->normal_window, None);
        XDefineCursor(w->dpy, w->fullscreen_window, None);
        XFlush(w->dpy);
        w->pointer_hidden = 0;
        }
      

    case ClientMessage:
      if((evt->xclient.message_type == w->WM_PROTOCOLS) &&
         (evt->xclient.data.l[0] == w->WM_DELETE_WINDOW))
        {
        w->do_delete = 1;
        }
      break;
    case UnmapNotify:
      if(evt->xunmap.window == w->normal_window)
        w->mapped = 0;
      break;
    case MapNotify:
      if(evt->xmap.window == w->normal_window)
        w->mapped = 1;
      break;
    }
  }

void x11_window_select_input(x11_window_t * w, long event_mask)
  {
  XSelectInput(w->dpy, w->normal_window, event_mask | w->event_mask);
  XSelectInput(w->dpy, w->fullscreen_window, event_mask | w->event_mask);
  }

static void get_fullscreen_coords(x11_window_t * w,
                                  int * x, int * y, int * width, int * height)
  {
#ifdef HAVE_LIBXINERAMA
  int x_return, y_return;
  int i;
  Window child;
  /* Get the coordinates of the normal window */

  *x = 0;
  *y = 0;
  *width  = DisplayWidth(w->dpy, w->screen);
  *height = DisplayHeight(w->dpy, w->screen);
  
  if(w->nxinerama)
    {
    XTranslateCoordinates(w->dpy, w->normal_window, w->root, 0, 0,
                          &x_return,
                          &y_return,
                          &child);
    
    /* Get the xinerama screen we are on */
    
    for(i = 0; i < w->nxinerama; i++)
      {
      if((x_return >= w->xinerama[i].x_org) &&
         (y_return >= w->xinerama[i].y_org) &&
         (x_return < w->xinerama[i].x_org + w->xinerama[i].width) &&
         (y_return < w->xinerama[i].y_org + w->xinerama[i].height))
        {
        *x = w->xinerama[i].x_org;
        *y = w->xinerama[i].y_org;
        *width = w->xinerama[i].width;
        *height = w->xinerama[i].height;
        break;
        }
      }
    }
#else
  *x = 0;
  *y = 0;
  *width  = DisplayWidth(w->dpy, w->screen);
  *height = DisplayHeight(w->dpy, w->screen);
#endif

  }


void x11_window_set_fullscreen(x11_window_t * w,int fullscreen)
  {
  int width;
  int height;
  int x;
  int y;
  XEvent evt;
  /* Normal->fullscreen */
  if(fullscreen && (w->current_window == w->normal_window))
    {
    
    get_fullscreen_coords(w, &x, &y, &width, &height);

    w->normal_width = w->window_width;
    w->normal_height = w->window_height;
    
    w->window_width = width;
    w->window_height = height;
    
    w->current_window = w->fullscreen_window;
    
    XMapRaised(w->dpy, w->fullscreen_window);

    if(w->disable_screensaver_fullscreen)
      {
      w->disable_screensaver = 1;
      disable_screensaver(w);
      }
    else
      {
      w->disable_screensaver = 0;
      enable_screensaver(w);
      }

    XSetTransientForHint(w->dpy, w->fullscreen_window, None);

//    XRaiseWindow(w->dpy, w->fullscreen_window);
//    XMapWindow(w->dpy, w->fullscreen_window);
    XMoveResizeWindow(w->dpy, w->fullscreen_window, x, y, width, height);
    
    
#if 1
    if(w->fullscreen_mode & FULLSCREEN_MODE_NET_ABOVE)
      {
      netwm_set_state(w, w->fullscreen_window,
                      _NET_WM_STATE_ADD, w->_NET_WM_STATE_ABOVE);
      }
    if(w->fullscreen_mode & FULLSCREEN_MODE_NET_FULLSCREEN)
      {
      netwm_set_state(w, w->fullscreen_window,
                      _NET_WM_STATE_ADD, w->_NET_WM_STATE_FULLSCREEN);
      }
#endif    

    XWithdrawWindow(w->dpy, w->normal_window, w->screen);
		
    /* Wait until the window is mapped */ 
    
    do
      {
      XMaskEvent(w->dpy, ExposureMask, &evt);
      x11_window_handle_event(w, &evt);
      } while((evt.type != Expose) && (evt.xexpose.window != w->fullscreen_window));

    XSetInputFocus(w->dpy, w->fullscreen_window, RevertToNone, CurrentTime);

//    XMoveResizeWindow(w->dpy, w->fullscreen_window, x, y, width, height);
//    XSync(w->dpy, False);
    x11_window_clear(w);
//    XRaiseWindow(w->dpy, w->fullscreen_window);
    XFlush(w->dpy);
    }
	
  if(!fullscreen && (w->current_window == w->fullscreen_window))
    {
    /* Unmap fullscreen window */
    netwm_set_state(w, w->fullscreen_window,
                    _NET_WM_STATE_REMOVE, w->_NET_WM_STATE_FULLSCREEN);
    netwm_set_state(w, w->fullscreen_window,
                    _NET_WM_STATE_REMOVE, w->_NET_WM_STATE_ABOVE);
    XWithdrawWindow(w->dpy, w->fullscreen_window, w->screen);
    XUnmapWindow(w->dpy, w->fullscreen_window);
        
    /* Map normal window */
    w->current_window = w->normal_window;
        
    XMapWindow(w->dpy, w->normal_window);

    if(w->disable_screensaver_normal)
      {
      w->disable_screensaver = 1;
      disable_screensaver(w);
      }
    else
      {
      w->disable_screensaver = 0;
      enable_screensaver(w);
      }
    w->window_width  = w->normal_width;
    w->window_height = w->normal_height;


    do
      {
      XMaskEvent(w->dpy, ExposureMask, &evt);
      x11_window_handle_event(w, &evt);
      } while((evt.type != Expose) && (evt.xexpose.window != w->normal_window));
    
    if(!w->is_embedded)
      XMoveResizeWindow(w->dpy, w->normal_window,
                        w->window_x, w->window_y,
                        w->window_width, w->window_height);
    
    XSetInputFocus(w->dpy, w->normal_window, RevertToNone, CurrentTime);
    
    x11_window_clear(w);
    XFlush(w->dpy);
    }
  }

void x11_window_destroy(x11_window_t * w)
  {
  XDestroyWindow(w->dpy, w->normal_window);
  XDestroyWindow(w->dpy, w->fullscreen_window);

  if(w->fullscreen_cursor)
    XFreeCursor(w->dpy, w->fullscreen_cursor);
  
  if(w->fullscreen_cursor_pixmap)
    XFreePixmap(w->dpy, w->fullscreen_cursor_pixmap);

  XFreeColormap(w->dpy, w->colormap);
  XFreeGC(w->dpy, w->gc);

#ifdef HAVE_LIBXINERAMA
  if(w->xinerama)
    XFree(w->xinerama);
#endif
  }

void x11_window_set_title(x11_window_t * w, const char * title)
  {
  XmbSetWMProperties(w->dpy, w->normal_window, title,
                     title, NULL, 0, NULL, NULL, NULL);

  }

void x11_window_set_class_hint(x11_window_t * w,
                               char * name,
                               char * class)
  {
  XClassHint xclasshint={name,class};
  XSetClassHint(w->dpy, w->normal_window, &xclasshint);
  XSetClassHint(w->dpy, w->fullscreen_window, &xclasshint);
  
  }

XEvent * x11_window_next_event(x11_window_t * w,
                               int milliseconds)
  {
  int fd;
  struct timeval timeout;
  fd_set read_fds;
  if(milliseconds < 0) /* Block */
    {
    XNextEvent(w->dpy, &(w->evt));
    return &(w->evt);
    }
  else if(!milliseconds)
    {
    if(!XPending(w->dpy))
      return (XEvent*)0;
    else
      {
      XNextEvent(w->dpy, &(w->evt));
      return &(w->evt);
      }
    }
  else /* Use timeout */
    {
    fd = ConnectionNumber(w->dpy);
    FD_ZERO (&read_fds);
    FD_SET (fd, &read_fds);

    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = 1000 * (milliseconds % 1000);
    if(!select(fd+1, &read_fds, (fd_set*)0,(fd_set*)0,&timeout))
      return (XEvent*)0;
    else
      {
      XNextEvent(w->dpy, &(w->evt));
      return &(w->evt);
      }
    }
  
  }

void x11_window_show(x11_window_t * win, int show)
  {
  if(!show)
    {
    XWithdrawWindow(win->dpy, win->current_window,
                    DefaultScreen(win->dpy));
    win->mapped = 0;
    XSync(win->dpy, False);

    if(win->disable_screensaver)
      enable_screensaver(win);
    
    return;
    }


  if(((win->current_window == win->normal_window) && win->disable_screensaver_normal) ||
     ((win->current_window == win->fullscreen_window) && win->disable_screensaver_fullscreen))
    {
    win->disable_screensaver = 1;
    disable_screensaver(win);
    }
  else
    {
    win->disable_screensaver = 0;
    }

  /* If the window was already mapped, raise it */
  
  if(win->mapped)
    {
    XRaiseWindow(win->dpy, win->current_window);
    }
  else
    {
    XMapWindow(win->dpy, win->current_window);
    if((win->current_window == win->normal_window) && !win->is_embedded)
      XMoveResizeWindow(win->dpy, win->normal_window,
                        win->window_x, win->window_y,
                        win->window_width, win->window_height);
    else
      {
      if(win->fullscreen_mode & FULLSCREEN_MODE_NET_ABOVE)
        {
        netwm_set_state(win, win->fullscreen_window,
                        _NET_WM_STATE_ADD, win->_NET_WM_STATE_ABOVE);
        }
      if(win->fullscreen_mode & FULLSCREEN_MODE_NET_FULLSCREEN)
        {
        netwm_set_state(win, win->fullscreen_window,
                        _NET_WM_STATE_ADD, win->_NET_WM_STATE_FULLSCREEN);
        }
      }

    
    }
  }

void x11_window_resize(x11_window_t * win,
                       int width, int height)
  {
  win->normal_width = width;
  win->normal_height = height;
  if(win->current_window == win->normal_window)
    {
    win->window_width = width;
    win->window_height = height;
    XResizeWindow(win->dpy, win->normal_window, width, height);
    }
  }

void x11_window_clear(x11_window_t * win)
  {
//  XSetForeground(win->dpy, win->gc, win->black);
//  XFillRectangle(win->dpy, win->normal_window, win->gc, 0, 0, win->window_width, 
//                 win->window_height);  
   XClearArea(win->dpy, win->normal_window, 0, 0,
              win->window_width, win->window_height, True);
   
   XClearArea(win->dpy, win->fullscreen_window, 0, 0,
              win->window_width, win->window_height, True);

   //   XSync(win->dpy, False);
  }

#include "x11.h"

int xerror(Display *dpy, XErrorEvent *ee) {
    if(ee->error_code == BadWindow
    ||(ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
    ||(ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
        return 0;
//    return xerrorxlib(dpy, ee); /* may call exit */
}

int xerrorstart(Display *dsply, XErrorEvent *ee, WM *winman) {
    winman->otherwm = True;
    return -1;
}

void setup(WM* winman) {
    int d;
    unsigned int i, j, mask;
    Window w;
    XModifierKeymap *modmap;
    XSetWindowAttributes wa;

    // init atoms
    winman->wmatom[WMProtocols] = XInternAtom(winman->dpy, "WM_PROTOCOLS", False);
    winman->wmatom[WMDelete] = XInternAtom(winman->dpy, "WM_DELETE_WINDOW", False);
    winman->wmatom[WMName] = XInternAtom(winman->dpy, "WM_NAME", False);
    winman->wmatom[WMState] = XInternAtom(winman->dpy, "WM_STATE", False);
    winman->netatom[NetSupported] = XInternAtom(winman->dpy, "_NET_SUPPORTED", False);
    winman->netatom[NetWMName] = XInternAtom(winman->dpy, "_NET_WM_NAME", False);
    XChangeProperty(winman->dpy, winman->root, winman->netatom[NetSupported], XA_ATOM, 32,
            PropModeReplace, (unsigned char *) winman->netatom, NetLast);

    // init geometry
    winman->sx = winman->sy = 0;
    winman->sw = DisplayWidth(winman->dpy, winman->screen);
    winman->sh = DisplayHeight(winman->dpy, winman->screen);

    // init window area
    winman->wax = winman->way = 0;
    winman->waw = winman->sw;
    winman->wah = winman->sh;

    // TODO: init modifier map
    // TODO: init cursors

    wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask
        | EnterWindowMask | LeaveWindowMask | StructureNotifyMask;
    XChangeWindowAttributes(winman->dpy, winman->root, CWEventMask | CWCursor, &wa);
    XSelectInput(winman->dpy, winman->root, wa.event_mask);
    
    // TODO: Grabkeys, Multihead-Support, Xinerama, Compiz, Multitouch :D
}

Client manage(WM* winman, Window w, XWindowAttributes *wa, Client* c) {
    XWindowChanges wc;
    Window trans;
    Status rettrans;
    long data[] = {NormalState, None};
    XEvent ev;
    XClassHint ch = { 0 };

    // Set window and WM of our Client
    c->win = w;
    c->manager = winman;

    // Set name of our Client to Class and Name delimited by "|"
    XGetClassHint(winman->dpy, c->win, &ch);
    snprintf(c->name, sizeof(char)*256, "%s|%s",
        ch.res_class ? ch.res_class : "",
        ch.res_name ? ch.res_name : "");
    if(ch.res_class) XFree(ch.res_class);
    if(ch.res_name) XFree(ch.res_name);

    // Set dimensions
    c->x = wa->x;
    c->y = wa->y;
    c->w = wa->width;
    c->h = wa->height;
    c->oldborder = wa->border_width;

    if(c->w == winman->sw && c->h == winman->sh) {
        // if we're fullscreen, make it fully visible
        c->x = winman->sx;
        c->y = winman->sy;
        c->border = wa->border_width;
    }
    else {
        // Make sure we don't drop out of proportion
        if(c->x + c->w + 2 * c->border > winman->wax + winman->waw)
            c->x = winman->wax + winman->waw - c->w - 2 * c->border;
        if(c->y + c->h + 2 * c->border > winman->way + winman->wah)
            c->y = winman->way + winman->wah - c->h - 2 * c->border;
        if(c->x < winman->wax)
            c->x = winman->wax;
        if(c->y < winman->way)
            c->y = winman->way;
        c->border = 0;
    }
    wc.border_width = c->border;

    // Configure the window with the clients values
    XConfigureWindow(winman->dpy, w, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
    XSelectInput(winman->dpy, w, EnterWindowMask | FocusChangeMask 
            | PropertyChangeMask | StructureNotifyMask);
    XMoveResizeWindow(winman->dpy, c->win, c->x, c->y, c->w, c->h); // some wins need this    
    XMapWindow(winman->dpy, c->win);
    XChangeProperty(winman->dpy, c->win, winman->wmatom[WMState], 
            winman->wmatom[WMState], 32, PropModeReplace, 
            (unsigned char*)data, 2);

    // Select and Raise the window
    winman->selected = c;
    XRaiseWindow(winman->dpy, winman->selected->win);

    // Sync
    XSync(winman->dpy, False);
}

void border_client(Client* c, int w) {
    XWindowChanges wc;

    // Save the current borderwidth and set the new one
    c->oldborder = c->border;
    c->border = w;
    wc.border_width = w;
    XConfigureWindow(c->manager->dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
    XMoveResizeWindow(c->manager->dpy, c->win, c->x, c->y, c->w, c->h); // some wins need this
    XSync(c->manager->dpy, False);
}

void unborder_client(Client* c) {
    XWindowChanges wc;

    // reset previous border width
    c->border = c->oldborder;
    wc.border_width = c->border;
    XConfigureWindow(c->manager->dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
    XMoveResizeWindow(c->manager->dpy, c->win, c->x, c->y, c->w, c->h); // some wins need this
    XSync(c->manager->dpy, False);
}

void raise_client(Client* c) {    
    XMoveResizeWindow(c->manager->dpy, c->win, c->x, c->y, c->w, c->h); // some wins need this
    XMapWindow(c->manager->dpy, c->win);

    // Select the client and raise its window
    c->manager->selected = c;
    XRaiseWindow(c->manager->dpy, c->win);
    XSync(c->manager->dpy, False);
}

void maprequest(WM* winman, XEvent *ev) {
    XWindowAttributes wa;

    winman->clients_num += 1;
    // remember: if realloc fails, it keeps the origin-block intact, 
    // so we can just do it directly :)
    winman->clients = realloc(winman->clients, sizeof(Client)*winman->clients_num);
    manage(winman, ev->xany.window, &wa, &winman->clients[winman->clients_num-1]);
}

void unmaprequest(WM* winman, XEvent *ev) {
    int i,j;

    // Find the unmapped window
    for(i=0; i<winman->clients_num; i++)
        if (winman->clients[i].win == ev->xany.window) {
            winman->clients_num -= 1;

            // Move follow up windows oone ahead, overwriting the unmapped client
            for(j=i+1; j<winman->clients_num; j++)
                winman->clients[j-1] = winman->clients[j];
            // Save memory
            winman->clients = realloc(winman->clients, sizeof(Client)*winman->clients_num); 
        }
}

void process_event(WM* winman) {
    XEvent ev;

    if (XPending(winman->dpy)) {
        XNextEvent(winman->dpy, &ev);
        switch (ev.type) {
            case (MapRequest):
                maprequest(winman, &ev);
                break;
            case (UnmapNotify || DestroyNotify):
                unmaprequest;
                break;
            default:
                break;
        }
    }
    printf("Poll again");
}

void
resize(WM* winman, Client *c, int x, int y, int w, int h, int sizehints) {
	XWindowChanges wc;

	if(sizehints != 0) {
		/* set minimum possible */
		if (w < 1)
			w = 1;
		if (h < 1)
			h = 1;

		/* temporarily remove base dimensions */
		w -= c->basew;
		h -= c->baseh;

		/* adjust for aspect limits */
		if (c->minay > 0 && c->maxay > 0 && c->minax > 0 && c->maxax > 0) {
			if (w * c->maxay > h * c->maxax)
				w = h * c->maxax / c->maxay;
			else if (w * c->minay < h * c->minax)
				h = w * c->minay / c->minax;
		}

		/* adjust for increment value */
		if(c->incw)
			w -= w % c->incw;
		if(c->inch)
			h -= h % c->inch;

		/* restore base dimensions */
		w += c->basew;
		h += c->baseh;

		if(c->minw > 0 && w < c->minw)
			w = c->minw;
		if(c->minh > 0 && h < c->minh)
			h = c->minh;
		if(c->maxw > 0 && w > c->maxw)
			w = c->maxw;
		if(c->maxh > 0 && h > c->maxh)
			h = c->maxh;
	}
	if(w <= 0 || h <= 0)
		return;
	/* offscreen appearance fixes */
	if(x > winman->sw)
		x = winman->sw - w - 2 * c->border;
	if(y > winman->sh)
		y = winman->sh - h - 2 * c->border;
	if(x + w + 2 * c->border < winman->sx)
		x = winman->sx;
	if(y + h + 2 * c->border < winman->sy)
		y = winman->sy;
	if(c->x != x || c->y != y || c->w != w || c->h != h) {
		c->x = wc.x = x;
		c->y = wc.y = y;
		c->w = wc.width = w;
		c->h = wc.height = h;
		wc.border_width = c->border;
		XConfigureWindow(winman->dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
		XSync(winman->dpy, False);
	}
}

void ban_client(Client* c) {
    if (c->isbanned)
        return;
    XMoveWindow(c->manager->dpy, c->win, c->manager->sw + 2, c->manager->sh + 2);
    c->isbanned = True;
    XSync(c->manager->dpy, False);
}

void unban_client(Client*c) {
    if (!(c->isbanned))
        return;
    XMoveWindow(c->manager->dpy, c->win, c->x, c->y);
    c->isbanned = False;
    XSync(c->manager->dpy, False);
}

Client* query_clients(WM* winman) {
    unsigned int i, num;
    Window *wins, d1, d2;
    XWindowAttributes wa;
    Client* c;

    wins = NULL;
    if (XQueryTree(winman->dpy, winman->root, &d1, &d2, &wins, &num)) {
        winman->clients_num = num;
        c = (Client*)calloc(num, sizeof(Client));
        for (i = 0; i < num; i++) {
            XGetWindowAttributes(winman->dpy, wins[i], &wa);
            if(wa.map_state == IsViewable)
                manage(winman, wins[i], &wa, &c[i]);
        }
    }
    if(wins)
        XFree(wins);
    return c;
}

WM* Init_WM(const char* locale) {
    WM* windowmanager;

    // Allocate Structure
    windowmanager = calloc(1,sizeof(WM));

    // Set language
    setlocale(LC_CTYPE, locale);

    // Get Display and Rootscreen
    if(!(windowmanager->dpy = XOpenDisplay(0)))
        printf("Cannot open Display!!\n");fflush(stdout);        
    windowmanager->screen = DefaultScreen(windowmanager->dpy);
    windowmanager->root = RootWindow(windowmanager->dpy, windowmanager->screen);
    
    // Try for other WM
    Bool otherwm = False;
    XSetErrorHandler(xerrorstart);
    // This causes an error if some other window manager is running
    XSelectInput(windowmanager->dpy, windowmanager->root, SubstructureRedirectMask);
    XSync(windowmanager->dpy, False);
    if(otherwm)
        printf("Another WM is running!!\n");fflush(stdout);

    // Reset Errorhandling and Sync display
    XSync(windowmanager->dpy, False);
    XSetErrorHandler(NULL);
    windowmanager->xerrorxlib = XSetErrorHandler(xerror);
    XSync(windowmanager->dpy, False);

    // Setup
    setup(windowmanager);

    // Start polling
    // pthread_create(&windowmanager->polling, NULL, 
    //        (void*(*)(void*))process_event, windowmanager);

    return windowmanager;
}

void Destroy_WM(WM* winman) {
    XUngrabKey(winman->dpy, AnyKey, AnyModifier, winman->root);
    XSetInputFocus(winman->dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    XSync(winman->dpy, False);
    XCloseDisplay(winman->dpy);
    free(winman->clients);
    free(winman);
}

int main() {
    WM* winman;   
    int i;

    printf("Start to init NOW!\n");fflush(stdout);
    winman = Init_WM("de_DE.UTF-8");
    printf("We have a screen: %d  %d %d %d\n", winman->sx, winman->sy, winman->sw, winman->sh);
    printf("         ... and a window area: %d %d %d %d\n", winman->wax, winman->way, winman->waw, winman->wah);
    printf("Start query NOW!\n");fflush(stdout);
    winman->clients = query_clients(winman);
    printf("Success! We should have clients now!\n");
    printf("         And they should all have the WM ptr... (trying just the first): %d %d %d %d\n", 
            winman->clients[0].manager->sx, winman->clients[0].manager->sy, 
            winman->clients[0].manager->sw, winman->clients[0].manager->sh);
    for(i=0; i < winman->clients_num; i++) {
        printf("                  Trying client number %d name: %s \n", i, winman->clients[i].name);
        printf("                           Geo: %d %d %d %d\n", 
                i, winman->clients[0].x, winman->clients[0].y, winman->clients[i].w, winman->clients[0].h);
    }
    printf("Great so far! Try some resizing now...\n");fflush(stdout);
    resize(winman, &winman->clients[0], winman->wax, winman->way, winman->waw, winman->wah, 0);
    printf("Good. Set a border around our main client now...\n");
    border_client(&winman->clients[0], 10);

    printf("Try to raise in cycles as well...\n");
    for(i=0; i < winman->clients_num; i++) {
        raise_client(&winman->clients[i]);
        printf("         Client %d should be raised now...\n", i);fflush(stdout);
        usleep(1000000);
    }

    printf("Ban - unban each client ...\n");
    for(i=0; i < winman->clients_num; i++) {
        ban_client(&winman->clients[i]);
        printf("         Client %d is banned? %d \n", i, winman->clients[i].isbanned);fflush(stdout);
        usleep(1000000);
        unban_client(&winman->clients[i]);
        printf("            ... and unbanned? %d \n", winman->clients[i].isbanned);fflush(stdout);
    }

    printf("Finish for now...\n");
    return 0;
}

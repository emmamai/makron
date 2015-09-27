#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <xcb/xcb.h>

#define PROGRAM_NAME "makron"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_STRING "0.1"
#define VERSION_BUILDSTR "20"

#define MAX_CLIENTS 1024

#define BORDER_SIZE_LEFT 1
#define BORDER_SIZE_RIGHT 1
#define BORDER_SIZE_TOP 19
#define BORDER_SIZE_BOTTOM 1

typedef enum {
	STATE_WITHDRAWN = 0,
	STATE_ICON = 1,
	STATE_NORMAL = 3,
} clientWindowState_t;

typedef enum {
	STATE_NO_REDIRECT, //override redirect
	STATE_INIT,
	STATE_REPARENTED,
	STATE_TRANSIENT
} clientManagementState_t;

typedef enum {
	WMSTATE_IDLE,
	WMSTATE_DRAG
} wmState_t;

typedef struct client_s {
	xcb_window_t window;
	xcb_window_t parent;

	short width;
	short height;
	short x;
	short y;

	clientWindowState_t windowState;
	clientManagementState_t managementState;

	//todo: gravity

	struct client_s *nextClient;
} client_t;

client_t *firstClient = NULL;
xcb_connection_t *c;
xcb_screen_t *screen;
xcb_generic_event_t *e;
xcb_colormap_t colormap;
unsigned int whitePixel;
unsigned int lightGreyPixel;
unsigned int darkGreyPixel;
unsigned int blackPixel;
unsigned int darkAccentPixel;
unsigned int accentPixel;
unsigned int lightAccentPixel;

unsigned int whiteContext;
unsigned int lightGreyContext;
unsigned int darkGreyContext;
unsigned int blackContext;
unsigned int darkAccentContext;
unsigned int accentContext;
unsigned int lightAccentContext;



wmState_t wmState = WMSTATE_IDLE;
client_t *dragClient;
short dragStartX;
short dragStartY;

xcb_window_t activeWindow = NULL;

void ConfigureClient( client_t *n, short x, short y, unsigned short width, unsigned short height ) {
	unsigned short pmask = 	XCB_CONFIG_WINDOW_X |
							XCB_CONFIG_WINDOW_Y |
							XCB_CONFIG_WINDOW_WIDTH |
							XCB_CONFIG_WINDOW_HEIGHT |
							XCB_CONFIG_WINDOW_BORDER_WIDTH;

	unsigned short cmask = 	XCB_CONFIG_WINDOW_WIDTH |
							XCB_CONFIG_WINDOW_HEIGHT |
							XCB_CONFIG_WINDOW_BORDER_WIDTH;
	int i;

	n->x = x;
	n->y = y;
	n->width = width;
	n->height = height;

	i = ( n->width + n->x );
	if ( i > screen->width_in_pixels ) {
		n->x -= i - screen->width_in_pixels;
	}
	i = ( n->height + n->y );
	if ( i > screen->height_in_pixels ) {
		n->y -= i - screen->height_in_pixels;
	}
	if ( n->x < 0 ) {
		n->x = 0;
	}
	if ( n->y < 0 ) {
		n->y = 0;
	}
	unsigned int pv[5] = {
		n->x, 
		n->y, 
		n->width + BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT, 
		n->height + BORDER_SIZE_TOP + BORDER_SIZE_BOTTOM, 
		0
	};
	unsigned int cv[3] = {
		n->width, 
		n->height, 
		0
	};
	

	xcb_configure_window( c, n->parent, pmask, pv );
	xcb_configure_window( c, n->window, cmask, cv );
	xcb_flush( c );
}

void DrawFrame( client_t *n ) {
	xcb_rectangle_t borderRect[1];
	xcb_segment_t topBorderSegment[1];

	borderRect[0].x = 0;
	borderRect[0].y = 0;
	borderRect[0].width = n->width + BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT - 1;  
	borderRect[0].height = n->height + BORDER_SIZE_TOP + BORDER_SIZE_BOTTOM - 1;

	topBorderSegment[0].x1 = 1;
	topBorderSegment[0].y1 = BORDER_SIZE_TOP - 1;
	topBorderSegment[0].x2 = n->width + BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT - 2;  
	topBorderSegment[0].y2 = BORDER_SIZE_TOP - 1;

	if ( n->parent == activeWindow ) {
		printf("Repainting active window\n");
		xcb_poly_fill_rectangle( c, n->parent, lightGreyContext, 1, borderRect );
		xcb_poly_rectangle( c, n->parent, blackContext, 1, borderRect );
		xcb_poly_segment( c, n->parent, blackContext, 1, topBorderSegment );
	} else {
		printf("Repainting non-active window\n");
		xcb_poly_fill_rectangle( c, n->parent, whiteContext, 1, borderRect );
		xcb_poly_rectangle( c, n->parent, darkGreyContext, 1, borderRect );
		xcb_poly_segment( c, n->parent, darkGreyContext, 1, topBorderSegment );
	}
	
	xcb_flush( c );
	return;
}

client_t *GetClientByWindow( xcb_window_t w ) {
	client_t *n = firstClient;

	for ( ; n != NULL; n = n->nextClient ) {
		if ( n->window == w ) {
			return n;
		}
	}
	return NULL;
}

client_t *GetClientByParent( xcb_window_t w ) {
	client_t *n = firstClient;

	for ( ; n != NULL; n = n->nextClient ) {
		if ( n->parent == w ) {
			return n;
		}
	}
	return NULL;
}

void RaiseClient( client_t *n ) {
	unsigned short mask = XCB_CONFIG_WINDOW_STACK_MODE;
	unsigned int v[1] = { XCB_STACK_MODE_ABOVE };
	client_t *m = GetClientByParent( activeWindow );

	xcb_set_input_focus( c, XCB_INPUT_FOCUS_NONE, n->window, 0 );
	activeWindow = n->parent;
	DrawFrame( n );
	if( m != NULL ) {
		DrawFrame( m );
	}
	if ( n->managementState == STATE_NO_REDIRECT ) { //this might not be necessary
		xcb_configure_window( c, n->window, mask, v );

	} else {
		xcb_configure_window( c, n->parent, mask, v );
	}

	xcb_flush( c );
}

void DoCreateNotify( xcb_create_notify_event_t *e ) {
	unsigned int v[2] = { 	whitePixel, 
							XCB_EVENT_MASK_EXPOSURE | 
							XCB_EVENT_MASK_BUTTON_PRESS | 
							XCB_EVENT_MASK_BUTTON_RELEASE | 
							XCB_EVENT_MASK_POINTER_MOTION | 
							XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | 
							XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT };
	client_t *m = firstClient;
	client_t *n = firstClient;

	if ( e->parent != screen->root ) {
		return;
	}

	if ( n == NULL ) {
		firstClient = n = malloc( sizeof( client_t ) );
		n->nextClient = NULL;
	} else {
		while ( n != NULL ) {
			if( e->window == n->window || e->window == n->parent ) {
					printf( "New frame\n" );
				return;
			}
			m = n;
			n = m->nextClient;
		}
		m->nextClient = n = malloc( sizeof( client_t ) );
		n->nextClient = NULL;
	}

	n->window = e->window;
	n->width = e->width;
	n->height = e->height;
	n->x = e->x;
	n->y = e->y;

	n->managementState = STATE_WITHDRAWN;

	if ( e->override_redirect ) {
		n->managementState = STATE_NO_REDIRECT;
		printf( "New unreparented window\n" );
		return;
	}
	n->parent = xcb_generate_id( c );
	xcb_create_window (		c, XCB_COPY_FROM_PARENT, n->parent, screen->root, 
							0, 0, BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT + 1, BORDER_SIZE_TOP + BORDER_SIZE_BOTTOM + 1, 
							0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, 
							XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, v);

	xcb_reparent_window( c, n->window, n->parent, 1, 19 );
	xcb_flush( c );
}

void DoReparentNotify( xcb_reparent_notify_event_t *e ) {
	client_t *n = firstClient;

	for ( ; n != NULL; n = n->nextClient ) {
		if ( n->window == e->window ) {
			n->managementState = STATE_REPARENTED;
			ConfigureClient( n, n->x, n->y, n->width, n->height );
			printf( "window %x reparented to window %x\n", e->window, e->parent );
			return;
		}
	}
	printf("Reparented window doesn't exist!\n");
}

void DoMapRequest( xcb_map_request_event_t *e ) {
	client_t *n = firstClient;

	/* Todo:
		Add ICCCM section 4.1.4 compatibility
		https://tronche.com/gui/x/icccm/sec-4.html#s-4.1.3
	*/

	for ( ; n != NULL; n = n->nextClient ) {
		if ( n->window == e->window ) {
			n->windowState = STATE_NORMAL;
			xcb_map_window( c, n->parent );
			xcb_map_window( c, n->window );
			xcb_flush( c );
			RaiseClient( n );
			printf( "window %x mapped\n", e->window );
			return;
		}
	}
	printf("Attempt to map nonexistent window!\n");
}

void DoConfigureRequest( xcb_configure_request_event_t *e ) {
	client_t *n = firstClient;

	for ( ; n != NULL; n = n->nextClient ) {
		if ( n->window == e->window ) {
			ConfigureClient( n, e->x, e->y, e->width, e->height );
			return;
		}
	}
}

void DoExpose( xcb_expose_event_t *e ) {
	client_t *n = firstClient;

	printf( "expose window %x, activeWindow is %x\n", e->window, activeWindow );

	for ( ; n != NULL; n = n->nextClient ) {
		if ( n->parent == e->window ) {
			DrawFrame( n );
		}
	}
}

void DoDestroy( xcb_destroy_notify_event_t *e ) {
	client_t *n = firstClient;
	client_t *m = firstClient;

	printf( "window %x destroyed\n", e->window );

	for ( ; n != NULL; m = n, n = n->nextClient ) {
		if ( n->window == e->window ) {
			m->nextClient = n->nextClient;
			if ( activeWindow == n->parent ) {
				activeWindow = 0;
			}
			xcb_destroy_window( c, n->parent );
			if( firstClient == n ) {
				firstClient = n->nextClient;
			}
			free( n );
			n = NULL;
			xcb_flush( c );
			return;
		}
	}
}

void DoButtonPress( xcb_button_press_event_t *e ) {
	client_t *n = firstClient;
	
	for ( ; n != NULL; n = n->nextClient ) {
		if ( n->parent == e->event ) {
			printf( "Window drag at (%d,%d)local, (%d,%d)root in window %x\n", e->event_x, e->event_y, e->root_x, e->root_y, e->event );
			RaiseClient( n );
			dragClient = n;
			wmState = WMSTATE_DRAG;
			dragStartX = e->event_x;
			dragStartY = e->event_y;
			return;
		}
	}
}

void DoButtonRelease( xcb_button_release_event_t *e ) {
	wmState = WMSTATE_IDLE;
	dragClient = NULL;
	dragStartX = 0;
	dragStartY = 0;
	printf( "Window drag done\n" );
}

void DoMotionNotify( xcb_motion_notify_event_t *e ) {
	int x;
	int y;
	if ( wmState == WMSTATE_DRAG ) {
		x = e->root_x - dragStartX;
		y =  e->root_y - dragStartY;
		ConfigureClient( dragClient, x, y, dragClient->width, dragClient->height );
	}
}

int BecomeWM(  ) {
	unsigned int v[1];
	xcb_void_cookie_t cookie;
	xcb_generic_error_t *error;

	v[0] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
	cookie = xcb_change_window_attributes_checked( c, screen->root, XCB_CW_EVENT_MASK, v );
	error = xcb_request_check( c, cookie );
	if ( error ) {
		free( error );
 		return -1;
	}
	return 0;
}

void Cleanup() {
	client_t *m = firstClient;
	client_t *n = firstClient;

	while ( n != NULL ) {
		m = n;
		n = m->nextClient;
		free( m );
	}

	xcb_disconnect( c );
}

void Quit( int r ) {
	Cleanup();
	exit( r );
}

void SetupColors() {
	xcb_alloc_color_reply_t *reply;
	unsigned int v[1];

	whitePixel = screen->white_pixel;
	blackPixel = screen->black_pixel;

	reply = xcb_alloc_color_reply ( c, xcb_alloc_color ( c, colormap, 62208, 62208, 62208 ), NULL );
	lightGreyPixel = reply->pixel;
	free( reply );

	reply = xcb_alloc_color_reply ( c, xcb_alloc_color ( c, colormap, 49152, 49152, 49152 ), NULL );
	darkGreyPixel = reply->pixel;
	free( reply );

	reply = xcb_alloc_color_reply ( c, xcb_alloc_color ( c, colormap, 45824, 45824, 55808 ), NULL );
	accentPixel = reply->pixel;
	free( reply );

	reply = xcb_alloc_color_reply ( c, xcb_alloc_color ( c, colormap, 55808, 55808, 65535 ), NULL );
	lightAccentPixel = reply->pixel;
	free( reply );

	reply = xcb_alloc_color_reply ( c, xcb_alloc_color ( c, colormap, 21504, 21504, 34560 ), NULL );
	darkAccentPixel = reply->pixel;
	free( reply );

	blackContext = xcb_generate_id( c );
	v[0] = blackPixel;
	xcb_create_gc( c, blackContext, screen->root, XCB_GC_FOREGROUND, v );

	darkGreyContext = xcb_generate_id( c );
	v[0] = darkGreyPixel;
	xcb_create_gc( c, darkGreyContext, screen->root, XCB_GC_FOREGROUND, v );

	lightGreyContext = xcb_generate_id( c );
	v[0] = lightGreyPixel;
	xcb_create_gc( c, lightGreyContext, screen->root, XCB_GC_FOREGROUND, v );

	whiteContext = xcb_generate_id( c );
	v[0] = whitePixel;
	xcb_create_gc( c, whiteContext, screen->root, XCB_GC_FOREGROUND, v );

	darkAccentContext = xcb_generate_id( c );
	v[0] = darkAccentPixel;
	xcb_create_gc( c, darkAccentContext, screen->root, XCB_GC_FOREGROUND, v );

	accentContext = xcb_generate_id( c );
	v[0] = accentPixel;
	xcb_create_gc( c, accentContext, screen->root, XCB_GC_FOREGROUND, v );

	lightAccentContext = xcb_generate_id( c );
	v[0] = lightAccentPixel;
	xcb_create_gc( c, lightAccentContext, screen->root, XCB_GC_FOREGROUND, v );
}

void SetRootBackground() {
	xcb_pixmap_t pixmap = xcb_generate_id( c );
	unsigned int v[1] = { pixmap };
	xcb_point_t lightPoints[2] = {{0,0},{1,1}};
	xcb_point_t darkPoints[2] = {{0,1},{1,0}};

	xcb_create_pixmap( c, screen->root_depth, pixmap, screen->root, 2, 2 );
	xcb_poly_point( c, XCB_COORD_MODE_ORIGIN, pixmap, lightGreyContext, 2, lightPoints );
	xcb_poly_point( c, XCB_COORD_MODE_ORIGIN, pixmap, darkGreyContext, 2, darkPoints );
	xcb_change_window_attributes( c, screen->root, XCB_CW_BACK_PIXMAP, v );
	xcb_clear_area( c, 1, screen->root, 0, 0, screen->width_in_pixels - 1, screen->height_in_pixels - 1 );

	xcb_flush( c );
}

int main() {
	signal( SIGTERM, Quit );
	signal( SIGINT, Quit );

	printf( "%s %s, build %s\n\n", PROGRAM_NAME, VERSION_STRING, VERSION_BUILDSTR );

	c = xcb_connect( NULL, NULL );
	if ( xcb_connection_has_error( c ) ) {
			printf( "Uh oh! It looks like X isn't running.\n" );
			printf( "You'll need to start it before you can run makron.\n" );
			Cleanup();
			return 1;
	}
	screen = xcb_setup_roots_iterator( xcb_get_setup( c ) ).data;
	if ( BecomeWM() < 0 ) {
		printf( "Uh oh! It looks like there's another window manager running.\n" );
		printf( "You'll need to close it before you can run makron.\n" );
		Cleanup();
		return 1;
	}

	colormap = screen->default_colormap;
	SetupColors();
	SetRootBackground();

	while( ( e = xcb_wait_for_event( c ) ) != NULL ) {
		
		switch( e->response_type & ~0x80 ) {
			case XCB_CREATE_NOTIFY: 
				DoCreateNotify( (xcb_create_notify_event_t *)e );
				break;
			case XCB_REPARENT_NOTIFY:
				DoReparentNotify( (xcb_reparent_notify_event_t *)e );
				break;
			case XCB_MAP_REQUEST:
				DoMapRequest( (xcb_map_request_event_t *)e );
				break;
			case XCB_CONFIGURE_REQUEST:
				DoConfigureRequest( (xcb_configure_request_event_t *)e );
				break;
			case XCB_EXPOSE:
				DoExpose( (xcb_expose_event_t *)e );
				break;
			case XCB_DESTROY_NOTIFY:
				DoDestroy( (xcb_destroy_notify_event_t *)e );
				break;
			case XCB_BUTTON_PRESS:
				DoButtonPress( (xcb_button_press_event_t *)e );
				break;
			case XCB_BUTTON_RELEASE:
				DoButtonRelease( (xcb_button_release_event_t *)e );
				break;
			case XCB_MOTION_NOTIFY:
				DoMotionNotify( (xcb_motion_notify_event_t *)e );
				break;
			default:
				printf( "Warning, unhandled event #%d\n", e->response_type & ~0x80 );
				break;
		}
		free( e );
	}
	printf( "Looks like we're done here. See you next time!\n" );
	Cleanup();
	return 0;
}
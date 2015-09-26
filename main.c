#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <xcb/xcb.h>

#define PROGRAM_NAME "makron"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_STRING "0.1"
#define VERSION_BUILDSTR "13"

#define MAX_CLIENTS 1024

#define BORDER_SIZE_LEFT 1
#define BORDER_SIZE_RIGHT 1
#define BORDER_SIZE_TOP 19
#define BORDER_SIZE_BOTTOM 1


xcb_connection_t *c;
xcb_screen_t *screen;
xcb_generic_event_t *e;
xcb_colormap_t colormap;
unsigned int greyPixel;

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

client_t *firstClient;

void ConfigureClient( client_t *n, short x, short y, unsigned short width, unsigned short height ) {
	unsigned short pmask = 	XCB_CONFIG_WINDOW_X |
							XCB_CONFIG_WINDOW_Y |
							XCB_CONFIG_WINDOW_WIDTH |
							XCB_CONFIG_WINDOW_HEIGHT |
							XCB_CONFIG_WINDOW_BORDER_WIDTH;

	unsigned short cmask = 	XCB_CONFIG_WINDOW_WIDTH |
							XCB_CONFIG_WINDOW_HEIGHT |
							XCB_CONFIG_WINDOW_BORDER_WIDTH;

	n->x = x;
	n->y = y;
	n->width = width;
	n->height = height;
	int i = 0;

	if ( ( i = screen->width_in_pixels - n->width - x ) > screen->width_in_pixels ) {
		n->x = n->x - i;
	}
	if ( ( i = screen->height_in_pixels - n->height - y ) > screen->height_in_pixels ) {
		n->y = n->y - i;
	}
	if ( x < 0 ) {
		n->x = 0;
	}
	if ( y < 0 ) {
		n->y = 0;
	}
	unsigned int pv[5] = {
		x, 
		y, 
		width + BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT, 
		height + BORDER_SIZE_TOP + BORDER_SIZE_BOTTOM, 
		0
	};
	unsigned int cv[3] = {
		width, 
		height, 
		0
	};
	

	xcb_configure_window( c, n->parent, pmask, pv );
	xcb_configure_window( c, n->window, cmask, cv );
	xcb_flush( c );
}

void DoCreateNotify( xcb_create_notify_event_t *e ) {
	unsigned int v[2] = { greyPixel, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT };
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
	printf( "window %x exposed\n", e->window );
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

	reply = xcb_alloc_color_reply ( c, xcb_alloc_color ( c, colormap, 65535, 0, 0 ), NULL );
	greyPixel = reply->pixel;
	free( reply );
}

int main() {
	signal( SIGTERM, Quit );
	signal( SIGINT, Quit );

	printf( "%s %s, build %s\n", PROGRAM_NAME, VERSION_STRING, VERSION_BUILDSTR );

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
			case XCB_EXPOSE:
				DoExpose( (xcb_expose_event_t *)e );
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
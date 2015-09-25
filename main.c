#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <xcb/xcb.h>

#define MAX_CLIENTS 1024

xcb_connection_t *c;
xcb_screen_t *screen;
xcb_generic_event_t *e;

typedef enum {
	STATE_WITHDRAWN,
	STATE_ICON,
	STATE_NORMAL,
	STATE_NULL
} clientWindowState_t;

typedef enum {
	STATE_UNMANAGED, //override redirect
	STATE_REPARENTING,
	STATE_MANAGED,
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

void HandleCreate( xcb_create_notify_event_t *e ) {
	unsigned int v[1] = { screen->white_pixel };
	client_t *m = firstClient;
	client_t *n = firstClient;
	int i = 0;
	int x, y;

	if ( e->parent != screen->root ) {
		return;
	}


	printf( "New child of root window\n" );

	if ( n == NULL ) {
		firstClient = n = malloc( sizeof( client_t ) );
		n->nextClient = NULL;
	} else {
		while ( n != NULL ) {
			m = n;
			n = m->nextClient;
		}
		m->nextClient = n = malloc( sizeof( client_t ) );
		n->nextClient = NULL;
	}
	
	n->window = e->window;

	if ( e->override_redirect ) {
		n->managementState = STATE_UNMANAGED;
		return;
	}

	n->managementState = STATE_WITHDRAWN;
	n->width = e->width + 2;
	n->height = e->height + 20;
	x = n->x = e->x - 1;
	y = n->y = e->y - 20;

	if ( ( i = screen->width_in_pixels - n->width - x ) > screen->width_in_pixels ) {
		x = x - i;
	}
	if ( ( i = screen->height_in_pixels - n->height - y ) > screen->height_in_pixels ) {
		y = y - i;
	}
	if ( x < 0 ) {
		x = 0;
	}
	if ( y < 0 ) {
		y = 0;
	}
	n->parent = xcb_generate_id(c);
	xcb_create_window (		c, XCB_COPY_FROM_PARENT, n->parent, screen->root, 
							x, y, n->width, n->height, 
							0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, 
							XCB_CW_BACK_PIXEL, v);
	xcb_reparent_window( c, n->window, n->parent, 1, 19 );
}

void HandleReparent( xcb_reparent_notify_event_t *e ) {
	client_t *n = firstClient;

	for ( ; n->window != e->window; n = n->nextClient )
		;
	n->managementState = STATE_MANAGED;
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
	printf( "PANTS\n" );
	Cleanup();
	exit( r );
}

int main() {
	signal( SIGTERM, Quit );
	signal( SIGINT, Quit );

	c = xcb_connect( NULL, NULL );
	if ( xcb_connection_has_error( c ) ) {
			printf( "Uh oh! It looks like X isn't running.\n" );
			printf( "You'll need to start it before you can run toolwm.\n" );
			Cleanup();
			return 1;
	}
	screen = xcb_setup_roots_iterator( xcb_get_setup( c ) ).data;
	if ( BecomeWM() < 0 ) {
		printf( "Uh oh! It looks like there's another window manager running.\n" );
		printf( "You'll need to close it before you can run toolwm.\n" );
		Cleanup();
		return 1;
	}
	while( ( e = xcb_wait_for_event( c ) ) != NULL ) {
		
		switch( e->response_type & ~0x80 ) {
			case XCB_CREATE_NOTIFY: 
				HandleCreate( (xcb_create_notify_event_t *)e );
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
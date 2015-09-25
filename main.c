#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>

int BecomeWM( xcb_connection_t *c, xcb_screen_t *screen ) {
	unsigned int v[1];
	xcb_void_cookie_t cookie;
	xcb_generic_error_t *error;

	v[0] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
	cookie = xcb_change_window_attributes_checked( c, screen->root, XCB_CW_EVENT_MASK, v );
	error = xcb_request_check( c, cookie );
	if ( error ) {
 		return -1;
	}
	return 0;
}

int main() {
	xcb_connection_t *c;
	xcb_screen_t *screen;
	xcb_generic_event_t *e;

	c = xcb_connect( NULL, NULL );
	screen = xcb_setup_roots_iterator( xcb_get_setup( c ) ).data;

	if ( BecomeWM( c, screen ) < 0 ) {
		printf( "Uh oh! It looks like there's another window manager running.\n" );
		printf( "You'll need to close it before you can run toolwm.\n" );
		return 1;
	}
	
	while( ( e = xcb_wait_for_event( c ) ) != NULL ) {

	}
	
	printf( "Looks like we're done here. See you next time!\n" );

	return 0;
}

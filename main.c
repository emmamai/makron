#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <xcb/xcb.h>

#define PROGRAM_NAME "makron"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_STRING "0.1"
#define VERSION_BUILDSTR "26"

#define MAX_CLIENTS 1024

#define BORDER_SIZE_LEFT 1
#define BORDER_SIZE_RIGHT 1
#define BORDER_SIZE_TOP 19
#define BORDER_SIZE_BOTTOM 1

#define FONT_NAME "fixed"

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
	WMSTATE_DRAG,
	WMSTATE_CLOSE
} wmState_t;

typedef struct client_s {
	xcb_window_t window;
	xcb_window_t parent;

	char parentMapped;

	short width;
	short height;
	short x;
	short y;

	clientWindowState_t windowState;
	clientManagementState_t managementState;

	char name[256];

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
unsigned int greyPixel;
unsigned int darkGreyPixel;
unsigned int blackPixel;
unsigned int darkAccentPixel;
unsigned int accentPixel;
unsigned int lightAccentPixel;

unsigned int whiteContext;
unsigned int lightGreyContext;
unsigned int greyContext;
unsigned int darkGreyContext;
unsigned int blackContext;
unsigned int darkAccentContext;
unsigned int accentContext;
unsigned int lightAccentContext;

unsigned int inactiveFontContext;
unsigned int activeFontContext;

xcb_font_t windowFont;

wmState_t wmState = WMSTATE_IDLE;
client_t *dragClient;
short dragStartX;
short dragStartY;
short mouseLastKnownX;
short mouseLastKnownY;
short mouseIsOverCloseButton;

xcb_window_t activeWindow = NULL;

/*
=================
Support functions
=================
*/

void ConfigureClient( client_t *n, short x, short y, unsigned short width, unsigned short height ) {
	int nx, ny;
	unsigned short pmask = 	XCB_CONFIG_WINDOW_X |
							XCB_CONFIG_WINDOW_Y |
							XCB_CONFIG_WINDOW_WIDTH |
							XCB_CONFIG_WINDOW_HEIGHT |
							XCB_CONFIG_WINDOW_BORDER_WIDTH;

	unsigned short cmask = 	XCB_CONFIG_WINDOW_WIDTH |
							XCB_CONFIG_WINDOW_HEIGHT |
							XCB_CONFIG_WINDOW_BORDER_WIDTH;
	int i;

	if ( n == NULL ) {
		return;
	}

	nx = x;
	ny = y;

	i = ( width + nx );
	if ( i > screen->width_in_pixels ) {
		nx -= i - screen->width_in_pixels;
	}
	i = ( height + ny );
	if ( i > screen->height_in_pixels ) {
		ny -= i - screen->height_in_pixels;
	}
	if ( nx < 0 ) {
		nx = 0;
	}
	if ( ny < 0 ) {
		ny = 0;
	}
	unsigned int pv[5] = {
		nx, 
		ny, 
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

void DrawFrame( client_t *n ) {
	xcb_rectangle_t closeRectDark[1];
	xcb_rectangle_t closeRectLight[1];
	xcb_rectangle_t closeRectGrey[1];
	xcb_rectangle_t borderRect[1];
	xcb_segment_t topBorderSegment[1];
	xcb_segment_t titleAccent[2];
	xcb_segment_t titleAccentShadow[2];
	int textLen = 0;
	int textWidth = 0;
	int textPos = 0;
	xcb_query_text_extents_reply_t *r;
	xcb_char2b_t *s;

	if( n == NULL ) {
		return;
	}

	borderRect[0].x = 0;
	borderRect[0].y = 0;
	borderRect[0].width = n->width + BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT - 1;  
	borderRect[0].height = n->height + BORDER_SIZE_TOP + BORDER_SIZE_BOTTOM - 1;

	topBorderSegment[0].x1 = 1;
	topBorderSegment[0].y1 = BORDER_SIZE_TOP - 1;
	topBorderSegment[0].x2 = n->width + BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT - 2;  
	topBorderSegment[0].y2 = BORDER_SIZE_TOP - 1;

	titleAccent[0].x1 = 1;
	titleAccent[0].y1 = 1;
	titleAccent[0].x2 = n->width + BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT - 2;  
	titleAccent[0].y2 = 1;
	titleAccent[1].x1 = 1;
	titleAccent[1].y1 = 1;
	titleAccent[1].x2 = 1;  
	titleAccent[1].y2 = BORDER_SIZE_TOP - 2;

	titleAccentShadow[0].x1 = 1;
	titleAccentShadow[0].y1 = BORDER_SIZE_TOP - 2;
	titleAccentShadow[0].x2 = n->width + BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT - 2;  
	titleAccentShadow[0].y2 = BORDER_SIZE_TOP - 2;
	titleAccentShadow[1].x1 = n->width + BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT - 2;
	titleAccentShadow[1].y1 = 1;
	titleAccentShadow[1].x2 = n->width + BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT - 2;  
	titleAccentShadow[1].y2 = BORDER_SIZE_TOP - 2;

	closeRectDark[0].x = 9;
	closeRectDark[0].y = 4;
	closeRectDark[0].width = 11;  
	closeRectDark[0].height = 11;

	closeRectLight[0].x = 10;
	closeRectLight[0].y = 5;
	closeRectLight[0].width = 9;  
	closeRectLight[0].height = 9;

	closeRectGrey[0].x = 11;
	closeRectGrey[0].y = 6;
	closeRectGrey[0].width = 7;  
	closeRectGrey[0].height = 7;

	textLen = strnlen( n->name, 256 );
	s = malloc( textLen * sizeof( xcb_char2b_t ) );
	for( int i = 0; i < textLen; i++ ) {
		s[i].byte1 = 0;
		s[i].byte2 = n->name[i];
	}
	r = xcb_query_text_extents_reply( c, xcb_query_text_extents( c, windowFont, textLen, s ), NULL );
	textWidth = r->overall_width;
	free( r ); 
	free( s );
	textPos = ( ( n->width + BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT ) / 2 ) - ( textWidth / 2 );

	if ( n->parent == activeWindow ) {

		xcb_poly_fill_rectangle( c, n->parent, lightGreyContext, 1, borderRect );
		xcb_poly_rectangle( c, n->parent, blackContext, 1, borderRect );
		xcb_poly_segment( c, n->parent, blackContext, 1, topBorderSegment );

		xcb_poly_segment( c, n->parent, lightAccentContext, 2, titleAccent );
		xcb_poly_segment( c, n->parent, accentContext, 2, titleAccentShadow );

		xcb_poly_fill_rectangle( c, n->parent, darkAccentContext, 1, closeRectDark );
		if ( wmState == WMSTATE_CLOSE && mouseIsOverCloseButton ) {

		} else {
			xcb_poly_rectangle( c, n->parent, lightAccentContext, 1, closeRectLight );
			xcb_poly_fill_rectangle( c, n->parent, greyContext, 1, closeRectGrey );
		}
		

		xcb_image_text_8( c, textLen, n->parent, activeFontContext, textPos, 14, n->name );
	} else {
		xcb_poly_fill_rectangle( c, n->parent, whiteContext, 1, borderRect );
		xcb_poly_rectangle( c, n->parent, darkGreyContext, 1, borderRect );
		xcb_poly_segment( c, n->parent, darkGreyContext, 1, topBorderSegment );
		xcb_image_text_8( c, textLen, n->parent, inactiveFontContext, textPos, 14, n->name );
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
	DrawFrame( m ); //Yes, m could be null. DrawFrame() handles this case.
	if ( n->managementState == STATE_NO_REDIRECT ) { //this might not be necessary
		xcb_configure_window( c, n->window, mask, v );
	} else {
		xcb_configure_window( c, n->parent, mask, v );
	}

	xcb_flush( c );
}

void SetupColors() {
	xcb_alloc_color_reply_t *reply;
	unsigned int v[1];

	whitePixel = screen->white_pixel;
	blackPixel = screen->black_pixel;

	reply = xcb_alloc_color_reply ( c, xcb_alloc_color ( c, colormap, 62208, 62208, 62208 ), NULL );
	lightGreyPixel = reply->pixel;
	free( reply );

	reply = xcb_alloc_color_reply ( c, xcb_alloc_color ( c, colormap, 32768, 32768, 32768 ), NULL );
	darkGreyPixel = reply->pixel;
	free( reply );

	reply = xcb_alloc_color_reply ( c, xcb_alloc_color ( c, colormap, 49152, 49152, 49152 ), NULL );
	greyPixel = reply->pixel;
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

	greyContext = xcb_generate_id( c );
	v[0] = greyPixel;
	xcb_create_gc( c, greyContext, screen->root, XCB_GC_FOREGROUND, v );

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

void SetupAtoms() {	
	//WM_NAME = xcb_intern_atom_reply( c, xcb_intern_atom( c, 0, strlen( "WM_NAME" ), "WM_NAME" ), NULL )->atom;
}

void SetupFonts() {
	unsigned int v[3];

	windowFont = xcb_generate_id( c );
	xcb_open_font( c, windowFont, strnlen( FONT_NAME, 256 ), FONT_NAME );

	v[2] = windowFont;

	activeFontContext = xcb_generate_id( c );
	v[0] = blackPixel;
	v[1] = lightGreyPixel;
	xcb_create_gc( c, activeFontContext, screen->root, XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT, v );

	inactiveFontContext = xcb_generate_id( c );
	v[0] = darkGreyPixel;
	v[1] = whitePixel;
	xcb_create_gc( c, inactiveFontContext, screen->root, XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT, v );
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
		printf( "unparenting %x\n", n->window );
		xcb_reparent_window( c, n->window, screen->root, n->x, n->y );
		xcb_destroy_window( c, n->parent );
		xcb_flush( c );
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

void SetRootBackground() {
	xcb_pixmap_t pixmap = xcb_generate_id( c );
	unsigned int v[1] = { pixmap };
	xcb_point_t lightPoints[2] = {{0,0},{1,1}};
	xcb_point_t darkPoints[2] = {{0,1},{1,0}};

	xcb_create_pixmap( c, screen->root_depth, pixmap, screen->root, 2, 2 );
	xcb_poly_point( c, XCB_COORD_MODE_ORIGIN, pixmap, greyContext, 2, lightPoints );
	xcb_poly_point( c, XCB_COORD_MODE_ORIGIN, pixmap, darkGreyContext, 2, darkPoints );
	xcb_change_window_attributes( c, screen->root, XCB_CW_BACK_PIXMAP, v );
	xcb_clear_area( c, 1, screen->root, 0, 0, screen->width_in_pixels - 1, screen->height_in_pixels - 1 );

	xcb_flush( c );
}

void ReparentWindow( xcb_window_t win, short x, short y, unsigned short width, unsigned short height, unsigned char override_redirect ) {
	unsigned int v[2] = { 	whitePixel, 
							XCB_EVENT_MASK_EXPOSURE | 
							XCB_EVENT_MASK_BUTTON_PRESS | 
							XCB_EVENT_MASK_BUTTON_RELEASE | 
							XCB_EVENT_MASK_POINTER_MOTION | 
							XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | 
							XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT };
	client_t *m = firstClient;
	client_t *n = firstClient;

	//if ( parent != screen->root ) {
	//	return;
	//}

	if ( n == NULL ) {
		firstClient = n = malloc( sizeof( client_t ) );
		n->nextClient = NULL;
	} else {
		while ( n != NULL ) {
			if( win == n->window || win == n->parent ) {
					printf( "New frame\n" );
				return;
			}
			m = n;
			n = m->nextClient;
		}
		m->nextClient = n = malloc( sizeof( client_t ) );
		n->nextClient = NULL;
	}
	if ( override_redirect ) {
		n->managementState = STATE_NO_REDIRECT;
		n->window = win;
		n->parent = screen->root;
		printf( "New unreparented window\n" );
		return;
	}

	n->window = win;
	n->width = width;
	n->height = height;
	n->x = x;
	n->y = y;
	n->parentMapped = 0;

	strncpy( n->name, "", 256 );

	n->managementState = STATE_WITHDRAWN;

	n->parent = xcb_generate_id( c );
	xcb_create_window (		c, XCB_COPY_FROM_PARENT, n->parent, screen->root, 
							0, 0, BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT + 1, BORDER_SIZE_TOP + BORDER_SIZE_BOTTOM + 1, 
							0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, 
							XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, v);
	xcb_reparent_window( c, n->window, n->parent, 1, 19 );

	v[0] = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_BUTTON_PRESS | 
							XCB_EVENT_MASK_BUTTON_RELEASE | 
							XCB_EVENT_MASK_POINTER_MOTION;
	xcb_change_window_attributes( c, n->window, XCB_CW_EVENT_MASK, v );

	xcb_flush( c );
}

void ReparentExistingWindows() {
	xcb_query_tree_cookie_t treecookie;
	xcb_query_tree_reply_t *treereply;
	xcb_get_geometry_cookie_t geocookie;
	xcb_get_geometry_reply_t *georeply;
	int i;
	xcb_window_t *children;

	treecookie = xcb_query_tree( c, screen->root );
	treereply = xcb_query_tree_reply( c, treecookie, NULL );
	if ( treereply == NULL ) {
		return;
	}
	children = xcb_query_tree_children( treereply );
	for( i = 0; i < xcb_query_tree_children_length( treereply ); i++ ) {
		geocookie = xcb_get_geometry( c, children[i] );
		georeply = xcb_get_geometry_reply( c, geocookie, NULL );
		if ( georeply != NULL ) {
			ReparentWindow( children[i], georeply->x, georeply->y, georeply->width, georeply->height, 0 );
			free( georeply );
		}
	}
	free( treereply );
}

/*
==============
Event handlers
==============
*/

void DoButtonPress( xcb_button_press_event_t *e ) {
	client_t *n = GetClientByParent( e->event );

	printf( "button press on window %x\n", e->event );
	
	if ( n == NULL ) {
		n = GetClientByWindow( e->event );
		printf( "is client window\n" );
		if ( n == NULL ) {
			return;
		}	
		RaiseClient( n );
		return;
	}
	if ( n->parent == activeWindow ) {
		// (9,4) (20,15)
		if ( mouseIsOverCloseButton ) {
			wmState = WMSTATE_CLOSE;
			printf("Close clicked!\n");
			DrawFrame( n );
			return;
		}
	}
	RaiseClient( n );
	dragClient = n;
	wmState = WMSTATE_DRAG;
	dragStartX = e->event_x;
	dragStartY = e->event_y;
}

void DoButtonRelease( xcb_button_release_event_t *e ) {
	switch ( wmState ) {
		case WMSTATE_IDLE:
			break;
		case WMSTATE_DRAG:
			wmState = WMSTATE_IDLE;
			dragClient = NULL;
			dragStartX = 0;
			dragStartY = 0;
			break;
		case WMSTATE_CLOSE:
			if ( mouseIsOverCloseButton == 1 ) {
				printf( "Close window now!\n" );
			}
			wmState = WMSTATE_IDLE;
			DrawFrame( GetClientByParent( activeWindow ) );
			break;
		default:
			wmState = WMSTATE_IDLE;
			break;
	}
}

void DoMotionNotify( xcb_motion_notify_event_t *e ) {
	int x;
	int y;

	mouseLastKnownX = e->root_x;
	mouseLastKnownY = e->root_y;

	mouseIsOverCloseButton = 0;
	if ( e->event_x >= 9 && e->event_x <= 20 ) {
		if ( e->event_y >= 4 && e->event_y <= 15 ) {
			mouseIsOverCloseButton = 1;
		}
	}

	if ( wmState == WMSTATE_CLOSE ) {
		DrawFrame( GetClientByParent( activeWindow ) );
	}

	if ( wmState == WMSTATE_DRAG ) {
		x = e->root_x - dragStartX;
		y =  e->root_y - dragStartY;
		ConfigureClient( dragClient, x, y, dragClient->width, dragClient->height );
	}
}

void DoExpose( xcb_expose_event_t *e ) {
	DrawFrame( GetClientByParent( e->window ) );
}

void DoCreateNotify( xcb_create_notify_event_t *e ) {
	if ( e->parent != screen->root ) {
		return;
	}
	ReparentWindow( e->window, e->x, e->y, e->width, e->height, e->override_redirect );
}

void DoDestroy( xcb_destroy_notify_event_t *e ) {
	client_t *n = firstClient;
	client_t *m = firstClient;

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
			if( dragClient == n ) {
				dragClient = NULL;
			}
			free( n );
			n = NULL;
			xcb_flush( c );
			return;
		}
	}
}

void DoMapRequest( xcb_map_request_event_t *e ) {
	client_t *n = GetClientByWindow( e->window );

	/* Todo:
		Add ICCCM section 4.1.4 compatibility
		https://tronche.com/gui/x/icccm/sec-4.html#s-4.1.3
	*/
	if ( n == NULL ) {
		return;
	}
	n->windowState = STATE_NORMAL;
	xcb_map_window( c, n->parent );
	xcb_map_window( c, n->window );
	xcb_flush( c );
	RaiseClient( n );
	printf( "window %x mapped\n", e->window );
}

//This is all a hack to make ReparentExistingWindows work.
//It relies on the fact that X will unmap then remap windows on reparent,
//if and only if they are already mapped
void DoMapNotify( xcb_map_notify_event_t *e ) {
	client_t *n = GetClientByWindow( e->window );

	if ( n == NULL ) {
		return;
	}
	if ( n->parentMapped == 0 ) {  
		n->windowState = STATE_NORMAL;
		n->parentMapped = 1;
		xcb_map_window( c, n->parent );
		xcb_flush( c );
		RaiseClient( n );
	}
}

void DoReparentNotify( xcb_reparent_notify_event_t *e ) {
	client_t *n = GetClientByWindow( e->window );

	if ( n == NULL ) {
		return;
	}

	n->managementState = STATE_REPARENTED;
	ConfigureClient( n, n->x, n->y, n->width, n->height );
	printf( "window %x reparented to window %x\n", e->window, e->parent );
	return;
}

void DoConfigureRequest( xcb_configure_request_event_t *e ) {
	ConfigureClient( GetClientByWindow( e->window ), e->x, e->y, e->width, e->height );
}

void DoConfigureNotify( xcb_configure_notify_event_t *e ) {
	client_t *n = GetClientByWindow( e->window );
	
	if ( n == NULL ) {
		client_t *n = GetClientByParent( e->window );
		if ( n == NULL ) {
			return;
		}
		n->x = e->x;
		n->y = e->y;
		n->width = e->width - ( BORDER_SIZE_LEFT + BORDER_SIZE_RIGHT );
		n->height = e->height - ( BORDER_SIZE_TOP + BORDER_SIZE_BOTTOM );
		return;
	}
	n->x = e->x;
	n->y = e->y;
	n->width = e->width;
	n->height = e->height;
}

void DoPropertyNotify( xcb_property_notify_event_t *e ) {
	xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
	client_t *n = GetClientByWindow( e->window );

	if ( n == NULL ) {
		return;
	}

	if ( e->atom == XCB_ATOM_WM_NAME ) {
		cookie = xcb_get_property( c, 0, e->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 256 );
		if ((reply = xcb_get_property_reply(c, cookie, NULL))) {
			int len = xcb_get_property_value_length(reply);
			if ( len != 0 ) {
				strncpy( n->name, (char*)xcb_get_property_value( reply ), len > 255 ? 255 : len );
				DrawFrame( n );
			}
		}	
		free( reply );
	} else {
		//printf( "window %x updated unknown atom\n", e->window );
	}
}

/*
=============
Main function
=============
*/

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
	SetupAtoms();
	SetupColors();
	SetupFonts();
	SetRootBackground();
	ReparentExistingWindows();

	while( ( e = xcb_wait_for_event( c ) ) != NULL ) {
		
		switch( e->response_type & ~0x80 ) {

			case XCB_BUTTON_PRESS:
				DoButtonPress( (xcb_button_press_event_t *)e );
				break;
			case XCB_BUTTON_RELEASE:
				DoButtonRelease( (xcb_button_release_event_t *)e );
				break;
			case XCB_MOTION_NOTIFY:
				DoMotionNotify( (xcb_motion_notify_event_t *)e );
				break;
			case XCB_EXPOSE:
				DoExpose( (xcb_expose_event_t *)e );
				break;
			case XCB_CREATE_NOTIFY: 
				DoCreateNotify( (xcb_create_notify_event_t *)e );
				break;
			case XCB_DESTROY_NOTIFY:
				DoDestroy( (xcb_destroy_notify_event_t *)e );
				break;
			case XCB_MAP_NOTIFY:
				DoMapNotify( (xcb_map_notify_event_t *)e );
			case XCB_MAP_REQUEST:
				DoMapRequest( (xcb_map_request_event_t *)e );
				break;
			case XCB_REPARENT_NOTIFY:
				DoReparentNotify( (xcb_reparent_notify_event_t *)e );
				break;
			case XCB_CONFIGURE_NOTIFY:
				DoConfigureNotify( (xcb_configure_notify_event_t *)e );
				break;
			case XCB_CONFIGURE_REQUEST:
				DoConfigureRequest( (xcb_configure_request_event_t *)e );
				break;
			case XCB_PROPERTY_NOTIFY:
				DoPropertyNotify( (xcb_property_notify_event_t *)e );
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
/*
 * Copyright 2011 Sven Weidauer <sven.weidauer@gmail.com>
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

#import "cocoa/BookmarksController.h"
#import "cocoa/Tree.h"
#import "cocoa/NetsurfApp.h"
#import "cocoa/BrowserViewController.h"

#import "desktop/hotlist.h"
#import "desktop/tree.h"
#import "desktop/tree_url_node.h"

@implementation BookmarksController

@synthesize defaultMenu;

- init;
{
	if ((self = [super init]) == nil) return nil;
	
	tree = [[Tree alloc] initWithFlags: hotlist_get_tree_flags()];
	hotlist_initialise( [tree tree], "/Users/sven/hotlist", "" );
	nodeForMenu = NSCreateMapTable( NSNonOwnedPointerMapKeyCallBacks, NSNonOwnedPointerMapValueCallBacks, 0 );
	
	return self;
}

- (void) dealloc;
{
	NSFreeMapTable( nodeForMenu );
	hotlist_cleanup( "/Users/sven/hotlist" );
	[tree release];
	[super dealloc];
}

- (void) menuNeedsUpdate: (NSMenu *)menu
{
	for (NSMenuItem *item in [menu itemArray]) {
		if ([item hasSubmenu]) NSMapRemove( nodeForMenu, [item submenu] );
		[menu removeItem: item];
	}

	bool hasSeparator = true;
	struct node *node = (struct node *)NSMapGet( nodeForMenu, menu );
	if (node == NULL) {
		for (NSMenuItem *item in [defaultMenu itemArray]) {
			[menu addItem: [[item copy] autorelease]];
		}
		hasSeparator = false;
		node = [tree rootNode];
	}
	
	for (struct node *child = tree_node_get_child( node ); 
		 child != NULL; 
		 child = tree_node_get_next( child )) {
		
		if (tree_node_is_deleted( child )) continue;
		
		if (!hasSeparator) {
			[menu addItem: [NSMenuItem separatorItem]];
			hasSeparator = true;
		}
		
		NSString *title = [NSString stringWithUTF8String: tree_url_node_get_title( child )];
		
		NSMenuItem *item = [menu addItemWithTitle: title action: NULL keyEquivalent: @""];
		if (tree_node_is_folder( child )) {
			NSMenu *subMenu = [[[NSMenu alloc] initWithTitle: title] autorelease];
			NSMapInsert( nodeForMenu, subMenu, child );
			[subMenu setDelegate: self];
			[menu setSubmenu: subMenu forItem: item];
		} else {
			[item setRepresentedObject: [NSString stringWithUTF8String: tree_url_node_get_url( child )]];
			[item setTarget: self];
			[item setAction: @selector( openBookmarkURL: )];
		}
	}
}

- (IBAction) openBookmarkURL: (id) sender;
{
	const char *url = [[sender representedObject] UTF8String];
	NSParameterAssert( url != NULL );
	
	BrowserViewController *tab = [(NetSurfApp *)NSApp frontTab];
	if (tab != nil) {
		browser_window_go( [tab browser], url, NULL, true );
	} else {
		browser_window_create( url, NULL, NULL, true, false );
	}
}

- (IBAction) showBookmarksWindow: (id) sender;
{
	NSLog( @"TODO: show bookmarks window" );
}

- (IBAction) addBookmark: (id) sender;
{
	NSLog( @"TODO: add bookmark" );
}

- (BOOL) validateUserInterfaceItem: (id) item;
{
	SEL action = [item action];
	
	if (action == @selector( addBookmark: )) {
		return [(NetSurfApp *)NSApp frontTab] != nil;
	}
	
	return YES;
}

@end
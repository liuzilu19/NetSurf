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

#import "DownloadWindowController.h"

#import "desktop/download.h"
#import "desktop/gui.h"

@interface DownloadWindowController ()

@property (readwrite, retain, nonatomic) NSFileHandle *outputFile;
@property (readwrite, retain, nonatomic) NSMutableData *savedData;
@property (readwrite, copy, nonatomic) NSDate *startDate;

- (void)savePanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (void)alertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;

- (BOOL) receivedData: (NSData *)data;

- (void) showError: (NSString *)error;
- (void) downloadDone;
- (void) removeIfPossible;

@end

static void cocoa_unregister_download( DownloadWindowController *download );
static void cocoa_register_download( DownloadWindowController *download );


@implementation DownloadWindowController

- initWithContext: (struct download_context *)ctx;
{
	if ((self = [super initWithWindowNibName: @"DownloadWindow"]) == nil) return nil;
	
	context = ctx;
	totalSize = download_context_get_total_length( context );
	[self setURL: [NSURL URLWithString: [NSString stringWithUTF8String: download_context_get_url( context )]]];
	[self setMIMEType: [NSString stringWithUTF8String: download_context_get_mime_type( context )]];
	[self setStartDate: [NSDate date]];
	
	return self;
}

- (void) dealloc;
{
	download_context_destroy( context );
	
	[self setURL: nil];
	[self setMIMEType: nil];
	[self setSaveURL: nil];
	[self setOutputFile: nil];
	[self setSavedData: nil];
	[self setStartDate: nil];
	
	[super dealloc];
}

- (void) abort;
{
	download_context_abort( context );
	[self removeIfPossible];
}

- (void) askForSave;
{
	canClose = NO;
	[[NSSavePanel savePanel] beginSheetForDirectory: nil 
											   file: [[url path] lastPathComponent]
									 modalForWindow: [self window] 
									  modalDelegate: self 
									 didEndSelector: @selector(savePanelDidEnd:returnCode:contextInfo:) 
										contextInfo: NULL];
}

- (void)savePanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
{
	canClose = YES;

	if (returnCode == NSCancelButton) {
		[self abort];
		return;
	}

	NSURL *targetURL = [sheet URL];
	NSString *path = [targetURL path];
	
	[[NSFileManager defaultManager] createFileAtPath: path contents: nil attributes: nil];
	[self setOutputFile: [NSFileHandle fileHandleForWritingAtPath: path]];
	[self setSaveURL: targetURL];
	
	NSWindow *win = [self window];
	[win setRepresentedURL: targetURL];
	[win setTitle: [self fileName]];
	
	if (nil == outputFile) {
		[self performSelector: @selector(showError:) withObject: @"Cannot create file" afterDelay: 0];
		return;
	}

	if (nil != savedData) {
		[outputFile writeData: savedData];
		[self setSavedData: nil];
	}

	[self removeIfPossible];
}

- (BOOL) receivedData: (NSData *)data;
{
	if (outputFile) {
		[outputFile writeData: data];
	} else {
		if (nil == savedData) [self setSavedData: [NSMutableData data]];
		[savedData appendData: data];
	}
	
	[self setReceivedSize: receivedSize + [data length]];
	
	return YES;
}

- (void) showError: (NSString *)error;
{
	canClose = NO;
	NSAlert *alert = [NSAlert alertWithMessageText: @"Error" defaultButton: @"OK" 
								   alternateButton: nil otherButton: nil 
						 informativeTextWithFormat: @"%@", error];
	
	[alert beginSheetModalForWindow: [self window] modalDelegate: self 
					 didEndSelector: @selector(alertDidEnd:returnCode:contextInfo:)
						contextInfo: NULL];
}

- (void)alertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;
{
	[self abort];
}

- (void) removeIfPossible;
{
	if (canClose && shouldClose) {
		cocoa_unregister_download( self );
	}
}
- (void) downloadDone;
{
	shouldClose = YES;
	[self removeIfPossible];
}

#pragma mark -
#pragma mark Properties

@synthesize URL = url;
@synthesize MIMEType = mimeType;
@synthesize totalSize;
@synthesize saveURL;
@synthesize outputFile;
@synthesize savedData;
@synthesize receivedSize;
@synthesize startDate;

+ (NSSet *) keyPathsForValuesAffectingStatusText;
{
	return [NSSet setWithObjects: @"totalSize", @"receivedSize", nil];
}

#ifndef NSAppKitVersionNumber10_5
#define NSAppKitVersionNumber10_5 949
#endif 

static NSString *cocoa_file_size_string( float size )
{
	static unsigned factor = 0;
	if (factor == 0) {
		if (floor( NSAppKitVersionNumber ) > NSAppKitVersionNumber10_5) factor = 1000;
		else factor = 1024;
	}
	
	if (size == 0) return @"nothing";
	if (size <= 1.0) return @"1 byte";
	
    if (size < factor - 1) return [NSString stringWithFormat:@"%1.0f bytes",size];
	
    size /= factor;
    if (size < factor - 1) return [NSString stringWithFormat:@"%1.1f KB", size];
    
	size /= factor;
    if (size < factor - 1) return [NSString stringWithFormat:@"%1.1f MB", size];

    size /= factor;
	if (size < factor - 1) return [NSString stringWithFormat:@"%1.1f GB", size];
	
	size /= factor;
	return [NSString stringWithFormat:@"%1.1f TB", size];
}

- (NSString *) statusText;
{
	NSString *speedString = @"";
	
	float elapsedTime = [[NSDate date] timeIntervalSinceDate: startDate];
	if (elapsedTime >= 0.1) {
		float speed = (float)receivedSize / elapsedTime;
		speedString = [NSString stringWithFormat: @" (%@/s)", cocoa_file_size_string( speed )];
	}
	
	NSString *totalSizeString = @"";
	if (totalSize != 0) {
		totalSizeString = [NSString stringWithFormat: @" of %@", cocoa_file_size_string( totalSize )];
	}
	
	return [NSString stringWithFormat: @"%@%@%@", cocoa_file_size_string( receivedSize ), 
			totalSizeString, speedString];
}

+ (NSSet *) keyPathsForValuesAffectingFileName;
{
	return [NSSet setWithObject: @"saveURL"];
}

- (NSString *) fileName;
{
	return [[saveURL path] lastPathComponent];
}

+ (NSSet *) keyPathsForValuesAffectingIcon;
{
	return [NSSet setWithObjects: @"mimeType", @"URL", nil];
}

- (NSImage *) icon;
{
	NSString *type = [(NSString *)UTTypeCreatePreferredIdentifierForTag( kUTTagClassMIMEType, (CFStringRef)mimeType, NULL ) autorelease];
	if ([type hasPrefix: @"dyn."] || [type isEqualToString: (NSString *)kUTTypeData]) {
		NSString *pathExt = [[url path] pathExtension];
		type = [(NSString *)UTTypeCreatePreferredIdentifierForTag( kUTTagClassFilenameExtension, (CFStringRef)pathExt, NULL ) autorelease];
	}
	return [[NSWorkspace sharedWorkspace] iconForFileType: type];
}


#pragma mark -
#pragma mark NetSurf interface functions

struct gui_download_window *gui_download_window_create(download_context *ctx,
													   struct gui_window *parent)
{
	DownloadWindowController * const window = [[DownloadWindowController alloc] initWithContext: ctx];
	cocoa_register_download( window );
	[window askForSave];
	[window release];
	
	return (struct gui_download_window *)window;
}

nserror gui_download_window_data(struct gui_download_window *dw, 
								 const char *data, unsigned int size)
{
	DownloadWindowController * const window = (DownloadWindowController *)dw;
	return [window receivedData: [NSData dataWithBytes: data length: size]] ? NSERROR_OK : NSERROR_SAVE_FAILED;
}

void gui_download_window_error(struct gui_download_window *dw,
							   const char *error_msg)
{
	DownloadWindowController * const window = (DownloadWindowController *)dw;
	[window showError: [NSString stringWithUTF8String: error_msg]];
}

void gui_download_window_done(struct gui_download_window *dw)
{
	DownloadWindowController * const window = (DownloadWindowController *)dw;
	[window downloadDone];
}

@end

#pragma mark -
static NSMutableSet *cocoa_all_downloads = nil;

static void cocoa_register_download( DownloadWindowController *download )
{
	if (cocoa_all_downloads == nil) {
		cocoa_all_downloads = [[NSMutableSet alloc] init];
	}
	[cocoa_all_downloads addObject: download];
}

static void cocoa_unregister_download( DownloadWindowController *download )
{
	[cocoa_all_downloads removeObject: download];
}


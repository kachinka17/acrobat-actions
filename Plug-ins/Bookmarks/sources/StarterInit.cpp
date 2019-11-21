/*********************************************************************

 Copyright (C) 2018-2019 Chun Tian (binghe)
 Copyright (C) 1998-2006 Adobe Systems Incorporated
 All rights reserved.

 NOTICE: Adobe permits you to use, modify, and distribute this file
 in accordance with the terms of the Adobe license agreement
 accompanying it. If you have received this file from a source other
 than Adobe, then your use, modification, or distribution of it
 requires the prior written permission of Adobe.

 ---------------------------------------------------------------------

 StarterInit.cpp

 - Skeleton .cpp file for a plug-in. It implements the basic
   handshaking methods required for all plug-ins.

*********************************************************************/

// Acrobat Headers.
#ifndef MAC_PLATFORM
#include "PIHeaders.h"
#endif

#include <cstdio>
#include <cstring>

#include "common-config.h"

#ifdef Platform_Windows
extern "C" {
#include "strcasestr.h"
}
#else
#include <strings.h>
#endif

/** 
  Starter is a plug-in template that provides a minimal
  implementation for a plug-in. Developers may use this plug-in a
  basis for their plug-ins.
 */

/*-------------------------------------------------------
	Constants/Declarations
 -------------------------------------------------------*/
static AVMenuItem topMenuItem = NULL;
static AVMenuItem menuItem[3] = {NULL, NULL, NULL};
static AVMenu subMenu = NULL;

ACCB1 ASBool ACCB2 FindPluginMenu(void);

/*-------------------------------------------------------
	Core Handshake Callbacks
-------------------------------------------------------*/

/* PluginExportHFTs
** ------------------------------------------------------
**/

/**
** Create and register the HFT's.
**
** @return true to continue loading plug-in,
** false to cause plug-in loading to stop.
*/
ACCB1 ASBool ACCB2 PluginExportHFTs(void)
{
    return true;
}

/* PluginImportReplaceAndRegister
** ------------------------------------------------------
** */
/** 
	The application calls this function to allow it to
	<ul>
	<li> Import plug-in supplied HFTs.
	<li> Replace functions in the HFTs you're using (where allowed).
	<li> Register to receive notification events.
	</ul>

	@return true to continue loading plug-in,
	false to cause plug-in loading to stop.
*/
ACCB1 ASBool ACCB2 PluginImportReplaceAndRegister(void)
{
    return true;
}

static const char *pluginMenuName = "Extensions";

/* Find an existing "Plug-ins" menu from the menubar. */
ACCB1 ASBool ACCB2 FindPluginMenu(void)
{
    AVMenubar menubar = AVAppGetMenubar();
    AVMenu volatile commonMenu = NULL;
    ASAtom PluginMenuName = ASAtomFromString(pluginMenuName);

    if (!menubar) return false;

    /* Gets the number of menus in menubar. */
    AVTArraySize nMenu = AVMenubarGetNumMenus(menubar);

    for (AVMenuIndex index = 0; index < nMenu; ++index) {
	/* Acquires the menu with the specified index. */
	AVMenu menu = AVMenubarAcquireMenuByIndex(menubar, index);
	/* get the menu's language-independent name. */
	ASAtom name = AVMenuGetName(menu);
	if (name == PluginMenuName) {
	    AVAlertNote(ASAtomGetString(name));
	}
    }

    return true;
}

void VisitAllBookmarks(PDDoc doc, PDBookmark aBookmark)
{
    PDBookmark treeBookmark;

DURING
    // ensure that the bookmark is valid
    if (!PDBookmarkIsValid(aBookmark)) E_RTRN_VOID;

    // collapse the current bookmark
    if (PDBookmarkIsOpen(aBookmark)) PDBookmarkSetOpen(aBookmark, false);

    // process children bookmarks
    if (PDBookmarkHasChildren(aBookmark)) {
	treeBookmark = PDBookmarkGetFirstChild(aBookmark);

	while (PDBookmarkIsValid(treeBookmark)) {
	    VisitAllBookmarks(doc, treeBookmark);
	    treeBookmark = PDBookmarkGetNext(treeBookmark);
	}
    }

HANDLER
END_HANDLER
}

int FixAllBookmarks(PDDoc doc, PDBookmark aBookmark, int acc)
{
    PDBookmark treeBookmark;
    PDViewDestination newDest;
    PDAction newAction;
    ASText title = ASTextNew(); // to be filled by PDBookmarkGetTitleASText()

DURING
    // ensure that the bookmark is valid
    if (!PDBookmarkIsValid(aBookmark)) return acc;

    // Fixing actions
    PDAction action = PDBookmarkGetAction(aBookmark);
    if (PDActionIsValid(action)) {
	PDViewDestination dest = PDActionGetDest(action);
	if (PDViewDestIsValid(dest)) {
	    ASInt32 pageNum;
	    ASAtom fitType, targetFitType = ASAtomFromString("XYZ");
	    ASFixedRect destRect;
	    ASFixed zoom;
	    PDViewDestGetAttr(dest, &pageNum, &fitType, &destRect, &zoom);
	    if (fitType != targetFitType || zoom != PDViewDestNULL) {
		PDPage page = PDDocAcquirePage(doc, pageNum);
		newDest =
		PDViewDestCreate(doc,
				 page,	    /* := pageNum */
				 targetFitType,   /* XYZ */
				 &destRect,
				 PDViewDestNULL,  /* when FitType is XYZ */
				 0);		    /* unused */
		newAction = PDActionNewFromDest(doc, newDest, doc);
		PDBookmarkSetAction(aBookmark, newAction);
		PDViewDestDestroy(dest);
		PDActionDestroy(action);
		acc++;
	    }
	}
    }

    // Fixing flags for TOC-related bookmarks
    PDBookmarkGetTitleASText(aBookmark, title);
    if (!ASTextIsEmpty(title)) {
	ASUTF16Val *u8 = ASTextGetUnicodeCopy(title, kUTF8);
	if (strcasestr((const char *)u8, "CONTENTS") ||
	    strcasestr((const char *)u8, "Inhalt")) // German "TOC"
	{
	    PDBookmarkSetFlags(aBookmark, 0x2); /* bold font */
	    acc++;
	}
	ASfree((void *)u8);
    }
    ASTextDestroy(title);

    // process children bookmarks
    if (PDBookmarkHasChildren(aBookmark)) {
	treeBookmark = PDBookmarkGetFirstChild(aBookmark);
	
	while (PDBookmarkIsValid(treeBookmark)) {
	    acc = FixAllBookmarks(doc, treeBookmark, acc);
	    treeBookmark = PDBookmarkGetNext(treeBookmark);
	}
    }

HANDLER
    if (PDActionIsValid(newAction)) PDActionDestroy(newAction);
    if (PDViewDestIsValid(newDest)) PDViewDestDestroy(newDest);
END_HANDLER

    return acc;
}

int CapitalizeAllBookmarks(PDDoc doc, PDBookmark b, int acc)
{
    PDBookmark treeBookmark;
    PDViewDestination newDest;
    PDAction newAction;
    ASText oldTitle = ASTextNew(); // to be filled by PDBookmarkGetTitleASText()

DURING
    // ensure that the bookmark is valid
    if (!PDBookmarkIsValid(b)) return acc;

    // change bookmark title
    PDBookmarkGetTitleASText(b, oldTitle);
    if (!ASTextIsEmpty(oldTitle)) {
	// (const char *)str is now available
	ASUTF16Val *u8 = ASTextGetUnicodeCopy(oldTitle, kUTF8);
	// AVAlertNote((const char*) str);
	char *str = (char *) u8;
	size_t len = strlen((const char*)str);
	
	// 1st round: all letters to small cases
	for (int i = 0; i < len; ++i) {
	    // capitalize the current letter and turn off the flag
	    if (str[i] >= 'A' && str[i] <= 'z') {
		str[i] = tolower((const char) str[i]);
	    }
	}

	// A flag to decide if the current letter should be capitalized
	// initially it's always true;
	bool flag = true;
	bool strong_flag = false;
	bool exception = false;

	// 2nd round: selective capitalization
	for (int i = 0; i < len; ++i) {
	    // capitalize the current letter and turn off the flag
	    if (str[i] >= 'A' && str[i] <= 'z') {
		// calculate exceptions
		if (0 != i &&
		    ((str[i] == 'o' && str[i+1] == 'v' && str[i+2] == 'e' &&
		      str[i+3] == 'r' && str[i+4] == ' ') ||
		     (str[i] == 'a' && str[i+1] == 'n' && str[i+2] == 'd' &&
		      str[i+3] == ' ') ||
		     (str[i] == 'f' && str[i+1] == 'o' && str[i+2] == 'r' &&
		      str[i+3] == ' ') ||
		     (str[i] == 't' && str[i+1] == 'h' && str[i+2] == 'e' &&
		      str[i+3] == ' ') ||
		     (str[i] == 'i' && str[i+1] == 'n' && str[i+2] == ' ') ||
		     (str[i] == 'o' && str[i+1] == 'n' && str[i+2] == ' ') ||
		     (str[i] == 'o' && str[i+1] == 'f' && str[i+2] == ' ') ||
		     (str[i] == 'a' && str[i+1] == 't' && str[i+2] == ' ') ||
		     (str[i] == 'a' && str[i+1] == 'n' && str[i+2] == ' ') ||
		     (str[i] == 'a' && str[i+1] == ' ') ||
		     false)) {
		    exception = true; // used only once below, then reset
		}

		if (flag || strong_flag) {
		    if (!exception) {
			str[i] = toupper((const char) str[i]);
		    }
		    
		    /* Exception 2: capitalize all roman numerals & abbrevs */
		    if ((str[i] == 'I' && str[i+1] == 'i') ||
			(str[i] == 'I' && str[i+1] == 'v') ||
			(str[i] == 'G' && str[i+1] == 'c' && str[i+2] == 'h' &&
			 str[i+3] == ' ') ||
			false) {
			strong_flag = true; // effective until next space
		    } else {
			flag = false;
		    }
		}
		exception = false;
	    }

	    if (' ' == str[i]) {
		flag = true;
		strong_flag = false;
	    }
	}
	// AVAlertNote((const char*) str);

	ASText newTitle = ASTextFromUnicode(u8, kUTF8);
	ASfree((void *)u8);
	PDBookmarkSetTitleASText(b, newTitle);
	ASTextDestroy(newTitle);
	++acc;
    }
    ASTextDestroy(oldTitle);

    // process children bookmarks
    if (PDBookmarkHasChildren(b)) {
	treeBookmark = PDBookmarkGetFirstChild(b);
	
	while (PDBookmarkIsValid(treeBookmark)) {
	    acc = CapitalizeAllBookmarks(doc, treeBookmark, acc);
	    treeBookmark = PDBookmarkGetNext(treeBookmark);
	}
    }

HANDLER
END_HANDLER
    return acc;
}

/* Collapse All Bookmarks */
ACCB1 void ACCB2 PluginCommand_0(void *clientData)
{
    // try to get front PDF document
    AVDoc avDoc = AVAppGetActiveDoc();
    PDDoc pdDoc = AVDocGetPDDoc(avDoc);
    PDBookmark rootBookmark = PDDocGetBookmarkRoot(pdDoc);

    // visit all bookmarks recursively
    VisitAllBookmarks(pdDoc, rootBookmark);

    return;
}

#define NOTESIZ 200
static char notes[NOTESIZ] = "";

/* Fix FitType of all Bookmarks */
ACCB1 void ACCB2 PluginCommand_1(void *clientData)
{
    // try to get front PDF document
    AVDoc avDoc = AVAppGetActiveDoc();
    PDDoc pdDoc = AVDocGetPDDoc(avDoc);
    PDBookmark rootBookmark = PDDocGetBookmarkRoot(pdDoc);
    int acc = 0;

    // visit all bookmarks recursively, fixing FitView
    acc = FixAllBookmarks(pdDoc, rootBookmark, acc);
    snprintf(notes, NOTESIZ, "Changed %d bookmarks.", acc);
    AVAlertNote((const char*) notes);

    return;
}

/* Capitalize all Bookmarks */
ACCB1 void ACCB2 PluginCommand_2(void *clientData)
{
    // try to get front PDF document
    AVDoc avDoc = AVAppGetActiveDoc();
    PDDoc pdDoc = AVDocGetPDDoc(avDoc);
    PDBookmark rootBookmark = PDDocGetBookmarkRoot(pdDoc);
    int acc = 0;

    // visit all bookmarks recursively, capitalizing them
    acc = CapitalizeAllBookmarks(pdDoc, rootBookmark, acc);
    snprintf(notes, NOTESIZ, "Changed %d bookmarks.", acc);
    AVAlertNote((const char*) notes);

    return;
}

ACCB1 ASBool ACCB2 PluginIsEnabled(void *clientData)
{
    // this code make it is enabled only if there is an open PDF document.
    return (AVAppGetActiveDoc() != NULL);
}

ACCB1 ASBool ACCB2 PluginSetMenu()
{
    AVMenubar menubar = AVAppGetMenubar();
    AVMenu volatile commonMenu = NULL;
    
    if (!menubar) return false;
    
DURING
    // Create our menu, title is not important
    subMenu = AVMenuNew("XXX", "CHUN:PluginsMenu", gExtensionID);

    // Create our menuitem
    topMenuItem = AVMenuItemNew("Chun Tian", "CHUN:PluginsMenuItem",
				subMenu, true, NO_SHORTCUT, 0, NULL,
				gExtensionID);
    // Command 0
    menuItem[0] = AVMenuItemNew("Collapse All Bookmarks", "CHUN:Col_Bookmarks",
				NULL, /* submenu */
				true, /* longMenusOnly */
				NO_SHORTCUT, 0 /* flags */,
				NULL /* icon */, gExtensionID);
    AVMenuItemSetExecuteProc
      (menuItem[0], ASCallbackCreateProto(AVExecuteProc, PluginCommand_0), NULL);

    AVMenuItemSetComputeEnabledProc
      (menuItem[0], ASCallbackCreateProto(AVComputeEnabledProc, PluginIsEnabled),
       (void *)pdPermEdit);
    AVMenuAddMenuItem(subMenu, menuItem[0], APPEND_MENUITEM);

    // Command 1
    menuItem[1] = AVMenuItemNew("Fix FitType of All Bookmarks", "CHUN:Fix_Bookmarks",
				NULL, /* submenu */
				true, /* longMenusOnly */
				NO_SHORTCUT, 0 /* flags */,
				NULL /* icon */, gExtensionID);
    AVMenuItemSetExecuteProc
      (menuItem[1], ASCallbackCreateProto(AVExecuteProc, PluginCommand_1), NULL);

    AVMenuItemSetComputeEnabledProc
      (menuItem[1], ASCallbackCreateProto(AVComputeEnabledProc, PluginIsEnabled),
       (void *)pdPermEdit);
    AVMenuAddMenuItem(subMenu, menuItem[1], APPEND_MENUITEM);

    // Command 2
    menuItem[2] = AVMenuItemNew("Capitalize All Bookmarks", "CHUN:Cap_Bookmarks",
				NULL, /* submenu */
				true, /* longMenusOnly */
				NO_SHORTCUT, 0 /* flags */,
				NULL /* icon */, gExtensionID);
    AVMenuItemSetExecuteProc
    (menuItem[2], ASCallbackCreateProto(AVExecuteProc, PluginCommand_2), NULL);

    AVMenuItemSetComputeEnabledProc
    (menuItem[2], ASCallbackCreateProto(AVComputeEnabledProc, PluginIsEnabled),
     (void *)pdPermEdit);
    AVMenuAddMenuItem(subMenu, menuItem[2], APPEND_MENUITEM);

    /* Acquire() needs a Release() */
    commonMenu = AVMenubarAcquireMenuByName(menubar, pluginMenuName);
    // if "Extensions" menu doesn't exist, create one.
    if (!commonMenu) {
	commonMenu = AVMenuNew(pluginMenuName, pluginMenuName, gExtensionID);
	AVMenubarAddMenu(menubar, commonMenu, APPEND_MENU);
    }

    AVMenuAddMenuItem(commonMenu, topMenuItem, APPEND_MENUITEM);
    AVMenuRelease(commonMenu);
    
HANDLER
    if (commonMenu) AVMenuRelease (commonMenu);
    return false;
    
END_HANDLER
    return true;
}

/* PluginInit
** ------------------------------------------------------
**/
/** 
	The main initialization routine.
	
	@return true to continue loading the plug-in, 
	false to cause plug-in loading to stop.
*/
ACCB1 ASBool ACCB2 PluginInit(void)
{
    return PluginSetMenu();
}

/* PluginUnload
** ------------------------------------------------------
**/
/** 
	The unload routine.
	Called when your plug-in is being unloaded when the application quits.
	Use this routine to release any system resources you may have
	allocated.

	Returning false will cause an alert to display that unloading failed.
	@return true to indicate the plug-in unloaded.
*/
ACCB1 ASBool ACCB2 PluginUnload(void)
{
    return true;
}

/* GetExtensionName
** ------------------------------------------------------
*/
/**
	Returns the unique ASAtom associated with your plug-in.
	@return the plug-in's name as an ASAtom.
*/
ASAtom GetExtensionName()
{
    return ASAtomFromString("CHUN:Bookmarks");
}

/** PIHandshake
    function provides the initial interface between your plug-in and the application.
    This function provides the callback functions to the application that allow it to
    register the plug-in with the application environment.

    Required Plug-in handshaking routine: <b>Do not change its name!</b>
	
    @param handshakeVersion the version this plug-in works with. There are two versions possible, the plug-in version
    and the application version. The application calls the main entry point for this plug-in with its version.
    The main entry point will call this function with the version that is earliest.
    @param handshakeData OUT the data structure used to provide the primary entry points for the plug-in. These
    entry points are used in registering the plug-in with the application and allowing the plug-in to register for
    other plug-in services and offer its own.
    @return true to indicate success, false otherwise (the plug-in will not load).
*/
ACCB1 ASBool ACCB2 PIHandshake(Uns32 handshakeVersion, void *handshakeData)
{
    if (handshakeVersion == HANDSHAKE_V0200) {
	/* Cast handshakeData to the appropriate type */
	PIHandshakeData_V0200 *hsData = (PIHandshakeData_V0200 *)handshakeData;
	
	/* Set the name we want to go by */
	hsData->extensionName = GetExtensionName();
	
	/* If you export your own HFT, do so in here */
	hsData->exportHFTsCallback =
	  (void*)ASCallbackCreateProto(PIExportHFTsProcType, &PluginExportHFTs);
	
	/*
	** If you import plug-in HFTs, replace functionality, and/or want to register for
	** notifications before
	** the user has a chance to do anything, do so in here.
	*/
	hsData->importReplaceAndRegisterCallback =
	    (void*)ASCallbackCreateProto(PIImportReplaceAndRegisterProcType,
					 &PluginImportReplaceAndRegister);

	/* Perform your plug-in's initialization in here */
	hsData->initCallback = (void*)ASCallbackCreateProto(PIInitProcType, &PluginInit);
	
	/* Perform any memory freeing or state saving on "quit" in here */
	hsData->unloadCallback =
	    (void*)ASCallbackCreateProto(PIUnloadProcType, &PluginUnload);
	
	/* All done */
	return true;
	
    } /* Each time the handshake version changes, add a new "else if" branch */
    
    /*
     ** If we reach here, then we were passed a handshake version number we don't know about.
     ** This shouldn't ever happen since our main() routine chose the version number.
     */
    return false;
}

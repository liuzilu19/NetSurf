/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_internal_html_document_h_
#define dom_internal_html_document_h_

#include <dom/html/html_document.h>

#include "core/document.h"

struct dom_hubbub_parser;
struct dom_xml_parser;

/**
 * The dom_html_document class
 */
struct dom_html_document {
	struct dom_document base;	/**< The base class */
	union {
		struct dom_hubbub_parser *hp;
		struct dom_xml_parser *xp;
	} parser;
		/**< The underlaying parser of this document */
	
	struct dom_string *title;	/**< HTML document title */
	struct dom_string *referer;	/**< HTML document referer */
	struct dom_string *domain;	/**< HTML document domain */
	struct dom_string *url;		/**< HTML document URL */
	struct dom_string *cookie;	/**< HTML document cookie */
};

/* Initialise a HTMLDocument */
dom_exception _dom_html_document_initialise(dom_html_document *doc,
		dom_alloc alloc, void *pw, dom_msg msg, void *msg_pw,
		dom_events_default_action_fetcher daf, dom_ui_handler *ui,
		dom_parser_type pt);
/* Finalise a HTMLDocument */
void _dom_html_document_finalise(dom_html_document *doc);
/* Destroy a HTMLDocument */
void _dom_html_document_destroy(dom_html_document *doc);

#endif


/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku.com>
 */

#include <assert.h>
#include <stdlib.h>

#include "html/html_document.h"
#include "html/html_element.h"

#include "core/node.h"
#include "core/attr.h"
#include "core/document.h"
#include "utils/utils.h"

static struct dom_html_element_vtable _dom_html_element_vtable = {
	{
		{
			{
				DOM_NODE_EVENT_TARGET_VTABLE
			},
			DOM_NODE_VTABLE_ELEMENT,
		},
		DOM_ELEMENT_VTABLE
	},
	DOM_HTML_ELEMENT_VTABLE
};

static struct dom_element_protected_vtable _dom_html_element_protect_vtable = {
	{
		DOM_HTML_ELEMENT_PROTECT_VTABLE
	},
	DOM_ELEMENT_PROTECT_VTABLE
};

dom_exception _dom_html_element_create(struct dom_html_document *doc,
		dom_string *name, dom_string *namespace,
		dom_string *prefix, struct dom_html_element **result)
{
	dom_exception error;
	dom_html_element *el;

	el = malloc(sizeof(struct dom_html_element));
	if (el == NULL)
		return DOM_NO_MEM_ERR;

	el->base.base.base.vtable = &_dom_html_element_vtable;
	el->base.base.vtable = &_dom_html_element_protect_vtable;

	error = _dom_html_element_initialise(doc, el, name, namespace,
			prefix);
	if (error != DOM_NO_ERR) {
		free(el);
		return error;
	}

	*result = el;

	return DOM_NO_ERR;
}

dom_exception _dom_html_element_initialise(struct dom_html_document *doc,
		struct dom_html_element *el, dom_string *name, 
		dom_string *namespace, dom_string *prefix)
{
	dom_exception err;

	err = _dom_element_initialise(&doc->base, &el->base, name, namespace, prefix);
	if (err != DOM_NO_ERR)
		return err;
	
	return err;
}

void _dom_html_element_finalise(struct dom_html_element *ele)
{
	_dom_element_finalise(&ele->base);
}

/*------------------------------------------------------------------------*/
/* The protected virtual functions */

/* The virtual destroy function, see src/core/node.c for detail */
void _dom_html_element_destroy(dom_node_internal *node)
{
	dom_html_element *html = (dom_html_element *) node;

	_dom_html_element_finalise(html);

	free(html);
}

/* The virtual copy function, see src/core/node.c for detail */
dom_exception _dom_html_element_copy(dom_node_internal *old,
		dom_node_internal **copy)
{
	return _dom_element_copy(old, copy);
}

/*-----------------------------------------------------------------------*/
/* API functions */

dom_exception _dom_html_element_get_id(dom_html_element *element,
                                       dom_string **id)
{
	dom_exception ret;
	dom_string *_memo_id;
	
	/* Because we're an HTML element, our document is always
	 * an HTML document, so we can get its memoised id string
	 */
	_memo_id = 
		((struct dom_html_document *)
		 ((struct dom_node_internal *)element)->owner)->_memo_id;
	
	ret = dom_element_get_attribute(element, _memo_id, id);
	
	return ret;
}

dom_exception _dom_html_element_set_id(dom_html_element *element,
                                       dom_string *id)
{
        dom_exception ret;
        dom_string *idstr;
        
        ret = dom_string_create_interned((const uint8_t *) "id", SLEN("id"), 
			&idstr);
        if (ret != DOM_NO_ERR)
                return ret;
        
        ret = dom_element_set_attribute(element, idstr, id);
        
        dom_string_unref(idstr);
        
        return ret;
}

dom_exception _dom_html_element_get_title(dom_html_element *element,
                                       dom_string **title)
{
	UNUSED(element);
	UNUSED(title);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_element_set_title(dom_html_element *element,
                                       dom_string *title)
{
	UNUSED(element);
	UNUSED(title);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_element_get_lang(dom_html_element *element,
                                       dom_string **lang)
{
	UNUSED(element);
	UNUSED(lang);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_element_set_lang(dom_html_element *element,
                                       dom_string *lang)
{
	UNUSED(element);
	UNUSED(lang);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_element_get_dir(dom_html_element *element,
                                       dom_string **dir)
{
	UNUSED(element);
	UNUSED(dir);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_element_set_dir(dom_html_element *element,
                                       dom_string *dir)
{
	UNUSED(element);
	UNUSED(dir);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_element_get_classname(dom_html_element *element,
                                       dom_string **classname)
{
	UNUSED(element);
	UNUSED(classname);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_element_set_classname(dom_html_element *element,
                                       dom_string *classname)
{
	UNUSED(element);
	UNUSED(classname);

	return DOM_NOT_SUPPORTED_ERR;
}


/*-----------------------------------------------------------------------*/
/* Common functions */

/**
 * Get the a bool property
 *
 * \param ele   The dom_html_element object
 * \param name  The name of the attribute
 * \param len   The length of ::name
 * \param has   The returned status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_element_get_bool_property(dom_html_element *ele,
		const char *name, unsigned long len, bool *has)
{
	dom_string *str = NULL;
	dom_attr *a = NULL;
	dom_exception err;

	err = dom_string_create((const uint8_t *) name, len, &str);
	if (err != DOM_NO_ERR)
		goto fail;

	err = dom_element_get_attribute_node(ele, str, &a);
	if (err != DOM_NO_ERR)
		goto cleanup1;

	if (a != NULL) {
		*has = true;
	} else {
		*has = false;
	}

	dom_node_unref(a);

cleanup1:
	dom_string_unref(str);

fail:
	return err;
}

/**
 * Set a bool property
 *
 * \param ele   The dom_html_element object
 * \param name  The name of the attribute
 * \param len   The length of ::name
 * \param has   The status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_element_set_bool_property(dom_html_element *ele,
		const char *name, unsigned long len, bool has)
{
	dom_string *str = NULL;
	dom_attr *a = NULL;
	dom_exception err;

	err = dom_string_create((const uint8_t *) name, len, &str);
	if (err != DOM_NO_ERR)
		goto fail;

	err = dom_element_get_attribute_node(ele, str, &a);
	if (err != DOM_NO_ERR)
		goto cleanup1;
	
	if (a != NULL && has == false) {
		dom_attr *res = NULL;

		err = dom_element_remove_attribute_node(ele, a, &res);
		if (err != DOM_NO_ERR)
			goto cleanup2;

		dom_node_unref(res);
	} else if (a == NULL && has == true) {
		dom_document *doc = dom_node_get_owner(ele);
		dom_attr *res = NULL;

		err = _dom_attr_create(doc, str, NULL, NULL, true, &a);
		if (err != DOM_NO_ERR) {
			goto cleanup1;
		}

		err = dom_element_set_attribute_node(ele, a, &res);
		if (err != DOM_NO_ERR)
			goto cleanup2;

		dom_node_unref(res);
	}

cleanup2:
	dom_node_unref(a);

cleanup1:
	dom_string_unref(str);

fail:
	return err;
}


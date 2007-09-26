/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <stddef.h>
#include <string.h>

#include <dom/core/attr.h>
#include <dom/core/document.h>
#include <dom/core/node.h>
#include <dom/core/string.h>

#include "core/attr.h"
#include "core/document.h"
#include "core/entity_ref.h"
#include "core/node.h"
#include "utils/utils.h"

struct dom_element;
struct dom_type_info;

/**
 * DOM node attribute
 */
struct dom_attr {
	struct dom_node base;		/**< Base node */

	bool specified;			/**< Whether attribute was specified
					 * or defaulted */

	struct dom_type_info *schema_type_info;	/**< Type information */

	bool is_id;			/**< Attribute is of type ID */
};

/**
 * Create an attribute node
 *
 * \param doc     The owning document
 * \param name    The name of the node to create
 * \param result  Pointer to location to receive created attribute
 * \return DOM_NO_ERR                on success,
 *         DOM_INVALID_CHARACTER_ERR if ::name is invalid,
 *         DOM_NO_MEM_ERR            on memory exhaustion.
 *
 * ::doc and ::name will have their reference counts increased.
 *
 * The returned attribute will already be referenced.
 */
dom_exception dom_attr_create(struct dom_document *doc,
		struct dom_string *name, struct dom_attr **result)
{
	struct dom_attr *a;
	dom_exception err;

	/** \todo Sanity check the attribute name */

	/* Allocate the element */
	a = dom_document_alloc(doc, NULL, sizeof(struct dom_attr));
	if (a == NULL)
		return DOM_NO_MEM_ERR;

	/* Initialise the base class */
	err = dom_node_initialise(&a->base, doc, DOM_ATTRIBUTE_NODE,
			name, NULL);
	if (err != DOM_NO_ERR) {
		dom_document_alloc(doc, a, 0);
		return err;
	}

	/* Perform our type-specific initialisation */
	a->specified = false;
	a->schema_type_info = NULL;
	a->is_id = false;

	*result = a;

	return DOM_NO_ERR;
}

/**
 * Destroy an attribute node
 *
 * \param doc   The owning document
 * \param attr  The attribute to destroy
 *
 * The contents of ::attr will be destroyed and ::attr will be freed
 */
void dom_attr_destroy(struct dom_document *doc, struct dom_attr *attr)
{
	struct dom_node *c, *d;

	/* Destroy children of this node */
	for (c = attr->base.first_child; c != NULL; c = d) {
		d = c->next;

		/* Detach child */
		c->parent = NULL;

		if (c->refcnt > 0) {
			/* Something is using this child */

			/** \todo add to list of nodes pending deletion */

			continue;
		}

		/* Detach from sibling list */
		c->previous = NULL;
		c->next = NULL;

		dom_node_destroy(c);
	}

	/* Now, clean up this node and destroy it */

	if (attr->schema_type_info != NULL) {
		/** \todo destroy schema type info */
	}

	dom_node_finalise(doc, &attr->base);

	dom_document_alloc(doc, attr, 0);
}

/**
 * Retrieve an attribute's name
 *
 * \param attr    Attribute to retrieve name from
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception dom_attr_get_name(struct dom_attr *attr,
		struct dom_string **result)
{
	/* This is the same as nodeName */
	return dom_node_get_node_name((struct dom_node *) attr, result);
}

/**
 * Determine if attribute was specified or defaulted
 *
 * \param attr    Attribute to inspect
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception dom_attr_get_specified(struct dom_attr *attr, bool *result)
{
	*result = attr->specified;

	return DOM_NO_ERR;
}

/**
 * Retrieve an attribute's value
 *
 * \param attr    Attribute to retrieve value from
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception dom_attr_get_value(struct dom_attr *attr,
		struct dom_string **result)
{
	struct dom_node *a = (struct dom_node *) attr;
	struct dom_node *c;
	uint8_t *rep;
	size_t rep_len;
	size_t rep_alloc;
	dom_exception err;

#define CHUNK 128

	rep = dom_document_alloc(a->owner, NULL, CHUNK);
	if (rep == NULL)
		return DOM_NO_MEM_ERR;

	rep_len = 0;
	rep_alloc = CHUNK;

	/* Traverse children, building a string representation as we go */
	for (c = a->first_child; c != NULL; c = c->next) {
		if (c->type == DOM_TEXT_NODE && c->value != NULL) {
			const uint8_t *data;
			size_t len;

			err = dom_string_get_data(c->value, &data, &len);
			if (err != DOM_NO_ERR) {
				dom_document_alloc(a->owner, rep, 0);
				return err;
			}

			/* Extend buffer, if necessary */
			if (rep_len + len >= rep_alloc) {
				uint8_t *temp;
				size_t required = (rep_len + len) - rep_alloc;

				/* Round required up to a chunk boundary */
				required = 
					(required + CHUNK - 1) & ~(CHUNK - 1);

				temp = dom_document_alloc(a->owner, rep, 
						rep_alloc + required);
				if (temp == NULL) {
					dom_document_alloc(a->owner, rep, 0);
					return DOM_NO_MEM_ERR;
				}

				rep = temp;
				rep_alloc += required;
			}

			/* Copy text into buffer */
			memcpy(rep + rep_len, data, len);

			/* And fix up length information */
			rep_len += len;
		} else if (c->type == DOM_ENTITY_REFERENCE_NODE) {
			struct dom_string *tr;
			const uint8_t *data;
			size_t len;

			/* Get textual representation of entity */
			err = dom_entity_reference_get_textual_representation(
					(struct dom_entity_reference *) c,
					&tr);
			if (err != DOM_NO_ERR) {
				dom_document_alloc(a->owner, rep, 0);
				return err;
			}

			err = dom_string_get_data(tr, &data, &len);
			if (err != DOM_NO_ERR) {
				dom_string_unref(tr);
				dom_document_alloc(a->owner, rep, 0);
				return err;
			}

			/* Extend buffer, if necessary */
			if (rep_len + len >= rep_alloc) {
				uint8_t *temp;
				size_t required = (rep_len + len) - rep_alloc;

				/* Round required up to a chunk boundary */
				required = 
					(required + CHUNK - 1) & ~(CHUNK - 1);

				temp = dom_document_alloc(a->owner, rep, 
						rep_alloc + required);
				if (temp == NULL) {
					dom_document_alloc(a->owner, rep, 0);
					return DOM_NO_MEM_ERR;
				}

				rep = temp;
				rep_alloc += required;
			}

			/* Copy text into buffer */
			memcpy(rep + rep_len, data, len);

			/* And fix up length information */
			rep_len += len;

			/* No longer need textual representation */
			dom_string_unref(tr);
		}
	}

#undef CHUNK

	/* Create DOMString */
	err = dom_string_create_from_ptr(a->owner, rep, rep_len, result);
	if (err != DOM_NO_ERR) {
		dom_document_alloc(a->owner, rep, 0);
		return err;
	}

	/* Cleanup */
	dom_document_alloc(a->owner, rep, 0);

	return DOM_NO_ERR;
}

/**
 * Set an attribute's value
 *
 * \param attr   Attribute to retrieve value from
 * \param value  New value for attribute
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if attribute is readonly.
 */
dom_exception dom_attr_set_value(struct dom_attr *attr,
		struct dom_string *value)
{
	struct dom_node *a = (struct dom_node *) attr;
	struct dom_node *c, *d;
	struct dom_text *text;
	dom_exception err;

	/* Ensure attribute is writable */
	if (_dom_node_readonly(a))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	/* Create text node containing new value */
	err = dom_document_create_text_node(a->owner, value, &text);
	if (err != DOM_NO_ERR)
		return err;

	/* Destroy children of this node */
	for (c = a->first_child; c != NULL; c = d) {
		d = c->next;

		/* Detach child */
		c->parent = NULL;

		if (c->refcnt > 0) {
			/* Something is using this child */

			/** \todo add to list of nodes pending deletion */

			continue;
		}

		/* Detach from sibling list */
		c->previous = NULL;
		c->next = NULL;

		dom_node_destroy(c);
	}

	/* And insert the text node as the value */
	((struct dom_node *) text)->parent = a;
	a->first_child = a->last_child = (struct dom_node *) text;

	return DOM_NO_ERR;
}

/**
 * Retrieve the owning element of an attribute
 *
 * \param attr    The attribute to extract owning element from
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 *
 * The returned node will have its reference count increased. The caller
 * should unref it once it has finished with it.
 */
dom_exception dom_attr_get_owner(struct dom_attr *attr,
		struct dom_element **result)
{
	struct dom_node *a = (struct dom_node *) attr;

	/* If there is an owning element, increase its reference count */
	if (a->parent != NULL)
		dom_node_ref(a->parent);

	*result = (struct dom_element *) a->parent;

	return DOM_NO_ERR;
}

/**
 * Retrieve an attribute's type information
 *
 * \param attr    The attribute to extract type information from
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 *
 * The returned type info will have its reference count increased. The caller
 * should unref it once it has finished with it.
 */
dom_exception dom_attr_get_schema_type_info(struct dom_attr *attr,
		struct dom_type_info **result)
{
	UNUSED(attr);
	UNUSED(result);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Determine if an attribute if of type ID
 *
 * \param attr    The attribute to inspect
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception dom_attr_is_id(struct dom_attr *attr, bool *result)
{
	*result = attr->is_id;

	return DOM_NO_ERR;
}

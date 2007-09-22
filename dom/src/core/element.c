/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <stdio.h>

#include <dom/core/attr.h>
#include <dom/core/element.h>
#include <dom/core/string.h>

#include "core/attr.h"
#include "core/document.h"
#include "core/element.h"
#include "core/node.h"
#include "utils/utils.h"

/**
 * DOM element node
 */
struct dom_element {
	struct dom_node base;		/**< Base node */

	struct dom_type_info *schema_type_info;	/**< Type information */
};

/**
 * Create an element node
 *
 * \param doc     The owning document
 * \param name    The name of the node to create
 * \param result  Pointer to location to receive created element
 * \return DOM_NO_ERR                on success,
 *         DOM_INVALID_CHARACTER_ERR if ::name is invalid,
 *         DOM_NO_MEM_ERR            on memory exhaustion.
 *
 * ::doc and ::name will have their reference counts increased.
 *
 * The returned element will already be referenced.
 */
dom_exception dom_element_create(struct dom_document *doc,
		struct dom_string *name, struct dom_element **result)
{
	struct dom_element *el;
	dom_exception err;

	/** \todo Sanity check the tag name */

	/* Allocate the element */
	el = dom_document_alloc(doc, NULL, sizeof(struct dom_element));
	if (el == NULL)
		return DOM_NO_MEM_ERR;

	/* Initialise the base class */
	err = dom_node_initialise(&el->base, doc, DOM_ELEMENT_NODE,
			name, NULL);
	if (err != DOM_NO_ERR) {
		dom_document_alloc(doc, el, 0);
		return err;
	}

	/* Perform our type-specific initialisation */
	el->schema_type_info = NULL;

	*result = el;

	return DOM_NO_ERR;
}

/**
 * Destroy an element
 *
 * \param doc      The owning document
 * \param element  The element to destroy
 *
 * The contents of ::element will be destroyed and ::element will be freed.
 */
void dom_element_destroy(struct dom_document *doc,
		struct dom_element *element)
{
	struct dom_node *c, *d;

	/* Destroy children of this node */
	for (c = element->base.first_child; c != NULL; c = d) {
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

	/* Destroy attributes attached to this node */
	for (c = (struct dom_node *) element->base.attributes;
			c != NULL; c = d) {
		d = c->next;

		/* Detach child */
		c->parent = NULL;

		if (c->refcnt > 0) {
			/* Something is using this attribute */

			/** \todo add to list of nodes pending deletion */

			continue;
		}

		/* Detach from sibling list */
		c->previous = NULL;
		c->next = NULL;

		dom_node_destroy(c);
	}

	if (element->schema_type_info != NULL) {
		/** \todo destroy schema type info */
	}

	/* Finalise base class */
	dom_node_finalise(doc, &element->base);

	/* Free the element */
	dom_document_alloc(doc, element, 0);
}

/**
 * Retrieve an element's tag name
 *
 * \param element  The element to retrieve the name from
 * \param name     Pointer to location to receive name
 * \return DOM_NO_ERR      on success,
 *         DOM_NO_MEM_ERR  on memory exhaustion.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception dom_element_get_tag_name(struct dom_element *element,
		struct dom_string **name)
{
	struct dom_node *e = (struct dom_node *) element;
	struct dom_string *tag_name;

	if (e->localname != NULL) {
		/* Has a localname, so build a qname string */
		size_t local_len = 0, prefix_len = 0;
		const uint8_t *local = NULL, *prefix = NULL;
		dom_exception err;

		if (e->prefix != NULL)
			dom_string_get_data(e->prefix, &prefix, &prefix_len);

		dom_string_get_data(e->localname, &local, &local_len);

		uint8_t qname[prefix_len + 1 /* : */ + local_len + 1 /* \0 */];

		sprintf((char *) qname, "%s:%s", 
				prefix ? (const char *) prefix : "", 
				(const char *) local);

		err = dom_string_create_from_ptr(e->owner, qname, 
				prefix_len + 1 + local_len, &tag_name);
		if (err != DOM_NO_ERR)
			return err;

		/* tag_name is referenced for us */
	} else {
		tag_name = e->name;

		dom_string_ref(tag_name);
	}

	*name = tag_name;

	return DOM_NO_ERR;
}

/**
 * Retrieve an attribute from an element by name
 *
 * \param element  The element to retrieve attribute from
 * \param name     The attribute's name
 * \param value    Pointer to location to receive attribute's value
 * \return DOM_NO_ERR.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception dom_element_get_attribute(struct dom_element *element,
		struct dom_string *name, struct dom_string **value)
{
	struct dom_node *a = (struct dom_node *) element->base.attributes;

	/* Search attributes, looking for name */
	for (; a != NULL; a = a->next) {
		if (dom_string_cmp(a->name, name) == 0)
			break;
	}

	/* Fill in value */
	if (a == NULL) {
		*value = NULL;
	} else {
		dom_attr_get_value(((struct dom_attr *) a), value);
	}

	return DOM_NO_ERR;
}

/**
 * Set an attribute on an element by name
 *
 * \param element  The element to set attribute on
 * \param name     The attribute's name
 * \param value    The attribute's value
 * \return DOM_NO_ERR                      on success,
 *         DOM_INVALID_CHARACTER_ERR       if ::name is invalid,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly.
 */
dom_exception dom_element_set_attribute(struct dom_element *element,
		struct dom_string *name, struct dom_string *value)
{
	struct dom_node *e = (struct dom_node *) element;
	struct dom_node *a = (struct dom_node *) e->attributes;

	/** \todo validate name */

	/* Ensure element can be written to */
	if (_dom_node_readonly(e))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	/* Search for existing attribute with same name */
	for (; a != NULL; a = a->next) {
		if (dom_string_cmp(a->name, name) == 0)
			break;
	}

	if (a != NULL) {
		/* Found an existing attribute, so replace its value */
		if (a->value != NULL)
			dom_string_unref(a->value);

		if (value != NULL)
			dom_string_ref(value);
		a->value = value;
	} else {
		/* No existing attribute, so create one */
		dom_exception err;
		struct dom_attr *attr;

		err = dom_attr_create(e->owner, name, &attr);
		if (err != DOM_NO_ERR)
			return err;

		/* Set its value */
		err = dom_attr_set_value(attr, value);
		if (err != DOM_NO_ERR) {
			dom_node_unref((struct dom_node *) attr);
			return err;
		}

		a = (struct dom_node *) attr;

		/* And insert it into the element */
		a->previous = NULL;
		a->next = (struct dom_node *) e->attributes;

		if (a->next != NULL)
			a->next->previous = a;

		e->attributes = attr;
	}

	return DOM_NO_ERR;
}

/**
 * Remove an attribute from an element by name
 *
 * \param element  The element to remove attribute from
 * \param name     The name of the attribute to remove
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly.
 */
dom_exception dom_element_remove_attribute(struct dom_element *element,
		struct dom_string *name)
{
	struct dom_node *e = (struct dom_node *) element;
	struct dom_node *a = (struct dom_node *) e->attributes;

	/* Ensure element can be written to */
	if (_dom_node_readonly(e))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	/* Search for existing attribute with same name */
	for (; a != NULL; a = a->next) {
		if (dom_string_cmp(a->name, name) == 0)
			break;
	}

	/* Detach attr node from list */
	if (a != NULL) {
		if (a->previous != NULL)
			a->previous->next = a->next;
		else
			e->attributes = (struct dom_attr *) a->next;

		if (a->next != NULL)
			a->next->previous = a->previous;

		a->previous = a->next = a->parent = NULL;

		/* And destroy attr */
		dom_node_unref(a);
	}

	return DOM_NO_ERR;
}

/**
 * Retrieve an attribute node from an element by name
 *
 * \param element  The element to retrieve attribute node from
 * \param name     The attribute's name
 * \param result   Pointer to location to receive attribute node
 * \return DOM_NO_ERR.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception dom_element_get_attribute_node(struct dom_element *element,
		struct dom_string *name, struct dom_attr **result)
{
	struct dom_node *a = (struct dom_node *) element->base.attributes;

	/* Search attributes, looking for name */
	for (; a != NULL; a = a->next) {
		if (dom_string_cmp(a->name, name) == 0)
			break;
	}

	if (a != NULL)
		dom_node_ref(a);
	*result = (struct dom_attr *) a;

	return DOM_NO_ERR;
}

/**
 * Set an attribute node on an element, replacing existing node, if present
 *
 * \param element  The element to add a node to
 * \param attr     The attribute node to add
 * \param result   Pointer to location to receive previous node
 * \return DOM_NO_ERR                      on success,
 *         DOM_WRONG_DOCUMENT_ERR          if ::attr does not belong to the
 *                                         same document as ::element,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_INUSE_ATTRIBUTE_ERR         if ::attr is already an attribute
 *                                         of another Element node.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception dom_element_set_attribute_node(struct dom_element *element,
		struct dom_attr *attr, struct dom_attr **result)
{
	struct dom_node *e = (struct dom_node *) element;
	struct dom_node *a = (struct dom_node *) attr;
	struct dom_attr *prev = NULL;

	/* Ensure element and attribute belong to the same document */
	if (e->owner != a->owner)
		return DOM_WRONG_DOCUMENT_ERR;

	/* Ensure element can be written to */
	if (_dom_node_readonly(e))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	/* Ensure attribute isn't attached to another element */
	if (a->parent != NULL && a->parent != e)
		return DOM_INUSE_ATTRIBUTE_ERR;

	/* Attach attr to element, if not already attached */
	if (a->parent == NULL) {

		/* Search for existing attribute with same name */
		prev = e->attributes; 
		while (prev != NULL) {
			struct dom_node *p = (struct dom_node *) prev;

			if (dom_string_cmp(a->name, p->name) == 0)
				break;

			prev = (struct dom_attr *) p->next;
		}

		a->parent = e;

		if (prev != NULL) {
			/* Found an existing attribute, so replace it */
			struct dom_node *p = (struct dom_node *) prev;

			a->previous = p->previous;
			a->next = p->next;

			if (a->previous != NULL)
				a->previous->next = a;
			else
				e->attributes = attr;

			if (a->next != NULL)
				a->next->previous = a;

			/* Invalidate existing attribute's location info */
			p->next = NULL;
			p->previous = NULL;
			p->parent = NULL;
		} else {
			/* No existing attribute, so insert at front of list */
			a->previous = NULL;
			a->next = (struct dom_node *) e->attributes;

			if (a->next != NULL)
				a->next->previous = a;

			e->attributes = attr;
		}
	}

	if (prev != NULL)
		dom_node_ref((struct dom_node *) prev);

	*result = prev;

	return DOM_NO_ERR;
}

/**
 * Remove an attribute node from an element
 *
 * \param element  The element to remove attribute node from
 * \param attr     The attribute node to remove
 * \param result   Pointer to location to receive attribute node
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NOT_FOUND_ERR               if ::attr is not an attribute of
 *                                         ::element.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception dom_element_remove_attribute_node(struct dom_element *element,
		struct dom_attr *attr, struct dom_attr **result)
{
	struct dom_node *e = (struct dom_node *) element;
	struct dom_node *a = (struct dom_node *) attr;

	/* Ensure element can be written to */
	if (_dom_node_readonly(e))
		return DOM_NO_MODIFICATION_ALLOWED_ERR;

	/* Ensure attr is an attribute of element */
	if (a->parent != e)
		return DOM_NOT_FOUND_ERR;

	/* Detach attr node from list */
	if (a->previous != NULL)
		a->previous->next = a->next;
	else
		e->attributes = (struct dom_attr *) a->next;

	if (a->next != NULL)
		a->next->previous = a->previous;

	a->previous = a->next = a->parent = NULL;

	/* Return the detached node */
	dom_node_ref(a);
	*result = attr;

	return DOM_NO_ERR;
}

/**
 * Retrieve a list of descendant elements of an element which match a given
 * tag name
 *
 * \param element  The root of the subtree to search
 * \param name     The tag name to match (or "*" for all tags)
 * \param result   Pointer to location to receive result
 * \return DOM_NO_ERR.
 *
 * The returned nodelist will have its reference count increased. It is
 * the responsibility of the caller to unref the nodelist once it has
 * finished with it.
 */
dom_exception dom_element_get_elements_by_tag_name(
		struct dom_element *element, struct dom_string *name,
		struct dom_nodelist **result)
{
	return dom_document_get_nodelist(element->base.owner, 
			(struct dom_node *) element, name, NULL, NULL, result);
}

/**
 * Retrieve an attribute from an element by namespace/localname
 *
 * \param element    The element to retrieve attribute from
 * \param namespace  The attribute's namespace URI
 * \param localname  The attribute's local name
 * \param value      Pointer to location to receive attribute's value
 * \return DOM_NO_ERR            on success,
 *         DOM_NOT_SUPPORTED_ERR if the implementation does not support
 *                               the feature "XML" and the language exposed
 *                               through the Document does not support
 *                               Namespaces.
 *
 * The returned string will have its reference count increased. It is
 * the responsibility of the caller to unref the string once it has
 * finished with it.
 */
dom_exception dom_element_get_attribute_ns(struct dom_element *element,
		struct dom_string *namespace, struct dom_string *localname,
		struct dom_string **value)
{
	UNUSED(element);
	UNUSED(namespace);
	UNUSED(localname);
	UNUSED(value);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Set an attribute on an element by namespace/qualified name
 *
 * \param element    The element to set attribute on
 * \param namespace  The attribute's namespace URI
 * \param qname      The attribute's qualified name
 * \param value      The attribute's value
 * \return DOM_NO_ERR                      on success,
 *         DOM_INVALID_CHARACTER_ERR       if ::qname is invalid,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NAMESPACE_ERR               if ::qname is malformed, or
 *                                         ::qname has a prefix and
 *                                         ::namespace is null, or ::qname
 *                                         has a prefix "xml" and
 *                                         ::namespace is not
 *                                         "http://www.w3.org/XML/1998/namespace",
 *                                         or ::qname has a prefix "xmlns"
 *                                         and ::namespace is not
 *                                         "http://www.w3.org/2000/xmlns",
 *                                         or ::namespace is
 *                                         "http://www.w3.org/2000/xmlns"
 *                                         and ::qname is not prefixed
 *                                         "xmlns",
 *         DOM_NOT_SUPPORTED_ERR           if the implementation does not
 *                                         support the feature "XML" and the
 *                                         language exposed through the
 *                                         Document does not support
 *                                         Namespaces.
 */
dom_exception dom_element_set_attribute_ns(struct dom_element *element,
		struct dom_string *namespace, struct dom_string *qname,
		struct dom_string *value)
{
	UNUSED(element);
	UNUSED(namespace);
	UNUSED(qname);
	UNUSED(value);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Remove an attribute from an element by namespace/localname
 *
 * \param element    The element to remove attribute from
 * \param namespace  The attribute's namespace URI
 * \param localname  The attribute's local name
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NOT_SUPPORTED_ERR           if the implementation does not
 *                                         support the feature "XML" and the
 *                                         language exposed through the
 *                                         Document does not support
 *                                         Namespaces.
 */
dom_exception dom_element_remove_attribute_ns(struct dom_element *element,
		struct dom_string *namespace, struct dom_string *localname)
{
	UNUSED(element);
	UNUSED(namespace);
	UNUSED(localname);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Retrieve an attribute node from an element by namespace/localname
 *
 * \param element    The element to retrieve attribute from
 * \param namespace  The attribute's namespace URI
 * \param localname  The attribute's local name
 * \param result     Pointer to location to receive attribute node
 * \return DOM_NO_ERR            on success,
 *         DOM_NOT_SUPPORTED_ERR if the implementation does not support
 *                               the feature "XML" and the language exposed
 *                               through the Document does not support
 *                               Namespaces.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception dom_element_get_attribute_node_ns(struct dom_element *element,
		struct dom_string *namespace, struct dom_string *localname,
		struct dom_attr **result)
{
	UNUSED(element);
	UNUSED(namespace);
	UNUSED(localname);
	UNUSED(result);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Set an attribute node on an element, replacing existing node, if present
 *
 * \param element  The element to add a node to
 * \param attr     The attribute node to add
 * \param result   Pointer to location to recieve previous node
 * \return DOM_NO_ERR                      on success,
 *         DOM_WRONG_DOCUMENT_ERR          if ::attr does not belong to the
 *                                         same document as ::element,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_INUSE_ATTRIBUTE_ERR         if ::attr is already an attribute
 *                                         of another Element node.
 *         DOM_NOT_SUPPORTED_ERR           if the implementation does not
 *                                         support the feature "XML" and the
 *                                         language exposed through the
 *                                         Document does not support
 *                                         Namespaces.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception dom_element_set_attribute_node_ns(struct dom_element *element,
		struct dom_attr *attr, struct dom_attr **result)
{
	UNUSED(element);
	UNUSED(attr);
	UNUSED(result);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Retrieve a list of descendant elements of an element which match a given
 * namespace/localname pair.
 *
 * \param element  The root of the subtree to search
 * \param namespace  The namespace URI to match (or "*" for all)
 * \param localname  The local name to match (or "*" for all)
 * \param result   Pointer to location to receive result
 * \return DOM_NO_ERR            on success,
 *         DOM_NOT_SUPPORTED_ERR if the implementation does not support
 *                               the feature "XML" and the language exposed
 *                               through the Document does not support
 *                               Namespaces.
 *
 * The returned nodelist will have its reference count increased. It is
 * the responsibility of the caller to unref the nodelist once it has
 * finished with it.
 */
dom_exception dom_element_get_elements_by_tag_name_ns(
		struct dom_element *element, struct dom_string *namespace,
		struct dom_string *localname, struct dom_nodelist **result)
{
	return dom_document_get_nodelist(element->base.owner, 
			(struct dom_node *) element, NULL, 
			namespace, localname, result);
}

/**
 * Determine if an element possesses and attribute with the given name
 *
 * \param element  The element to query
 * \param name     The attribute name to look for
 * \param result   Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception dom_element_has_attribute(struct dom_element *element,
		struct dom_string *name, bool *result)
{
	struct dom_node *a = (struct dom_node *) element->base.attributes;

	/* Search attributes, looking for name */
	for (; a != NULL; a = a->next) {
		if (dom_string_cmp(a->name, name) == 0)
			break;
	}

	*result = (a != NULL);

	return DOM_NO_ERR;
}

/**
 * Determine if an element possesses and attribute with the given
 * namespace/localname pair.
 *
 * \param element    The element to query
 * \param namespace  The attribute namespace URI to look for
 * \param localname  The attribute local name to look for
 * \param result   Pointer to location to receive result
 * \return DOM_NO_ERR            on success,
 *         DOM_NOT_SUPPORTED_ERR if the implementation does not support
 *                               the feature "XML" and the language exposed
 *                               through the Document does not support
 *                               Namespaces.
 */
dom_exception dom_element_has_attribute_ns(struct dom_element *element,
		struct dom_string *namespace, struct dom_string *localname,
		bool *result)
{
	UNUSED(element);
	UNUSED(namespace);
	UNUSED(localname);
	UNUSED(result);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Retrieve the type information associated with an element
 *
 * \param element  The element to retrieve type information from
 * \param result   Pointer to location to receive type information
 * \return DOM_NO_ERR.
 *
 * The returned typeinfo will have its reference count increased. It is
 * the responsibility of the caller to unref the typeinfo once it has
 * finished with it.
 */
dom_exception dom_element_get_schema_type_info(struct dom_element *element,
		struct dom_type_info **result)
{
	UNUSED(element);
	UNUSED(result);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * (Un)declare an attribute as being an element's ID by name
 *
 * \param element  The element containing the attribute
 * \param name     The attribute's name
 * \param is_id    Whether the attribute is an ID
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NOT_FOUND_ERR               if the specified node is not an
 *                                         attribute of ::element.
 */
dom_exception dom_element_set_id_attribute(struct dom_element *element,
		struct dom_string *name, bool is_id)
{
	UNUSED(element);
	UNUSED(name);
	UNUSED(is_id);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * (Un)declare an attribute as being an element's ID by namespace/localname
 *
 * \param element    The element containing the attribute
 * \param namespace  The attribute's namespace URI
 * \param localname  The attribute's local name
 * \param is_id      Whether the attribute is an ID
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NOT_FOUND_ERR               if the specified node is not an
 *                                         attribute of ::element.
 */
dom_exception dom_element_set_id_attribute_ns(struct dom_element *element,
		struct dom_string *namespace, struct dom_string *localname,
		bool is_id)
{
	UNUSED(element);
	UNUSED(namespace);
	UNUSED(localname);
	UNUSED(is_id);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * (Un)declare an attribute node as being an element's ID
 *
 * \param element  The element containing the attribute
 * \param id_attr  The attribute node
 * \param is_id    Whether the attribute is an ID
 * \return DOM_NO_ERR                      on success,
 *         DOM_NO_MODIFICATION_ALLOWED_ERR if ::element is readonly,
 *         DOM_NOT_FOUND_ERR               if the specified node is not an
 *                                         attribute of ::element.
 */
dom_exception dom_element_set_id_attribute_node(struct dom_element *element,
		struct dom_attr *id_attr, bool is_id)
{
	UNUSED(element);
	UNUSED(id_attr);
	UNUSED(is_id);

	return DOM_NOT_SUPPORTED_ERR;
}


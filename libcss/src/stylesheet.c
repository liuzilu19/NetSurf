/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <string.h>

#include "stylesheet.h"
#include "utils/utils.h"

/**
 * Create a selector
 *
 * \param sheet  The stylesheet context
 * \param type   The type of selector to create
 * \param name   Name of selector
 * \param value  Value of selector, or NULL
 * \return Pointer to selector object, or NULL on failure
 */
css_selector *css_stylesheet_selector_create(css_stylesheet *sheet,
		css_selector_type type, css_string *name, css_string *value)
{
	css_selector *sel;

	if (sheet == NULL || name == NULL)
		return NULL;

	sel = sheet->alloc(NULL, sizeof(css_selector), sheet->pw);
	if (sel == NULL)
		return NULL;

	memset(sel, 0, sizeof(css_selector));

	sel->type = type;
	sel->data.name = *name;
	if (value != NULL) {
		sel->data.value = *value;
	}
	/** \todo specificity */
	sel->specificity = 0;
	sel->combinator_type = CSS_COMBINATOR_NONE;

	return sel;
}

/**
 * Destroy a selector object
 *
 * \param sheet     The stylesheet context
 * \param selector  The selector to destroy
 */
void css_stylesheet_selector_destroy(css_stylesheet *sheet,
		css_selector *selector)
{
	UNUSED(sheet);
	UNUSED(selector);

	/** \todo Need to ensure that selector is removed from whatever it's 
	 * attached to (be that the parent selector, parent rule, or the 
	 * hashtable of selectors (or any combination of these) */
}

/**
 * Append a selector to the specifics chain of another selector
 *
 * \param sheet     The stylesheet context
 * \param parent    The parent selector
 * \param specific  The selector to append
 * \return CSS_OK on success, appropriate error otherwise.
 */
css_error css_stylesheet_selector_append_specific(css_stylesheet *sheet,
		css_selector *parent, css_selector *specific)
{
	css_selector *s;

	if (sheet == NULL || parent == NULL || specific == NULL)
		return CSS_BADPARM;

	/** \todo this may want optimising */
	for (s = parent->specifics; s != NULL && s->next != NULL; s = s->next)
		/* do nothing */;
	if (s == NULL) {
		specific->prev = specific->next = NULL;
		parent->specifics = specific;
	} else {
		s->next = specific;
		specific->prev = s;
		specific->next = NULL;
	}

	return CSS_OK;
}

/**
 * Combine a pair of selectors
 *
 * \param sheet  The stylesheet context
 * \param type   The combinator type
 * \param a      The first operand
 * \param b      The second operand
 * \return CSS_OK on success, appropriate error otherwise.
 *
 * For example, given A + B, the combinator field of B would point at A, 
 * with a combinator type of CSS_COMBINATOR_SIBLING. Thus, given B, we can
 * find its combinator. It is not possible to find B given A.
 *
 * \todo Check that this (backwards) representation plays well with CSSOM.
 */
css_error css_stylesheet_selector_combine(css_stylesheet *sheet,
		css_combinator type, css_selector *a, css_selector *b)
{
	if (sheet == NULL || a == NULL || b == NULL)
		return CSS_BADPARM;

	/* Ensure that there is no existing combinator on B */
	if (b->combinator != NULL)
		return CSS_INVALID;

	b->combinator = a;
	b->combinator_type = type;

	return CSS_OK;
}

/**
 * Create a CSS rule
 *
 * \param sheet  The stylesheet context
 * \param type   The rule type
 * \return Pointer to rule object, or NULL on failure.
 */
css_rule *css_stylesheet_rule_create(css_stylesheet *sheet, css_rule_type type)
{
	css_rule *rule;

	if (sheet == NULL)
		return NULL;

	rule = sheet->alloc(NULL, sizeof(css_rule), sheet->pw);
	if (rule == NULL)
		return NULL;

	memset(rule, 0, sizeof(css_rule));

	rule->type = type;

	return rule;
}

/**
 * Destroy a CSS rule
 *
 * \param sheet  The stylesheet context
 * \param rule   The rule to destroy
 */
void css_stylesheet_rule_destroy(css_stylesheet *sheet, css_rule *rule)
{
	UNUSED(sheet);
	UNUSED(rule);

	/** \todo should this be recursive? */
	/** \todo what happens to non-rule objects owned by this rule? */
}

/**
 * Add a selector to a CSS rule
 *
 * \param sheet     The stylesheet context
 * \param rule      The rule to add to (must be of type CSS_RULE_SELECTOR)
 * \param selector  The selector to add
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css_stylesheet_rule_add_selector(css_stylesheet *sheet, 
		css_rule *rule, css_selector *selector)
{
	css_selector **sels;

	if (sheet == NULL || rule == NULL || selector == NULL)
		return CSS_BADPARM;

	/* Ensure rule is a CSS_RULE_SELECTOR */
	if (rule->type != CSS_RULE_SELECTOR)
		return CSS_INVALID;

	sels = sheet->alloc(rule->data.selector.selectors, 
			(rule->data.selector.selector_count + 1) * 
				sizeof(css_selector *), 
			sheet->pw);
	if (sels == NULL)
		return CSS_NOMEM;

	/* Insert into rule's selector list */
	sels[rule->data.selector.selector_count] = selector;
	rule->data.selector.selector_count++;
	rule->data.selector.selectors = sels;

	/* Set selector's rule field */
	selector->rule = rule;
	
	return CSS_OK;
}

/**
 * Add a rule to a stylesheet
 *
 * \param sheet  The stylesheet to add to
 * \param rule   The rule to add
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css_stylesheet_add_rule(css_stylesheet *sheet, css_rule *rule)
{
	css_rule *r;

	if (sheet == NULL || rule == NULL)
		return CSS_BADPARM;

	/* Fill in rule's index and owner fields */
	rule->index = sheet->rule_count;
	rule->owner = sheet;

	/* Add rule to sheet */
	sheet->rule_count++;
	/** \todo this may need optimising */
	for (r = sheet->rule_list; r != NULL && r->next != NULL; r = r->next)
		/* do nothing */;
	if (r == NULL) {
		rule->prev = rule->next = NULL;
		sheet->rule_list = rule;
	} else {
		r->next = rule;
		rule->prev = r;
		rule->next = NULL;
	}

	/** \todo If there are selectors in the rule, add them to the hash 
	 * (this needs to recurse over child rules, too) */

	return CSS_OK;
}

/**
 * Remove a rule from a stylesheet
 *
 * \param sheet  The sheet to remove from
 * \param rule   The rule to remove
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error css_stylesheet_remove_rule(css_stylesheet *sheet, css_rule *rule)
{
	UNUSED(sheet);
	UNUSED(rule);

	/** \todo If there are selectors (recurse over child rules, too),
	 * then they must be removed from the hash */
	/**\ todo renumber subsequent rules? may not be necessary, as there's 
	 * only an expectation that rules which occur later in the stylesheet 
	 * have a higher index than those that appear earlier. There's no 
	 * guarantee that the number space is continuous. */

	return CSS_OK;
}



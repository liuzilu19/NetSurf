/*
 * Copyright 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/exec.h>
#include "amiga/tree.h"
#include "amiga/sslcert.h"

void gui_cert_verify(const char *url, 
		const struct ssl_cert_info *certs, unsigned long num,
		nserror (*cb)(bool proceed, void *pw), void *cbpw)
{
	struct sslcert_session_data *data;
	struct treeview_window *ssl_window;

	data = sslcert_create_session_data(num, url, cb, cbpw);

	ssl_window = ami_tree_create(sslcert_get_tree_flags(), data);
	if(!ssl_window) return;

	sslcert_load_tree(ami_tree_get_tree(ssl_window), certs, data);

	ami_tree_open(ssl_window, AMI_TREE_SSLCERT);
}

void ami_ssl_free(struct treeview_window *twin)
{
	ami_tree_destroy(twin);
}

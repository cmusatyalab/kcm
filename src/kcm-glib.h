/*
 *  Kimberley Display Control Manager
 *
 *  Copyright (c) 2007-2008 Carnegie Mellon University
 *  All rights reserved.
 *
 *  Kimberley is free software: you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation.
 *
 *  Kimberley is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Kimberley. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _KCM_GLIB_H_
#define _KCM_GLIB_H_

typedef struct {
  GObject base;
} KCM;

typedef struct {
  GObjectClass base;
  DBusGConnection *conn;
} KCMClass;

static void kcm_init        (KCM *server);
static void kcm_class_init  (KCMClass *class);
GType       kcm_get_type    (void);

gboolean    kcm_sense       (KCM *server, 
			     gchar ***interfaces, 
			     GError **error);

gboolean    kcm_browse      (KCM *server, 
			     gchar *service_name,			     
			     gint interface, 
			     guint *port, 
			     GError **error);

gboolean    kcm_publish     (KCM *server, 
			     gchar *service_name,
			     gint interface,
			     guint port, 
			     GError **error);

#endif /*_KCM_GLIB_H_ */

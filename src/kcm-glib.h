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
			     gchar **interfaces, 
			     guint *port, 
			     GError **error);

gboolean    kcm_publish     (KCM *server, 
			     gchar *service_name,
			     gchar **interfaces, 
			     guint port, 
			     GError **error);

#endif /*_KCM_GLIB_H_ */

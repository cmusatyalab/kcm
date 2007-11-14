#ifndef _DCM_GLIB_H_
#define _DCM_GLIB_H_

typedef struct {
  GObject base;
} DCM;

typedef struct {
  GObjectClass base;
  DBusGConnection *conn;
} DCMClass;

static void dcm_init        (DCM *server);
static void dcm_class_init  (DCMClass *class);
GType       dcm_get_type    (void);
gboolean    dcm_client      (DCM *server, guint *port, GError **error);
gboolean    dcm_server      (DCM *server, guint port, GError **error);


#endif /*_DCM_GLIB_H_ */

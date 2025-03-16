#ifndef ALTERATOR_MODULE_DBDCONF_MODULE
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <errno.h>
#include <toml.h>
#include <alterator/alterator_manager_module_info.h>
#define ALTERATOR_MODULE_DBDCONF_MODULE

#define PLUGIN_NAME "dbdconf"

G_BEGIN_DECLS

typedef struct Dbdconf_Args_t {
    GDBusConnection *connection;
    GDBusMethodInvocation *invocation;
    const gchar *method_name;
    const gchar *gvdb_file;
    const gchar *path_or_dir;
} Dbdconf_Args;

gboolean alterator_module_init(AlteratorManagerInterface *interface);
gboolean module_init(const ManagerData *manager_data);
gint module_interface_version();
void module_destroy();

G_END_DECLS

#endif //ALTERATOR_MODULE_DBDCONF_MODULE

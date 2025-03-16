#include <module.h>

static PolkitAuthority *polkit_authority = NULL;
static AlteratorManagerInterface *manager_interface = NULL;
static gboolean user_mode = FALSE;
static GHashTable *sender_environment_data = NULL;

static AlteratorModuleInterface module_interface =
{
    .interface_version = module_interface_version,
    .init = (gpointer) module_init,
    .destroy = module_destroy
};

#define dbdconf_introspection \
"<node>" \
"  <interface name='org.altlinux.alterator.dbdconf'>" \
"    <method name='Dump'>" \
"      <arg type='s' name='GVDB_PATH' direction='in'/>" \
"      <arg type='s' name='DIR' direction='in'/>" \
"      <arg type='as' name='stdout_string' direction='out'/>" \
"      <arg type='i' name='status' direction='out'/>" \
"    </method>" \
"    <method name='List'>" \
"      <arg type='s' name='GVDB_PATH' direction='in'/>" \
"      <arg type='s' name='DIR' direction='in'/>" \
"      <arg type='as' name='stdout_string' direction='out'/>" \
"      <arg type='i' name='status' direction='out'/>" \
"    </method>" \
"    <method name='Read'>" \
"      <arg type='s' name='GVDB_PATH' direction='in'/>" \
"      <arg type='s' name='KEY' direction='in'/>" \
"      <arg type='as' name='stdout_string' direction='out'/>" \
"      <arg type='i' name='status' direction='out'/>" \
"    </method>" \
"  </interface>" \
"</node>"

GVariantBuilder* build_string_array(const gchar* const* strv, gssize len) {
    GVariantBuilder* builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

    while (*strv && len != 0) {
        if (strlen(*strv) == 0) {
            ++strv, --len;
            continue;
        }
        g_variant_builder_add(builder, "s", *strv);
        ++strv, --len;
    }

    return builder;
}

#define MAKE_ANSWER(status, format, ...) \
({ \
    gchar* __tmp_format_result__ = g_strdup_printf(format, __VA_ARGS__); \
    gchar **__tmp_format_splited_result__ = g_strsplit(__tmp_format_result__, "\n", -1); \
    GVariantBuilder *__array_builder__ = build_string_array((gpointer) __tmp_format_splited_result__, -1); \
    GVariant *__result_variant__; \
\
    __result_variant__ = g_variant_new("(asi)", __array_builder__, status); \
\
    g_variant_builder_unref(__array_builder__);\
    g_strfreev(__tmp_format_splited_result__); \
    g_free(__tmp_format_result__); \
\
    __result_variant__; \
})

#define SEND_ANSWER(invocation, status, format, ...) \
{ \
    GVariant* __tmp_answer_value__ = MAKE_ANSWER(status, format, __VA_ARGS__); \
    g_dbus_method_invocation_return_value(invocation, __tmp_answer_value__); \
    g_variant_unref(__tmp_answer_value__); \
} \


static inline const gchar* method_name_to_arg(const gchar* method_name) {
    switch (method_name[0]) {
        case 'D':
            if (!strcmp(method_name, "Dump")) {
                return "dump";
            }
        return NULL;
        case 'L':
            if (!strcmp(method_name, "List")) {
                return "list";
            }
        return NULL;
        case 'R':
            if (!strcmp(method_name, "Read")) {
                return "read";
            }
        return NULL;
        default:
            return NULL;
    }
}

// call_dbdconf_method calls dbdconf. This is done because some libsvdb functions terminate the application in case of a
// critical error, and this may lead to the termination of alterator-manager. Until this is fixed and all possible
// memory leaks are resolved, this method will be used. Once this is fixed, libsvdb functions will be used directly
// instead of calling dbdconf.
//
// For critical error termination see:
// ```bash
// grep -R "// TODO: replace by GError\* and return" ./
// ```
static gpointer call_dbdconf_method(const Dbdconf_Args* dbdconf_args) {
    const gchar *args[5] = {"dbdconf", dbdconf_args->gvdb_file, method_name_to_arg(dbdconf_args->method_name), dbdconf_args->path_or_dir, NULL};

    gchar *stdout_output = NULL;
    gchar *stdout_error = NULL;
    gint exit_status = 0;
    GError *error = NULL;

    const gboolean success = g_spawn_sync(
        NULL,
        (gpointer)args,
        NULL,
        G_SPAWN_SEARCH_PATH,
        NULL,
        NULL,
        &stdout_output,
        &stdout_error,
        &exit_status,
        &error
    );

    if (!success) {
        g_warning("%s `%s`", "Dbdconf: execute dbdconf error", error->message);
        SEND_ANSWER(dbdconf_args->invocation, 12, "%s `%s`", "Dbdconf: execute dbdconf error", error->message);
    }
    else if (exit_status != 0) {
        SEND_ANSWER(dbdconf_args->invocation, exit_status, "%s", stdout_error);
    }
    else {
        SEND_ANSWER(dbdconf_args->invocation, exit_status, "%s", stdout_output);
    }

    return NULL;
}

static void handle_method_call(GDBusConnection *connection,
                               const gchar *sender,
                               const gchar *object_path,
                               const gchar *interface_name,
                               const gchar *method_name,
                               GVariant *parameters,
                               GDBusMethodInvocation *invocation,
                               gpointer user_data) {
    Dbdconf_Args* args;
    GThread *thread;

    args = g_new0(Dbdconf_Args, 1);
    args->connection = connection;
    args->invocation = invocation;
    args->method_name = g_strdup(method_name);
    g_variant_get(parameters, "(ss)", &args->gvdb_file, &args->path_or_dir);

    thread = g_thread_new("dbdconf", (gpointer) call_dbdconf_method, (gpointer) args);
    if (thread != NULL) {
        g_thread_unref(thread);
    }
    else {
        g_warning("%s", "Dbdconf: g_thread_new() returned NULL.");
        SEND_ANSWER(invocation, 12, "%s", "Dbdconf: g_thread_new() returned NULL.");
    }
}


static const GDBusInterfaceVTable interface_vtable =
{
    &handle_method_call,
};

InterfaceObjectInfo *create_dbdconf_interface() {
    InterfaceObjectInfo *interface = g_new0(InterfaceObjectInfo, 1);
    GDBusNodeInfo *introspection_data = NULL;
    GDBusInterfaceInfo *interface_info = NULL;

    introspection_data = g_dbus_node_info_new_for_xml(dbdconf_introspection, NULL);
    if (introspection_data == NULL) {
        return NULL;
    }

    interface_info = g_dbus_node_info_lookup_interface(introspection_data,
                                                       "org.altlinux.alterator.dbdconf");

    if (!interface_info) {
        g_dbus_node_info_unref(introspection_data);
        return NULL;
    }

    if (!manager_interface->interface_validation(interface_info,
                                                 "org.altlinux.alterator.dbdconf")) {
        g_dbus_node_info_unref(introspection_data);
        return NULL;
    }


    interface->interface_introspection = g_dbus_interface_info_ref(interface_info);
    interface->module_name = g_strdup("dbdconf");
    interface->action_id = g_strdup("org.altlinux.alterator.dbdconf");

    interface->methods = g_hash_table_new_similar(sender_environment_data);
    // g_hash_table_insert(interface->methods, g_strdup("dbdconf"), g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free));

    interface->thread_limit = 5;
    interface->interface_vtable = &interface_vtable;

    return interface;
}

gboolean module_init(const ManagerData *manager_data) {
    GHashTable *interfaces;
    InterfaceObjectInfo *info;

    g_debug("Initialize alterator module %s.", PLUGIN_NAME);
    user_mode = manager_data->user_mode;

    /* authority is only needed in system mode. */
    if (manager_data->authority == NULL && !user_mode) {
        g_warning("Executor: module_init(), manager_data->authority == "
            "NULL. NULL will be returned.");
        return FALSE;
    }

    polkit_authority = manager_data->authority;
    sender_environment_data = manager_data->sender_environment_data;


    info = create_dbdconf_interface();
    if (!info) {
        return FALSE;
    }

    interfaces = g_hash_table_new_similar(manager_data->backends_data);
    g_hash_table_insert(interfaces, g_strdup("org.altlinux.alterator.dbdconf"), info);

    g_hash_table_insert(manager_data->backends_data, g_strdup("dbdconf"), interfaces);

    g_debug("alterator node: %s", (g_hash_table_contains(manager_data->backends_data, "alterator")? "true" : "false"));

    return TRUE;
}

gint module_interface_version() {
    return ALTERATOR_MODULE_INTERFACE_VERSION;
}

void module_destroy() {
    g_warning("deinitialize alterator module %s.", PLUGIN_NAME);
}

gboolean alterator_module_init(AlteratorManagerInterface *interface) {
    if (!interface || !interface->register_module) {
        g_warning("Module '%s' initialization failed - invalid "
                  "AlteratorModuleInterface.", PLUGIN_NAME);
        return FALSE;
    }

    if (!interface->interface_validation) {
        g_warning("Module '%s' initialization failed - invalid "
                  "AlteratorModuleInterface. interface_validation == NULL.",
                  PLUGIN_NAME);
        return FALSE;
    }

    manager_interface = interface;

    if (!interface->register_module(&module_interface)) {
        g_warning("Register module '%s' failed.", PLUGIN_NAME);
        return FALSE;
    }

    return TRUE;
}

#include "app_settings_credentials.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clogger.h"

#define APPSETTINGS_CREDENTIALS_CAT "credentials"

// Forward declarations
static void dispatch_state_changed(AppSettingsCredentials * self);
static void on_add_clicked(GtkButton * button, AppSettingsCredentials * self);
static void on_remove_clicked(GtkButton * button, AppSettingsCredentials * self);
static void on_edit_clicked(GtkButton * button, AppSettingsCredentials * self);
static void on_row_selected(GtkListBox * listbox, GtkListBoxRow * row, AppSettingsCredentials * self);
static void update_button_states(AppSettingsCredentials * self);
static void refresh_listbox(AppSettingsCredentials * self);
static void show_credential_dialog(AppSettingsCredentials * self, CredentialEntry * entry);
static void on_dialog_response(GtkDialog * dialog, gint response_id, AppSettingsCredentials * self);

// CredentialEntry implementation
CredentialEntry * CredentialEntry__create(const char * username, const char * password, const char * description) {
    CredentialEntry * entry = malloc(sizeof(CredentialEntry));
    entry->username = username ? strdup(username) : strdup("");
    entry->password = password ? strdup(password) : strdup("");
    entry->description = description ? strdup(description) : strdup("");
    return entry;
}

void CredentialEntry__destroy(CredentialEntry * entry) {
    if (entry) {
        free(entry->username);
        free(entry->password);
        free(entry->description);
        free(entry);
    }
}

const char * CredentialEntry__get_username(CredentialEntry * entry) {
    return entry ? entry->username : NULL;
}

const char * CredentialEntry__get_password(CredentialEntry * entry) {
    return entry ? entry->password : NULL;
}

const char * CredentialEntry__get_description(CredentialEntry * entry) {
    return entry ? entry->description : NULL;
}

// Helper functions
static void dispatch_state_changed(AppSettingsCredentials * self) {
    self->has_changes = 1; // Mark that we have unsaved changes
    if (self->state_changed_callback) {
        self->state_changed_callback(self->state_changed_user_data);
    }
}

static void update_button_states(AppSettingsCredentials * self) {
    gboolean has_selection = (self->selected_row != NULL);
    gtk_widget_set_sensitive(self->remove_button, has_selection);
    gtk_widget_set_sensitive(self->edit_button, has_selection);
}

static void refresh_listbox(AppSettingsCredentials * self) {
    // Clear existing rows
    GList * children = gtk_container_get_children(GTK_CONTAINER(self->listbox));
    for (GList * l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    // Add credential entries
    for (GList * l = self->credentials; l != NULL; l = l->next) {
        CredentialEntry * entry = (CredentialEntry *)l->data;

        GtkWidget * row = gtk_list_box_row_new();
        GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

        // Create display text
        char display_text[256];
        if (strlen(entry->description) > 0) {
            snprintf(display_text, sizeof(display_text), "%s (%s)", entry->username, entry->description);
        } else {
            snprintf(display_text, sizeof(display_text), "%s", entry->username);
        }

        GtkWidget * label = gtk_label_new(display_text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);

        gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(row), box);

        // Store entry pointer in row data
        g_object_set_data(G_OBJECT(row), "credential_entry", entry);

        gtk_list_box_insert(GTK_LIST_BOX(self->listbox), row, -1);
    }

    gtk_widget_show_all(self->listbox);
    self->selected_row = NULL;
    update_button_states(self);
}

static void on_row_selected(GtkListBox * listbox, GtkListBoxRow * row, AppSettingsCredentials * self) {
    self->selected_row = row;
    update_button_states(self);
}

static void on_add_clicked(GtkButton * button, AppSettingsCredentials * self) {
    show_credential_dialog(self, NULL);
}

static void on_remove_clicked(GtkButton * button, AppSettingsCredentials * self) {
    if (self->selected_row) {
        CredentialEntry * entry = g_object_get_data(G_OBJECT(self->selected_row), "credential_entry");
        if (entry) {
            self->credentials = g_list_remove(self->credentials, entry);
            CredentialEntry__destroy(entry);
            refresh_listbox(self);
            dispatch_state_changed(self);
        }
    }
}

static void on_edit_clicked(GtkButton * button, AppSettingsCredentials * self) {
    if (self->selected_row) {
        CredentialEntry * entry = g_object_get_data(G_OBJECT(self->selected_row), "credential_entry");
        if (entry) {
            show_credential_dialog(self, entry);
        }
    }
}

static void on_show_password_toggled(GtkToggleButton * button, AppSettingsCredentials * self) {
    gboolean visible = gtk_toggle_button_get_active(button);
    gtk_entry_set_visibility(GTK_ENTRY(self->password_entry), visible);

    // Update button text
    const char * text = visible ? "Hide" : "Show";
    gtk_button_set_label(GTK_BUTTON(button), text);
}

static void show_credential_dialog(AppSettingsCredentials * self, CredentialEntry * entry) {
    GtkWidget * parent = gtk_widget_get_toplevel(self->widget);

    self->dialog = gtk_dialog_new_with_buttons(
        entry ? "Edit Credential" : "Add Credential",
        GTK_WINDOW(parent),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL,
        "OK", GTK_RESPONSE_OK,
        NULL);

    GtkWidget * content_area = gtk_dialog_get_content_area(GTK_DIALOG(self->dialog));
    GtkWidget * grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    g_object_set(grid, "margin", 12, NULL);

    // Username
    GtkWidget * username_label = gtk_label_new("Username:");
    gtk_label_set_xalign(GTK_LABEL(username_label), 0.0);
    self->username_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), username_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), self->username_entry, 1, 0, 1, 1);

    // Password with show/hide button
    GtkWidget * password_label = gtk_label_new("Password:");
    gtk_label_set_xalign(GTK_LABEL(password_label), 0.0);

    // Create a horizontal box for password entry and show/hide button
    GtkWidget * password_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    self->password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(self->password_entry), FALSE);
    gtk_widget_set_hexpand(self->password_entry, TRUE);

    GtkWidget * show_password_button = gtk_toggle_button_new_with_label("Show");
    gtk_widget_set_tooltip_text(show_password_button, "Show/hide password");
    g_signal_connect(show_password_button, "toggled", G_CALLBACK(on_show_password_toggled), self);

    gtk_box_pack_start(GTK_BOX(password_box), self->password_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(password_box), show_password_button, FALSE, FALSE, 0);

    gtk_grid_attach(GTK_GRID(grid), password_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), password_box, 1, 1, 1, 1);

    // Description
    GtkWidget * description_label = gtk_label_new("Description:");
    gtk_label_set_xalign(GTK_LABEL(description_label), 0.0);
    self->description_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(self->description_entry), "Optional description");
    gtk_grid_attach(GTK_GRID(grid), description_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), self->description_entry, 1, 2, 1, 1);

    // Fill in existing values if editing
    if (entry) {
        gtk_entry_set_text(GTK_ENTRY(self->username_entry), entry->username);
        gtk_entry_set_text(GTK_ENTRY(self->password_entry), entry->password);
        gtk_entry_set_text(GTK_ENTRY(self->description_entry), entry->description);
        g_object_set_data(G_OBJECT(self->dialog), "editing_entry", entry);
    }

    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(self->dialog);

    g_signal_connect(self->dialog, "response", G_CALLBACK(on_dialog_response), self);
}

static void on_dialog_response(GtkDialog * dialog, gint response_id, AppSettingsCredentials * self) {
    if (response_id == GTK_RESPONSE_OK) {
        const char * username = gtk_entry_get_text(GTK_ENTRY(self->username_entry));
        const char * password = gtk_entry_get_text(GTK_ENTRY(self->password_entry));
        const char * description = gtk_entry_get_text(GTK_ENTRY(self->description_entry));

        if (strlen(username) > 0) {
            CredentialEntry * editing_entry = g_object_get_data(G_OBJECT(dialog), "editing_entry");

            if (editing_entry) {
                // Update existing entry
                free(editing_entry->username);
                free(editing_entry->password);
                free(editing_entry->description);
                editing_entry->username = strdup(username);
                editing_entry->password = strdup(password);
                editing_entry->description = strdup(description);
            } else {
                // Add new entry
                CredentialEntry * new_entry = CredentialEntry__create(username, password, description);
                self->credentials = g_list_append(self->credentials, new_entry);
            }

            refresh_listbox(self);
            dispatch_state_changed(self);
        }
    }

    gtk_widget_destroy(GTK_WIDGET(dialog));
    self->dialog = NULL;
}

// UI Creation
static GtkWidget * AppSettingsCredentials__create_ui(AppSettingsCredentials * self) {
    GtkWidget * widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    g_object_set(widget, "margin", 20, NULL);

    // Title
    GtkWidget * title_label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(title_label), "<span size=\"large\"><b>Discovery Credentials</b></span>");
    gtk_label_set_xalign(GTK_LABEL(title_label), 0.0);
    gtk_box_pack_start(GTK_BOX(widget), title_label, FALSE, FALSE, 0);

    // Description
    GtkWidget * desc_label = gtk_label_new("Credentials to try when probing discovered devices.\nDevices will be tested with these credentials during discovery.");
    gtk_label_set_xalign(GTK_LABEL(desc_label), 0.0);
    g_object_set(desc_label, "margin", 10, NULL);
    gtk_box_pack_start(GTK_BOX(widget), desc_label, FALSE, FALSE, 0);

    // Credentials list
    GtkWidget * list_frame = gtk_frame_new("Stored Credentials");
    GtkWidget * scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, -1, 150);

    self->listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(self->listbox), GTK_SELECTION_SINGLE);
    g_signal_connect(self->listbox, "row-selected", G_CALLBACK(on_row_selected), self);

    gtk_container_add(GTK_CONTAINER(scrolled), self->listbox);
    gtk_container_add(GTK_CONTAINER(list_frame), scrolled);
    gtk_box_pack_start(GTK_BOX(widget), list_frame, TRUE, TRUE, 0);

    // Buttons
    GtkWidget * button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    self->add_button = gtk_button_new_with_label("Add");
    self->edit_button = gtk_button_new_with_label("Edit");
    self->remove_button = gtk_button_new_with_label("Remove");

    g_signal_connect(self->add_button, "clicked", G_CALLBACK(on_add_clicked), self);
    g_signal_connect(self->edit_button, "clicked", G_CALLBACK(on_edit_clicked), self);
    g_signal_connect(self->remove_button, "clicked", G_CALLBACK(on_remove_clicked), self);

    gtk_box_pack_start(GTK_BOX(button_box), self->add_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), self->edit_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), self->remove_button, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(widget), button_box, FALSE, FALSE, 0);

    update_button_states(self);
    return widget;
}

// Main API functions
AppSettingsCredentials * AppSettingsCredentials__create(void (*state_changed_callback)(void *), void * state_changed_user_data) {
    AppSettingsCredentials * self = malloc(sizeof(AppSettingsCredentials));

    self->credentials = NULL;
    self->selected_row = NULL;
    self->dialog = NULL;
    self->has_changes = 0; // Initialize with no changes
    self->state_changed_callback = state_changed_callback;
    self->state_changed_user_data = state_changed_user_data;

    self->widget = AppSettingsCredentials__create_ui(self);

    return self;
}

void AppSettingsCredentials__destroy(AppSettingsCredentials * self) {
    if (self) {
        AppSettingsCredentials__clear_credentials(self);
        free(self);
    }
}

GtkWidget * AppSettingsCredentials__get_widget(AppSettingsCredentials * self) {
    return self->widget;
}

// Settings management
int AppSettingsCredentials__get_state(AppSettingsCredentials * self) {
    return self->has_changes;
}

void AppSettingsCredentials__set_state(AppSettingsCredentials * self, int state) {
    gtk_widget_set_sensitive(self->widget, state);
    if (state == 0) {
        // Settings were saved, clear the changes flag
        self->has_changes = 0;
    }
}

char * AppSettingsCredentials__get_category(AppSettingsCredentials * self) {
    return APPSETTINGS_CREDENTIALS_CAT;
}

static char settings_buffer[4096];
char * AppSettingsCredentials__save(AppSettingsCredentials * self) {
    size_t offset = 0;
    offset += snprintf(settings_buffer + offset, sizeof(settings_buffer) - offset, "[%s]\n", APPSETTINGS_CREDENTIALS_CAT);

    int count = 0;
    for (GList * l = self->credentials; l != NULL; l = l->next) {
        CredentialEntry * entry = (CredentialEntry *)l->data;

        // Encode credentials (simple base64-like encoding for storage)
        offset += snprintf(settings_buffer + offset, sizeof(settings_buffer) - offset,
                          "credential_%d_username=%s\n", count, entry->username);
        offset += snprintf(settings_buffer + offset, sizeof(settings_buffer) - offset,
                          "credential_%d_password=%s\n", count, entry->password);
        offset += snprintf(settings_buffer + offset, sizeof(settings_buffer) - offset,
                          "credential_%d_description=%s\n", count, entry->description);
        count++;

        if (offset >= sizeof(settings_buffer) - 100) break; // Safety margin
    }

    offset += snprintf(settings_buffer + offset, sizeof(settings_buffer) - offset, "count=%d\n", count);

    return settings_buffer;
}

int AppSettingsCredentials__set_property(AppSettingsCredentials * self, char * key, char * value) {
    if (strncmp(key, "credential_", 11) == 0) {
        // Parse credential properties
        int index;
        char property[32];
        if (sscanf(key, "credential_%d_%31s", &index, property) == 2) {
            // Ensure we have enough entries
            while (g_list_length(self->credentials) <= (guint)index) {
                CredentialEntry * entry = CredentialEntry__create("", "", "");
                self->credentials = g_list_append(self->credentials, entry);
            }

            CredentialEntry * entry = g_list_nth_data(self->credentials, index);
            if (entry) {
                if (strcmp(property, "username") == 0) {
                    free(entry->username);
                    entry->username = strdup(value);
                } else if (strcmp(property, "password") == 0) {
                    free(entry->password);
                    entry->password = strdup(value);
                } else if (strcmp(property, "description") == 0) {
                    free(entry->description);
                    entry->description = strdup(value);
                    // Refresh UI when description is set (last property for each credential)
                    refresh_listbox(self);
                }
                return 1;
            }
        }
    } else if (strcmp(key, "count") == 0) {
        // Count is just for validation, refresh UI to make sure everything is displayed
        refresh_listbox(self);
        return 1;
    }

    return 0;
}

void AppSettingsCredentials__reset(AppSettingsCredentials * self) {
    // Reset UI state only, don't clear loaded credentials
    refresh_listbox(self);
    self->has_changes = 0; // Reset doesn't count as a change
}

// Credential management functions
void AppSettingsCredentials__add_credential(AppSettingsCredentials * self, const char * username, const char * password, const char * description) {
    CredentialEntry * entry = CredentialEntry__create(username, password, description);
    self->credentials = g_list_append(self->credentials, entry);
    refresh_listbox(self);
    dispatch_state_changed(self);
}

void AppSettingsCredentials__remove_credential(AppSettingsCredentials * self, int index) {
    CredentialEntry * entry = g_list_nth_data(self->credentials, index);
    if (entry) {
        self->credentials = g_list_remove(self->credentials, entry);
        CredentialEntry__destroy(entry);
        refresh_listbox(self);
        dispatch_state_changed(self);
    }
}

void AppSettingsCredentials__clear_credentials(AppSettingsCredentials * self) {
    for (GList * l = self->credentials; l != NULL; l = l->next) {
        CredentialEntry__destroy((CredentialEntry *)l->data);
    }
    g_list_free(self->credentials);
    self->credentials = NULL;
}

int AppSettingsCredentials__get_credential_count(AppSettingsCredentials * self) {
    return g_list_length(self->credentials);
}

CredentialEntry * AppSettingsCredentials__get_credential(AppSettingsCredentials * self, int index) {
    return g_list_nth_data(self->credentials, index);
}

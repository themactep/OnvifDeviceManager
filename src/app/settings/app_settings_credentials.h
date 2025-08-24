#ifndef ONVIF_APP_SETTINGS_CREDENTIALS_H_ 
#define ONVIF_APP_SETTINGS_CREDENTIALS_H_

#include <gtk/gtk.h>

typedef struct _AppSettingsCredentials AppSettingsCredentials;
typedef struct _CredentialEntry CredentialEntry;

struct _CredentialEntry {
    char * username;
    char * password;
    char * description;
};

struct _AppSettingsCredentials {
    GtkWidget * widget;
    GtkWidget * listbox;
    GtkWidget * add_button;
    GtkWidget * remove_button;
    GtkWidget * edit_button;
    
    // Dialog widgets for add/edit
    GtkWidget * dialog;
    GtkWidget * username_entry;
    GtkWidget * password_entry;
    GtkWidget * description_entry;
    
    // List of credential entries
    GList * credentials;

    // Currently selected row
    GtkListBoxRow * selected_row;

    // State tracking
    int has_changes;

    void (*state_changed_callback)(void * );
    void * state_changed_user_data;
};

// Main functions
AppSettingsCredentials * AppSettingsCredentials__create(void (*state_changed_callback)(void * ),void * state_changed_user_data);
void AppSettingsCredentials__destroy(AppSettingsCredentials * self);
GtkWidget * AppSettingsCredentials__get_widget(AppSettingsCredentials * self);

// Settings management
int AppSettingsCredentials__get_state(AppSettingsCredentials * self);
void AppSettingsCredentials__set_state(AppSettingsCredentials * self, int state);
char * AppSettingsCredentials__save(AppSettingsCredentials * self);
void AppSettingsCredentials__reset(AppSettingsCredentials * self);
char * AppSettingsCredentials__get_category(AppSettingsCredentials * self);
int AppSettingsCredentials__set_property(AppSettingsCredentials * self, char * key, char * value);

// Credential management
void AppSettingsCredentials__add_credential(AppSettingsCredentials * self, const char * username, const char * password, const char * description);
void AppSettingsCredentials__remove_credential(AppSettingsCredentials * self, int index);
void AppSettingsCredentials__clear_credentials(AppSettingsCredentials * self);
int AppSettingsCredentials__get_credential_count(AppSettingsCredentials * self);
CredentialEntry * AppSettingsCredentials__get_credential(AppSettingsCredentials * self, int index);

// Credential entry functions
CredentialEntry * CredentialEntry__create(const char * username, const char * password, const char * description);
void CredentialEntry__destroy(CredentialEntry * entry);
const char * CredentialEntry__get_username(CredentialEntry * entry);
const char * CredentialEntry__get_password(CredentialEntry * entry);
const char * CredentialEntry__get_description(CredentialEntry * entry);

#endif

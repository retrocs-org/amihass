#ifndef ND_PANEL_H
#define ND_PANEL_H
/* A non-modal native panel: the owner keeps serving dock messages. */
struct NDPanel;
#define ND_PANEL_CHECKBOX (~0U)
#define ND_PANEL_CHOICE (~0U-1)
#define ND_PANEL_PERCENT (~0U-2)
#define ND_PANEL_BYTE (~0U-3)
struct NDPanelField {const char *label,*value;unsigned int limit;};
struct NDPanel *nd_panel_open(const char *title,const struct NDPanelField *fields,
                              unsigned int count,const char **buttons,unsigned int buttons_count);
struct NDPanel *nd_panel_preferences(const struct NDPanelField *fields,int boot);
void nd_panel_enable(struct NDPanel *panel,unsigned int id,int enabled);
struct NDPanel *nd_panel_server_settings(const struct NDPanelField *fields,int reconnect,unsigned int retry_interval);
/* Server editor: URL/token/transport/status/name fields, saved connections left. */
struct NDPanel *nd_panel_server_editor(const struct NDPanelField *fields,const char **names,unsigned int count,unsigned int selected,int reconnect,unsigned int retry_interval);
void nd_panel_servers(struct NDPanel *panel,const char **names,unsigned int count,unsigned int selected);
unsigned int nd_panel_server_selected(struct NDPanel *panel);
int nd_panel_reconnect(struct NDPanel *panel);
unsigned int nd_panel_retry(struct NDPanel *panel);
void nd_panel_connection_state(struct NDPanel *panel,int connected,int enabled);
void nd_panel_service_state(struct NDPanel *panel,int running,int busy);
void nd_panel_show_secret(struct NDPanel *panel,int show);
int nd_panel_secret_shown(struct NDPanel *panel);
int nd_panel_enabled(struct NDPanel *panel,unsigned int id);
void nd_panel_set_source(struct NDPanel *panel,int sample);
int nd_panel_source(struct NDPanel *panel);
void nd_panel_choices(struct NDPanel *panel,unsigned int field,const char **labels,unsigned int active);
unsigned int nd_panel_choice(struct NDPanel *panel,unsigned int field);
unsigned int nd_panel_level(struct NDPanel *panel,unsigned int field);
void nd_panel_set_level(struct NDPanel *panel,unsigned int field,unsigned int level);
int nd_panel_toggle(struct NDPanel *panel,unsigned int index);
int nd_panel_checked(struct NDPanel *panel);
void nd_panel_close(struct NDPanel *panel);
void nd_panel_raise(struct NDPanel *panel);
unsigned long nd_panel_signals(const struct NDPanel *panel);
/* Wait for panel input, an extra signal or ticks (1/50 s), whichever is first. */
void nd_panel_wait(struct NDPanel *panel,unsigned long extra,unsigned long ticks);
/* 0 no action, -1 close/Escape, 100+button index, 1+field index on Return/Tab. */
int nd_panel_event(struct NDPanel *panel);
const char *nd_panel_get(struct NDPanel *panel,unsigned int index);
void nd_panel_set_input(struct NDPanel *panel,unsigned int index,const char *text);
void nd_panel_set_label(struct NDPanel *panel,unsigned int index,const char *label);
void nd_panel_set(struct NDPanel *panel,unsigned int index,const char *text);
void nd_panel_title(struct NDPanel *panel,const char *title);
void nd_panel_beep(struct NDPanel *panel);
#endif

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
int nd_panel_reconnect(struct NDPanel *panel);
unsigned int nd_panel_retry(struct NDPanel *panel);
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
/* 0 no action, -1 close/Escape, 100+button index, 1+field index on Return/Tab. */
int nd_panel_event(struct NDPanel *panel);
const char *nd_panel_get(struct NDPanel *panel,unsigned int index);
void nd_panel_set_input(struct NDPanel *panel,unsigned int index,const char *text);
void nd_panel_set_label(struct NDPanel *panel,unsigned int index,const char *label);
void nd_panel_set(struct NDPanel *panel,unsigned int index,const char *text);
void nd_panel_title(struct NDPanel *panel,const char *title);
void nd_panel_beep(struct NDPanel *panel);
#endif

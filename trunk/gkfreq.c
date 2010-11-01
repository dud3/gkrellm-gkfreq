/*
 * version 1.0
 * Using code from gkx86info http://anchois.free.fr/
 * with patches from whatdoineed2do@yahoo.co.uk
 * and knefas@gmail.com
 */

#include <gkrellm2/gkrellm.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>


static GkrellmMonitor *monitor;
static GkrellmPanel *panel;
static GkrellmDecal *decal_text1;
static gint style_id;


static void read_MHz(char* buffer_, size_t bufsz_)
{
	FILE *f;
	if ( (f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r")) == NULL) {
		snprintf(buffer_, bufsz_, "n/a MHz");
	} else {
		int i;
		fscanf(f, "%d", &i);
		snprintf(buffer_, bufsz_, "%d MHz", i/1000);
		fclose(f);
	}
}


static gint panel_expose_event(GtkWidget *widget, GdkEventExpose *ev)
{
	gdk_draw_pixmap(widget->window,
	                widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
	                panel->pixmap,
	                ev->area.x,
	                ev->area.y,
	                ev->area.x,
	                ev->area.y,
	                ev->area.width,
	                ev->area.height);

	return FALSE;
}


static void update_plugin()
{
	static gint x_scroll, w;
	static gchar info[32];

	if (w == 0)
		w = gkrellm_chart_width();
	x_scroll = (x_scroll + 1) % (2 * w);
	
	// dont do it too much...
	if ((GK.timer_ticks % 10) != 0)
		return;

	read_MHz(info, 31);

	GdkFont *fDesc = gdk_font_from_description(decal_text1->text_style.font);
	decal_text1->x_off = (w - gdk_string_width(fDesc, info)) / 2;
	if (decal_text1->x_off < 0)
		decal_text1->x_off = 0;

	gkrellm_draw_decal_text(panel, decal_text1, info, w - x_scroll);
	gkrellm_draw_panel_layers(panel);
}


static void create_plugin(GtkWidget *vbox, gint first_create)
{
	GkrellmStyle *style;
	GkrellmTextstyle *ts, *ts_alt;

	if (first_create)
		panel = gkrellm_panel_new0();

	style = gkrellm_meter_style(style_id);

	ts = gkrellm_meter_textstyle(style_id);
	ts_alt = gkrellm_meter_alt_textstyle(style_id);

	decal_text1 = gkrellm_create_decal_text(panel, "8MHz", ts, style, -1, -1, -1);

	gkrellm_panel_configure(panel, NULL, style);
	gkrellm_panel_create(vbox, monitor, panel);

	if (first_create)
		g_signal_connect(G_OBJECT (panel->drawing_area),
		                 "expose_event",
		                 G_CALLBACK (panel_expose_event),
		                 NULL);
}


static GkrellmMonitor plugin_mon = {
	"gkfreq",                     /* Name, for config tab */
	0,                            /* Id, 0 if a plugin */
	create_plugin,                /* The create function */
	update_plugin,                /* The update function */
	NULL,                         /* The config tab create function */
	NULL,                         /* Apply the config function */

	NULL,                         /* Save user config */
	NULL,                         /* Load user config */
	NULL,                         /* config keyword */

	NULL,                         /* Undefined 2 */
	NULL,                         /* Undefined 1 */
	NULL,                         /* private */

	MON_INSERT_AFTER | MON_CPU,   /* Insert plugin before this monitor */
	NULL,                         /* Handle if a plugin, filled in by GKrellM */
	NULL                          /* path if a plugin, filled in by GKrellM */
};


GkrellmMonitor* gkrellm_init_plugin()
{
	style_id = gkrellm_add_meter_style(&plugin_mon, "gkfreq");
	monitor = &plugin_mon;

	return &plugin_mon;
}

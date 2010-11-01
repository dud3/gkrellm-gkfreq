/*
 * version 1.1
 * Using code from gkx86info http://anchois.free.fr/
 * with patches from whatdoineed2do@yahoo.co.uk
 * and knefas@gmail.com
 * last modified by carlo.casta@gmail.com (oct 2010)
 */

#include <gkrellm2/gkrellm.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#define GKFREQ_MAX_CPUS 8

static GkrellmMonitor *monitor;
static GkrellmPanel *panel;
static GkrellmDecal *decal_text[8];
static gint style_id;
static gint cpu_online[8];

static void read_MHz(int cpu_id, char *buffer_, size_t bufsz_)
{
	FILE *f;
	char syspath[60];
	int i;

	snprintf(syspath, 60, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu_id);
	
	if ((f = fopen(syspath, "r")) == NULL) {
		snprintf(buffer_, bufsz_, "CPU%d N/A MHz", cpu_id);
	} else {
		fscanf(f, "%d", &i);
		i /= 1000;
		if (i < 1000)
			snprintf(buffer_, bufsz_, "CPU%d @ %d MHz", cpu_id, i);
		else
			snprintf(buffer_, bufsz_, "CPU%d @ %.2f GHz", cpu_id, i * 0.001f);
		fclose(f);
	}
}


static void get_CPUCount()
{
	char c;
	int i;

	for (i = 0; i < 8; ++i)
		cpu_online[i] = 0;
		
	FILE *f = fopen("/sys/devices/system/cpu/online", "r");
	if (f != NULL) {

		while ((c = fgetc(f)) != EOF) {
			if (c == '-') {
				continue;
			} else if (isdigit(c)) {
				short cpu_id = c - '0';
				if (cpu_id >= 0 && cpu_id < 8) {
					cpu_online[cpu_id] = 1;
				}
			}
		}
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
	int i;

	if ((GK.timer_ticks % 10) != 0)
		return;

	if (w == 0)
		w = gkrellm_chart_width();
	x_scroll = (x_scroll + 1) % (2 * w);

	for (i = 0; i < GKFREQ_MAX_CPUS; ++i) {
		if (cpu_online[i] == 1) {
			read_MHz(i, info, 31);

			GdkFont *fDesc = gdk_font_from_description(decal_text[i]->text_style.font);
			decal_text[i]->x_off = (w - gdk_string_width(fDesc, info)) / 2;
			if (decal_text[i]->x_off < 0)
				decal_text[i]->x_off = 0;

			gkrellm_draw_decal_text(panel, decal_text[i], info, w - x_scroll);
		}
	}

	gkrellm_draw_panel_layers(panel);
}


static void create_plugin(GtkWidget *vbox, gint first_create) 
{
	GkrellmStyle *style;
	GkrellmTextstyle *ts, *ts_alt;
	int i, y;

	get_CPUCount();

	if (first_create)
		panel = gkrellm_panel_new0();

	style = gkrellm_meter_style(style_id);

	ts = gkrellm_meter_textstyle(style_id);
	ts_alt = gkrellm_meter_alt_textstyle(style_id);

	y = -1;
	for (i = 0; i < GKFREQ_MAX_CPUS; ++i) {
		if (cpu_online[i] == 1) {
			decal_text[i] = gkrellm_create_decal_text(panel, "CPU8 @ 88888 MHz", ts, style, -1, y, -1);
			y += decal_text[i]->y + decal_text[i]->h + style->border.top + style->border.bottom;
		}
	}

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

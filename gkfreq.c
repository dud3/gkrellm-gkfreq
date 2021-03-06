/*****************************************************************************
 * GKrellM GKfreq                                                            *
 * A plugin for GKrellM showing current cpu frequency scale                  *
 * Copyright (C) 2010 Carlo Casta <carlo.casta@gmail.com>                    *
 *                                                                           *
 * This program is free software; you can redistribute it and/or modify      *  
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation; either version 2 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program; if not, write to the Free Software               *
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA *
 *                                                                           *
 *****************************************************************************/

#include <gkrellm2/gkrellm.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#define GKFREQ_MAX_CPUS 8
#define GKFREQ_CONFIG_KEYWORD "gkfreq"

static GkrellmMonitor *monitor;
static GkrellmPanel *panel;
static GkrellmDecal *decal_text[8];
static GtkWidget *text_format_combo_box;
static int style_id;
static int cpu_online[8];
static gchar *text_format;


static void format_String(int cpuid, int hz, char *buf, int bufsize)
{
	gchar *ptr;
	int len;
	
	/* safe default */
	if (text_format == NULL)
		gkrellm_dup_string(&text_format, "$L: $F");
	
	ptr = text_format;
	while (*ptr != '\0' && bufsize > 0) {
		len = 1;
		if (*ptr == '$') {
			++ptr;
			switch (*ptr) {
			case 'L':
				len = snprintf(buf, bufsize, "CPU%d", cpuid);
				break;
			case 'N':
				len = snprintf(buf, bufsize, "%d", cpuid);
				break;
			case 'F':
				if (hz < 0)
					len = snprintf(buf, bufsize, "N/A MHz");
				else if (hz < 1000000)
					len = snprintf(buf, bufsize, "%d MHz", hz / 1000);
				else
					len = snprintf(buf, bufsize, "%.2f GHz", hz * 0.000001f);
				break;
			case 'M':
				if (hz < 0)
					len = snprintf(buf, bufsize, "N/A MHz");
				else
					len = snprintf(buf, bufsize, "%d MHz", hz / 1000);
				break;
			case 'm':
				if (hz < 0)
					len = snprintf(buf, bufsize, "N/A");
				else
					len = snprintf(buf, bufsize, "%d", hz / 1000);
				break;
			case 'G':
				if (hz < 0)
					len = snprintf(buf, bufsize, "N/A GHz");
				else
					len = snprintf(buf, bufsize, "%.2f GHz", hz * 0.000001f);
				break;
			case 'g':
				if (hz < 0)
					len = snprintf(buf, bufsize, "N/A");
				else
					len = snprintf(buf, bufsize, "%.2f", hz * 0.000001f);
				break;
			default:
				*buf = *ptr;
				break;
			}
		} else {
			*buf = *ptr;
		}
		
		bufsize -= len;
		buf += len;
		++ptr;
	}
	*buf = '\0';
}


static void read_MHz(int cpu_id, char *buffer_, int bufsz_)
{
	FILE *f;
	char syspath[] = "/sys/devices/system/cpu/cpuN/cpufreq/scaling_cur_freq";
	gchar *buffer_ptr;
	int freq;

	syspath[27] = cpu_id + '0';

	if ((f = fopen(syspath, "r")) == NULL) {
		freq = -1;
	} else {
		fscanf(f, "%d", &freq);
		fclose(f);
	}
	
	buffer_ptr = buffer_;
	format_String(cpu_id, freq, buffer_ptr, bufsz_);
}


static void get_CPUCount()
{
	/*
	 * From Kernel Doc: cputopology.txt
	 *
	 * [...]
	 * In this example, there are 64 CPUs in the system but cpus 32-63 exceed
	 * the kernel max which is limited to 0..31 by the NR_CPUS config option
	 * being 32.  Note also that CPUs 2 and 4-31 are not online but could be
	 * brought online as they are both present and possible.
	 *
	 * kernel_max: 31
	 * offline: 2,4-31,32-63
	 * online: 0-1,3
	 * possible: 0-31
	 * present: 0-31
	 */

	char c;
	int i, j, range_mode = 0;
	FILE *f = NULL;

	f = fopen("/sys/devices/system/cpu/online", "r");
	if (f != NULL) {
		i = 0;
		while ((c = fgetc(f)) != EOF) {

			switch (c) {
			case ',':
				continue;
			case '-':
				range_mode = 1;
				break;
			default:
				if (!isdigit(c))
					break;
				
				if (range_mode == 1) {
					for (j = cpu_online[i - 1] + 1; j <= c - '0'; ++j)
						cpu_online[i++] = j;
					range_mode = 0;
				} else {
					cpu_online[i++] = c - '0';
				}
				break;
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

	return 0;
}


static void update_plugin()
{
	static int w, x_scroll[GKFREQ_MAX_CPUS];
	static char info[32];
	int i, idx;

	w = gkrellm_chart_width();

	i = 0;
	while ((idx = cpu_online[i++]) != -1) {
		
		gboolean scrolling;
		gint w_scroll, w_decal;

		read_MHz(idx, info, 31);

		w_scroll = gdk_string_width(
			gdk_font_from_description(decal_text[idx]->text_style.font),
			info
		);

		decal_text[idx]->x_off = (w - w_scroll) / 2;
		scrolling = (decal_text[idx]->x_off < 0)? TRUE : FALSE;
		if (scrolling) {
#if defined(GKRELLM_HAVE_DECAL_SCROLL_TEXT)
			gkrellm_decal_scroll_text_set_text(panel, decal_text[idx], info);
			gkrellm_decal_scroll_text_get_size(decal_text[idx],
			                                   &w_scroll,
			                                   NULL);
			gkrellm_decal_get_size(decal_text[idx], &w_decal, NULL);
			
			x_scroll[idx] = (x_scroll[idx] + 1) % (2 * w);
			
			gkrellm_decal_scroll_text_horizontal_loop(decal_text[idx],
			                                          scrolling);
			gkrellm_decal_text_set_offset(decal_text[idx], x_scroll[idx], 0);
#else
			decal_text[idx]->x_off = 0;
			gkrellm_draw_decal_text(panel, decal_text[idx], info, 0);
#endif
		} else {
			gkrellm_draw_decal_text(panel, decal_text[idx], info, 0);
		}
	}

	gkrellm_draw_panel_layers(panel);
}


static void cb_text_format(GtkWidget *widget, gpointer data)
{
	gchar *s;
	GtkWidget *entry;

	entry = gtk_bin_get_child(GTK_BIN(text_format_combo_box));
	s = gkrellm_gtk_entry_get_text(&entry);
	gkrellm_dup_string(&text_format, s);
}


static gchar *gkfreq_info_text[] = {
	N_("<h>Label\n"),
	N_("Substitution variables for the format string for label:\n"),
	N_("\t$L    the CPU label\n"),
	N_("\t$N    the CPU id\n"),
	N_("\t$F    the CPU frequency, in MHz or GHz\n"),
	N_("\t$M    the CPU frequency, in MHz\n"),
	N_("\t$m    the CPU frequency, in MHz, without 'MHz' string\n"),
	N_("\t$G    the CPU frequency, in GHz\n"),
	N_("\t$g    the CPU frequency, in GHz, without 'GHz' string\n"),
	N_("\t$$    $ symbol")
};

static void create_plugin_tab(GtkWidget *tab_vbox)
{
	GtkWidget *tabs, *hbox, *vbox, *vbox1, *text;
	int i;
	
	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));
	vbox1 = gkrellm_gtk_category_vbox(vbox, _("Format String for Label"),
	                                  4, 0, TRUE);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
	text_format_combo_box = gtk_combo_box_entry_new_text();
	gtk_box_pack_start(GTK_BOX(hbox), text_format_combo_box, TRUE, TRUE, 0);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
	                          text_format);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
	                          _("$L: $F"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(text_format_combo_box), 0);
	
	g_signal_connect(G_OBJECT(GTK_COMBO_BOX(text_format_combo_box)),
	                 "changed",
	                 G_CALLBACK(cb_text_format),
	                 NULL);

	text = gkrellm_gtk_scrolled_text_view(vbox, NULL, GTK_POLICY_AUTOMATIC,
	                                                  GTK_POLICY_AUTOMATIC);

	for (i = 0; i < sizeof(gkfreq_info_text) / sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(gkfreq_info_text[i]));
}


static void save_plugin_config(FILE *f)
{
	fprintf(f, "%s text_format %s\n", GKFREQ_CONFIG_KEYWORD, text_format);
}


static void load_plugin_config(gchar *arg)
{
	gchar config[32], item[CFG_BUFSIZE];
	
	if ((sscanf(arg, "%31s %[^\n]", config, item)) == 2)
		gkrellm_dup_string(&text_format, item);
}


void gkfreq_click_event(GtkWidget *w, GdkEventButton *event, gpointer p)
{
	if (event->button == 3)
		gkrellm_open_config_window(monitor);
}


static void create_plugin(GtkWidget *vbox, gint first_create) 
{
	GkrellmStyle *style;
	GkrellmTextstyle *ts;
	int i, idx, y;

	memset(cpu_online, -1, GKFREQ_MAX_CPUS * sizeof(gint));

	get_CPUCount();

	if (first_create)
		panel = gkrellm_panel_new0();

	style = gkrellm_meter_style(style_id);
	ts = gkrellm_meter_textstyle(style_id);

	y = -1;
	i = 0;
	while ((idx = cpu_online[i++]) != -1) {

		decal_text[idx] = gkrellm_create_decal_text(panel,
		                                            "CPU8: @ 8888GHz",
		                                            ts,
		                                            style,
		                                            -1,
		                                            y,
		                                            -1);
		y = decal_text[idx]->y
		  + decal_text[idx]->h
		  + style->border.top
		  + style->border.bottom;

	}
	gkrellm_panel_configure(panel, NULL, style);
	gkrellm_panel_create(vbox, monitor, panel);

	if (first_create) {
		g_signal_connect(G_OBJECT(panel->drawing_area),
		                 "expose_event",
		                 G_CALLBACK(panel_expose_event),
		                 NULL);
		
		g_signal_connect(G_OBJECT(panel->drawing_area),
		                 "button_press_event",
		                 G_CALLBACK(gkfreq_click_event),
		                 NULL);
	}
}


static GkrellmMonitor plugin_mon = {
	"gkfreq",                   /* Name, for config tab */
	0,                          /* Id, 0 if a plugin */
	create_plugin,              /* The create function */
	update_plugin,              /* The update function */
	create_plugin_tab,          /* The config tab create function */
	NULL,                       /* Apply the config function */

	save_plugin_config,         /* Save user config */
	load_plugin_config,         /* Load user config */
	GKFREQ_CONFIG_KEYWORD,      /* config keyword */

	NULL,                       /* Undefined 2 */
	NULL,                       /* Undefined 1 */
	NULL,                       /* private */

	MON_CPU,                    /* Insert plugin before this monitor */
	NULL,                       /* Handle if a plugin, filled in by GKrellM */
	NULL                        /* path if a plugin, filled in by GKrellM */
};


GkrellmMonitor* gkrellm_init_plugin()
{
	style_id = gkrellm_add_meter_style(&plugin_mon, "gkfreq");
	monitor = &plugin_mon;

	return &plugin_mon;
}

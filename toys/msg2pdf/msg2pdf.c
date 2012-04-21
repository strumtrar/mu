/*
** Copyright (C) 2012 Dirk-Jan C. Binnema <djcb@djcbsoftware.nl>
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 3, or (at your option) any
** later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation,
** Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
**
*/

#include <mu-msg.h>
#include <mu-date.h>
#include <mu-msg-part.h>

#include <gtk/gtk.h>
#include <webkit/webkitwebview.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static GtkPrintOperation*
get_print_operation (const char *path)
{
	GtkPrintOperation *printop;

	printop = gtk_print_operation_new ();

	gtk_print_operation_set_export_filename
		(GTK_PRINT_OPERATION(printop), path);

	return printop;
}


/* return the path to the output file, or NULL in case of error */
static char*
generate_pdf (MuMsg *msg, const char *str, GError **err)
{
	GtkWidget *view;
	WebKitWebFrame *frame;
	WebKitWebSettings *settings;

	GtkPrintOperationResult printres;
	GtkPrintOperation *printop;
	char *path;

	settings = webkit_web_settings_new ();
	g_object_set (G_OBJECT(settings),
		      "enable-scripts", FALSE,
		      "auto-load-images", TRUE,
		      "enable-plugins", FALSE,
		      NULL);

	view = webkit_web_view_new ();
	webkit_web_view_set_settings (WEBKIT_WEB_VIEW(view), settings);
 	webkit_web_view_load_string (WEBKIT_WEB_VIEW(view),
				     str, "text/html", "utf-8", "");
	g_object_unref (settings);
	frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW(view));
	if (!frame) {
		g_set_error (err, 0, MU_ERROR, "cannot get web frame");
		return FALSE;
	}

	path = g_strdup_printf ("%s%c%x.pdf",mu_util_cache_dir(),
				G_DIR_SEPARATOR, (unsigned)random());

	printop  = get_print_operation (path);
	printres = webkit_web_frame_print_full
		(frame, printop, GTK_PRINT_OPERATION_ACTION_EXPORT,
		 err);
	g_object_unref (printop);

	if (printres != GTK_PRINT_OPERATION_RESULT_ERROR)
		return path;
	else {
		g_free (path);
		return  NULL;
	}
}


static void
add_header (GString *gstr, const char* header, const char *val)
{
	char *esc;

	if (!val)
		return;

	esc = g_markup_escape_text (val, -1);
	g_string_append_printf (gstr, "<b>%s</b>: %s<br>", header, esc);
	g_free (esc);
}

 /* return the path to the output file, or NULL in case of error */
static char*
convert_to_pdf (MuMsg *msg, GError **err)
{
	GString *gstr;
	const char *body;
	gchar *data, *path;

	gstr = g_string_sized_new (4096);

	add_header (gstr, "From", mu_msg_get_from (msg));
	add_header (gstr, "To", mu_msg_get_to (msg));
	add_header (gstr, "Cc", mu_msg_get_cc (msg));
	add_header (gstr, "Subject", mu_msg_get_subject (msg));
	add_header (gstr, "Date", mu_date_display_s (mu_msg_get_date(msg)));

	gstr =	g_string_append (gstr, "<hr>\n");

	body = mu_msg_get_body_html (msg);
	if (body)
 		g_string_append_printf (gstr, "%s", body);
	else {
		body = mu_msg_get_body_text (msg);
		if (body) {
			gchar *esc;
			esc = g_markup_escape_text (body, -1);
			g_string_append_printf (gstr, "<pre>\n%s\n</pre>",
						esc);
			g_free (esc);
		} else
			gstr = g_string_append
				(gstr, "<i>No body</i>\n");
	}

	data = g_string_free (gstr, FALSE);
	path = generate_pdf (msg, data, err);
	g_free (data);

	return path;
}


int
main(int argc, char *argv[])
{
	MuMsg *msg;
	GError *err;
	char *path;

	if (argc != 2) {
		g_print ("msg2pdf: generate pdf files from e-mail messages\n"
		 "usage: msg2pdf <msgfile>\n");
		return 1;
	}

	gtk_init (&argc, &argv);

	if (access (argv[1], R_OK) != 0) {
		g_printerr ("%s is not a readable file\n", argv[1]);
		return 1;
	}

	err = NULL;
	msg = mu_msg_new_from_file (argv[1], NULL, &err);
	if (!msg) {
		g_printerr ("failed to create msg for %s: %s\n",
			    argv[1],
			    err && err->message ? err->message :
			    "unknown error");
		g_clear_error (&err);
		return 1;
	}

	path = convert_to_pdf (msg, &err);
	if (!path) {
		g_printerr ("failed to create pdf from %s: %s\n",
			    argv[1],
			    err && err->message ? err->message :
			    "unknown error");
		g_clear_error (&err);
		mu_msg_unref (msg);
		return 1;
	}
	mu_msg_unref (msg);

	g_print ("%s\n", path);
	return 0;
}
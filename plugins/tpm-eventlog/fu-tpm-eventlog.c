/*
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"FuTpmEventlog"

#include "config.h"

#include <fwupd.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <stdlib.h>
#include <unistd.h>

#include "fu-tpm-eventlog-parser.h"

static gint
fu_tmp_eventlog_sort_cb (gconstpointer a, gconstpointer b)
{
	FuTpmEventlogItem *item_a = *((FuTpmEventlogItem **) a);
	FuTpmEventlogItem *item_b = *((FuTpmEventlogItem **) b);
	if (item_a->pcr > item_b->pcr)
		return 1;
	if (item_a->pcr < item_b->pcr)
		return -1;
	return 0;
}

static gboolean
fu_tmp_eventlog_process (const gchar *fn, gint pcr, GError **error)
{
	gsize bufsz = 0;
	g_autofree guint8 *buf = NULL;
	g_autoptr(GPtrArray) items = NULL;
	g_autoptr(GString) str = g_string_new (NULL);

	/* parse this */
	if (!g_file_get_contents (fn, (gchar **) &buf, &bufsz, error))
		return FALSE;
	items = fu_tpm_eventlog_parser_new (buf, bufsz,
					    FU_TPM_EVENTLOG_PARSER_FLAG_ALL_ALGS |
					    FU_TPM_EVENTLOG_PARSER_FLAG_ALL_PCRS,
					    error);
	if (items == NULL)
		return FALSE;
	g_ptr_array_sort (items, fu_tmp_eventlog_sort_cb);

	for (guint i = 0; i < items->len; i++) {
		FuTpmEventlogItem *item = g_ptr_array_index (items, i);
		if (pcr >= 0 && item->pcr != pcr)
			continue;
		fu_tpm_eventlog_item_to_string (item, 0, str);
		g_string_append (str, "\n");
	}

	/* success */
	g_print ("%s", str->str);
	return TRUE;
}

int
main (int argc, char *argv[])
{
	const gchar *fn;
	gboolean verbose = FALSE;
	gint pcr = -1;
	g_autoptr(GError) error = NULL;
	g_autoptr(GOptionContext) context = g_option_context_new (NULL);
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			/* TRANSLATORS: command line option */
			_("Show extra debugging information"), NULL },
		{ "pcr", 'p', 0, G_OPTION_ARG_INT, &pcr,
			/* TRANSLATORS: command line option */
			_("Only show single PCR value"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, FWUPD_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* TRANSLATORS: program name */
	g_option_context_add_main_entries (context, options, NULL);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		/* TRANSLATORS: the user didn't read the man page */
		g_print ("%s: %s\n", _("Failed to parse arguments"),
			 error->message);
		return EXIT_FAILURE;
	}

	/* set verbose? */
	if (verbose) {
		g_setenv ("G_MESSAGES_DEBUG", "all", FALSE);
		g_setenv ("FWUPD_TPM_EVENTLOG_VERBOSE", "1", FALSE);
	}

	/* allow user to chose a local file */
	fn = argc <= 1 ? "/sys/kernel/security/tpm0/binary_bios_measurements" : argv[1];
	if (!fu_tmp_eventlog_process (fn, pcr, &error)) {
		/* TRANSLATORS: failed to read measurements file */
		g_printerr ("%s: %s\n", _("Failed to parse file"),
			 error->message);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

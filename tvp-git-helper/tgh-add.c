/*-
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>

#include <thunar-vfs/thunar-vfs.h>

#include "tgh-common.h"
#include "tgh-dialog-common.h"
#include "tgh-file-selection-dialog.h"

#include "tgh-add.h"

struct exit_args
{
  TghOutputParser *parser;
  GtkWidget *dialog;
};

static void child_exit(GPid pid, gint status, gpointer user_data)
{
  struct exit_args *args = user_data;

  gtk_widget_destroy(args->dialog);

  if(WEXITSTATUS(status) <= 1)
  {
    GtkWidget *dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_OTHER, GTK_BUTTONS_CLOSE, _("Add finished"));
    tgh_dialog_start(GTK_DIALOG(dialog), TRUE);
  }

  tgh_child_exit(pid, status, args->parser);

  g_free(args);
}

gboolean add_spawn (GtkWidget *dialog, gchar **files, GPid *pid)
{
  GError *error = NULL;
  gint fd_err;
  GIOChannel *chan_err;
  TghOutputParser *parser;
  gsize length;
  gint i;
  gchar **argv;
  struct exit_args *args = g_new(struct exit_args, 1);

  length = 3;
  length += g_strv_length(files);

  argv = g_new(gchar*, length);

  argv[0] = "git";
  argv[1] = "add";
  argv[length-1] = NULL;

  i = 2;
  while(*files)
    argv[i++] = *files++;

  if(!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL, NULL, pid, NULL, NULL, &fd_err, &error))
  {
    return FALSE;
  }

  parser = tgh_error_parser_new(GTK_WIDGET(dialog));

  args->parser = parser;
  args->dialog = dialog;

  g_child_watch_add(*pid, (GChildWatchFunc)child_exit, args);

  chan_err = g_io_channel_unix_new(fd_err);
  g_io_add_watch(chan_err, G_IO_IN|G_IO_HUP, (GIOFunc)tgh_parse_output_func, parser);

  return TRUE;
}

gboolean tgh_add (gchar **files, GPid *pid)
{
  GtkWidget *dialog;

  dialog = tgh_file_selection_dialog_new (_("Add"), NULL, 0, files, TGH_FILE_SELECTION_FLAG_MODIFIED|TGH_FILE_SELECTION_FLAG_UNTRACKED);
  if(gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
  {
    gtk_widget_destroy (dialog);
    return FALSE;
  }
  g_strfreev (files);
  files = tgh_file_selection_dialog_get_files (TGH_FILE_SELECTION_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if(!files)
    return FALSE;

  dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_OTHER, GTK_BUTTONS_CANCEL, _("Adding ..."));
	g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (tgh_cancel), NULL);
  tgh_dialog_start(GTK_DIALOG(dialog), TRUE);

  return add_spawn(dialog, files, pid);
}

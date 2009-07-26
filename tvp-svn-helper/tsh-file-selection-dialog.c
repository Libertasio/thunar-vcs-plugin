/*-
 * Copyright (c) 2006 Peter de Ridder <peter@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar-vfs/thunar-vfs.h>
#include <gtk/gtk.h>

#include <subversion-1/svn_client.h>
#include <subversion-1/svn_pools.h>

#include "tsh-common.h"
#include "tsh-tree-common.h"
#include "tsh-file-selection-dialog.h"

static void tsh_file_selection_status_func2 (void *, const char *, svn_wc_status2_t *);
static svn_error_t *tsh_file_selection_status_func3 (void *, const char *, svn_wc_status2_t *, apr_pool_t *);
static void selection_cell_toggled (GtkCellRendererToggle *, gchar *, gpointer);
static void selection_all_toggled (GtkToggleButton *, gpointer);

static gboolean count_selected (GtkTreeModel*, GtkTreePath*, GtkTreeIter*, gpointer);
static gboolean copy_selected (GtkTreeModel*, GtkTreePath*, GtkTreeIter*, gpointer);
static gboolean set_selected (GtkTreeModel*, GtkTreePath*, GtkTreeIter*, gpointer);
static void move_info (GtkTreeStore*, GtkTreeIter*, GtkTreeIter*);

struct _TshFileSelectionDialog
{
	GtkDialog dialog;

	GtkWidget *tree_view;
  GtkWidget *all;

  TshFileSelectionFlags flags;
};

struct _TshFileSelectionDialogClass
{
	GtkDialogClass dialog_class;
};

G_DEFINE_TYPE (TshFileSelectionDialog, tsh_file_selection_dialog, GTK_TYPE_DIALOG)

static void
tsh_file_selection_dialog_class_init (TshFileSelectionDialogClass *klass)
{
}

enum {
  COLUMN_PATH = 0,
  COLUMN_NAME,
  COLUMN_TEXT_STAT,
  COLUMN_PROP_STAT,
  COLUMN_SELECTION,
  COLUMN_ENABLED,
  COLUMN_COUNT
};

static void
tsh_file_selection_dialog_init (TshFileSelectionDialog *dialog)
{
	GtkWidget *tree_view;
	GtkWidget *scroll_window;
	GtkWidget *all;
	GtkCellRenderer *renderer;
	GtkTreeModel *model;
  gint n_columns;

	scroll_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	dialog->tree_view = tree_view = gtk_tree_view_new ();

	renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled", G_CALLBACK (selection_cell_toggled), dialog);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
	                                             -1, "", renderer,
                                               "active", COLUMN_SELECTION,
                                               "activatable", COLUMN_ENABLED,
                                               NULL);

	renderer = gtk_cell_renderer_text_new ();
  n_columns = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
                                                           -1, _("Path"), renderer,
                                                           "text", COLUMN_NAME,
                                                           NULL);
  gtk_tree_view_set_expander_column (GTK_TREE_VIEW (tree_view), gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), n_columns - 1));
  
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
	                                             -1, ("State"), renderer,
                                               "text", COLUMN_TEXT_STAT,
                                               NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
	                                             -1, ("Prop state"), renderer,
                                               "text", COLUMN_PROP_STAT,
                                               NULL);

  model = GTK_TREE_MODEL (gtk_tree_store_new (COLUMN_COUNT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN));

	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), model);

	g_object_unref (model);

	gtk_container_add (GTK_CONTAINER (scroll_window), tree_view);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), scroll_window, TRUE, TRUE, 0);
	gtk_widget_show (tree_view);
	gtk_widget_show (scroll_window);

	dialog->all = all = gtk_check_button_new_with_label (_("Select/Unselect all"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (all), TRUE);
  gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (all), TRUE);
  g_signal_connect (all, "toggled", G_CALLBACK (selection_all_toggled), dialog);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), all, FALSE, FALSE, 0);
	gtk_widget_show (all);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Status"));

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
	                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                        GTK_STOCK_OK, GTK_RESPONSE_OK,
	                        NULL);
	gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 200);
}

GtkWidget*
tsh_file_selection_dialog_new (const gchar *title, GtkWindow *parent, GtkDialogFlags flags, gchar **files, TshFileSelectionFlags selection_flags, svn_client_ctx_t *ctx, apr_pool_t *pool)
{
  svn_opt_revision_t revision;
	svn_error_t *err;
  apr_pool_t *subpool;

	TshFileSelectionDialog *dialog = g_object_new (TSH_TYPE_FILE_SELECTION_DIALOG, NULL);

	if(title)
		gtk_window_set_title (GTK_WINDOW(dialog), title);

	if(parent)
		gtk_window_set_transient_for (GTK_WINDOW(dialog), parent);

	if(flags & GTK_DIALOG_MODAL)
		gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);

	if(flags & GTK_DIALOG_DESTROY_WITH_PARENT)
		gtk_window_set_destroy_with_parent (GTK_WINDOW(dialog), TRUE);

	if(flags & GTK_DIALOG_NO_SEPARATOR)
		gtk_dialog_set_has_separator (GTK_DIALOG(dialog), FALSE);

  dialog->flags = selection_flags;

  subpool = svn_pool_create (pool);

  revision.kind = svn_opt_revision_head;
  if(files)
  {
    while (*files)
    {
#if CHECK_SVN_VERSION(1,5)
      if((err = svn_client_status3(NULL, *files, &revision, tsh_file_selection_status_func2, dialog, (selection_flags&TSH_FILE_SELECTION_FLAG_RECURSIVE)?svn_depth_infinity:svn_depth_immediates, selection_flags&TSH_FILE_SELECTION_FLAG_UNCHANGED, FALSE, selection_flags&TSH_FILE_SELECTION_FLAG_IGNORED, TRUE, NULL, ctx, subpool)))
#else /* CHECK_SVN_VERSION(1,6) */
      if((err = svn_client_status4(NULL, *files, &revision, tsh_file_selection_status_func3, dialog, (selection_flags&TSH_FILE_SELECTION_FLAG_RECURSIVE)?svn_depth_infinity:svn_depth_immediates, selection_flags&TSH_FILE_SELECTION_FLAG_UNCHANGED, FALSE, selection_flags&TSH_FILE_SELECTION_FLAG_IGNORED, TRUE, NULL, ctx, subpool)))
#endif
      {
        svn_pool_destroy (subpool);

        gtk_widget_unref(GTK_WIDGET(dialog));

        svn_error_clear(err);
        return NULL;  //FIXME: needed ??
      }
      files++;
    }
  }
  else
  {
#if CHECK_SVN_VERSION(1,5)
    if((err = svn_client_status3(NULL, "", &revision, tsh_file_selection_status_func2, dialog, (selection_flags&TSH_FILE_SELECTION_FLAG_RECURSIVE)?svn_depth_infinity:svn_depth_immediates, selection_flags&TSH_FILE_SELECTION_FLAG_UNCHANGED, FALSE, selection_flags&TSH_FILE_SELECTION_FLAG_IGNORED, TRUE, NULL, ctx, subpool)))
#else /* CHECK_SVN_VERSION(1,6) */
    if((err = svn_client_status4(NULL, "", &revision, tsh_file_selection_status_func3, dialog, (selection_flags&TSH_FILE_SELECTION_FLAG_RECURSIVE)?svn_depth_infinity:svn_depth_immediates, selection_flags&TSH_FILE_SELECTION_FLAG_UNCHANGED, FALSE, selection_flags&TSH_FILE_SELECTION_FLAG_IGNORED, TRUE, NULL, ctx, subpool)))
#endif
    {
      svn_pool_destroy (subpool);

      gtk_widget_unref(GTK_WIDGET(dialog));

      svn_error_clear(err);
      return NULL;
    }
  }

  svn_pool_destroy (subpool);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (dialog->tree_view));

  return GTK_WIDGET(dialog);
}

gchar**
tsh_file_selection_dialog_get_files (TshFileSelectionDialog *dialog)
{
  GtkTreeModel *model;
  gchar **files, **files_iter;
  guint count = 0;

  g_return_val_if_fail (TSH_IS_FILE_SELECTION_DIALOG (dialog), NULL);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->tree_view));

  gtk_tree_model_foreach (model, count_selected, &count);

  if (!count)
    return NULL;

  files_iter = files = g_new(gchar *, count+1);

  gtk_tree_model_foreach (model, copy_selected, &files_iter);

  *files_iter = NULL;

  return files;
}

static void
tsh_file_selection_status_func2(void *baton, const char *path, svn_wc_status2_t *status)
{
	TshFileSelectionDialog *dialog = TSH_FILE_SELECTION_DIALOG (baton);
  gboolean add = FALSE;

  if (dialog->flags & (status->entry?(TSH_FILE_SELECTION_FLAG_MODIFIED|TSH_FILE_SELECTION_FLAG_UNCHANGED|TSH_FILE_SELECTION_FLAG_IGNORED):TSH_FILE_SELECTION_FLAG_UNVERSIONED))
    add = TRUE;

  if (dialog->flags & TSH_FILE_SELECTION_FLAG_CONFLICTED)
    if(status->text_status == svn_wc_status_conflicted || status->prop_status == svn_wc_status_conflicted)
      add = TRUE;

  if (add)
  {
    GtkTreeModel *model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->tree_view));

    tsh_tree_get_iter_for_path (GTK_TREE_STORE (model), path, &iter, COLUMN_NAME, move_info);
    gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
                        COLUMN_PATH, path,
                        COLUMN_TEXT_STAT, tsh_status_to_string(status->text_status),
                        COLUMN_PROP_STAT, tsh_status_to_string(status->prop_status),
                        COLUMN_SELECTION, TRUE,
                        COLUMN_ENABLED, TRUE,
                        -1);
  }
}

static svn_error_t*
tsh_file_selection_status_func3(void *baton, const char *path, svn_wc_status2_t *status, apr_pool_t *pool)
{
    tsh_file_selection_status_func2(baton, path, status);
    return SVN_NO_ERROR;
}

static void
selection_cell_toggled (GtkCellRendererToggle *renderer, gchar *path, gpointer user_data)
{
	TshFileSelectionDialog *dialog = TSH_FILE_SELECTION_DIALOG (user_data);
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean selection;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->tree_view));

  gtk_tree_model_get_iter_from_string (model, &iter, path);

  gtk_tree_model_get (model, &iter,
                      COLUMN_SELECTION, &selection,
                      -1);
  gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
                      COLUMN_SELECTION, !selection,
                      -1);

  gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (dialog->all), TRUE);
}

static void
selection_all_toggled (GtkToggleButton *button, gpointer user_data)
{
	TshFileSelectionDialog *dialog = TSH_FILE_SELECTION_DIALOG (user_data);
  GtkTreeModel *model;
  gboolean selection;

  gtk_toggle_button_set_inconsistent (button, FALSE);
  
  selection = gtk_toggle_button_get_active (button);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->tree_view));

  gtk_tree_model_foreach (model, set_selected, GINT_TO_POINTER (selection));
}

static gboolean
count_selected (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer count)
{
  gboolean selection;
  gtk_tree_model_get (model, iter,
                      COLUMN_SELECTION, &selection,
                      -1);
  if (selection)
    (*(guint*)count)++;
  return FALSE;
}

static gboolean
copy_selected (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer files_iter)
{
  gboolean selection;
  gtk_tree_model_get (model, iter,
                      COLUMN_SELECTION, &selection,
                      -1);
  if (selection)
  {
    gtk_tree_model_get (model, iter,
                        COLUMN_PATH, *(gchar***)files_iter,
                        -1);
    (*(gchar***)files_iter)++;
  }
  return FALSE;
}

static gboolean
set_selected (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer selection)
{
  gboolean enabled;
  gtk_tree_model_get (model, iter,
                      COLUMN_ENABLED, &enabled,
                      -1);
  if (enabled)
    gtk_tree_store_set (GTK_TREE_STORE (model), iter,
                        COLUMN_SELECTION, GPOINTER_TO_INT (selection),
                        -1);
  return FALSE;
}

static void
move_info (GtkTreeStore *store, GtkTreeIter *dest, GtkTreeIter *src)
{
  gchar *path, *text, *prop;
  gboolean selected, enabled;

  gtk_tree_model_get (GTK_TREE_MODEL (store), src,
                      COLUMN_PATH, &path,
                      COLUMN_TEXT_STAT, &text,
                      COLUMN_PROP_STAT, &prop,
                      COLUMN_SELECTION, &selected,
                      COLUMN_ENABLED, &enabled,
                      -1);

  gtk_tree_store_set (store, dest,
                      COLUMN_PATH, path,
                      COLUMN_TEXT_STAT, text,
                      COLUMN_PROP_STAT, prop,
                      COLUMN_SELECTION, selected,
                      COLUMN_ENABLED, enabled,
                      -1);

  g_free (path);
  g_free (text);
  g_free (prop);
}


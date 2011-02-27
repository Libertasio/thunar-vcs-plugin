/*-
 * Copyright (C) 2007-2011  Peter de Ridder <peter@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __TGH_DIALOG_COMMON_H__
#define __TGH_DIALOG_COMMON_H__

G_BEGIN_DECLS

void tgh_dialog_start (GtkDialog*, gboolean);

void tgh_dialog_replace_action_area (GtkDialog *);

void tgh_make_homogeneous (GtkWidget *, ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /*__TGH_DIALOG_COMMON_H__*/


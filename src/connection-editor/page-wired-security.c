/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * Dan Williams <dcbw@redhat.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * (C) Copyright 2008 Red Hat, Inc.
 */

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <NetworkManager.h>
#include <nm-setting-connection.h>
#include <nm-setting-wired.h>
#include <nm-setting-8021x.h>
#include <nm-setting-wireless.h>
#include <nm-utils.h>

#include "wireless-security.h"
#include "page-wired.h"
#include "page-wired-security.h"
#include "nm-connection-editor.h"
#include "gconf-helpers.h"

G_DEFINE_TYPE (CEPageWiredSecurity, ce_page_wired_security, CE_TYPE_PAGE)

#define CE_PAGE_WIRED_SECURITY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_WIRED_SECURITY, CEPageWiredSecurityPrivate))

typedef struct {
	GtkToggleButton *enabled;
	GtkWidget *security_widget;
	WirelessSecurity *security;

	gboolean disposed;
} CEPageWiredSecurityPrivate;

static void
stuff_changed (WirelessSecurity *sec, gpointer user_data)
{
	ce_page_changed (CE_PAGE (user_data));
}

static void
enable_toggled (GtkToggleButton *button, gpointer user_data)
{
	CEPageWiredSecurityPrivate *priv = CE_PAGE_WIRED_SECURITY_GET_PRIVATE (user_data);

	gtk_widget_set_sensitive (priv->security_widget, gtk_toggle_button_get_active (priv->enabled));
	ce_page_changed (CE_PAGE (user_data));
}

CEPageWiredSecurity *
ce_page_wired_security_new (NMConnection *connection)
{
	CEPageWiredSecurity *self;
	CEPage *parent;
	CEPageWiredSecurityPrivate *priv;
	const char *glade_file = GLADEDIR "/applet.glade";
	const char *connection_id;
	NMSetting *setting;

	self = CE_PAGE_WIRED_SECURITY (g_object_new (CE_TYPE_PAGE_WIRED_SECURITY, NULL));
	parent = CE_PAGE (self);
	priv = CE_PAGE_WIRED_SECURITY_GET_PRIVATE (self);

	parent->title = g_strdup (_("802.1x Security"));
	parent->page = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (parent->page), 6);

	setting = nm_connection_get_setting (connection, NM_TYPE_SETTING_802_1X);

	connection_id = g_object_get_data (G_OBJECT (connection), NMA_CONNECTION_ID_TAG);
	priv->security = (WirelessSecurity *) ws_wpa_eap_new (glade_file, connection, connection_id);
	wireless_security_set_changed_notify (priv->security, stuff_changed, self);
	priv->security_widget = wireless_security_get_widget (priv->security);

	priv->enabled = GTK_TOGGLE_BUTTON (gtk_check_button_new_with_label (_("Use 802.1X security for this connection")));
	g_signal_connect (priv->enabled, "toggled",
					  G_CALLBACK (enable_toggled), self);

	gtk_toggle_button_set_active (priv->enabled, setting != NULL);
	gtk_widget_set_sensitive (priv->security_widget, setting != NULL);

	gtk_box_pack_start (GTK_BOX (parent->page), GTK_WIDGET (priv->enabled), FALSE, TRUE, 12);
	gtk_box_pack_start_defaults (GTK_BOX (parent->page), priv->security_widget);
	g_object_ref_sink (parent->page);
	gtk_widget_show_all (parent->page);

	return self;
}

static gboolean
validate (CEPage *page, GError **error)
{
	CEPageWiredSecurityPrivate *priv = CE_PAGE_WIRED_SECURITY_GET_PRIVATE (page);
	gboolean valid = TRUE;

	if (gtk_toggle_button_get_active (priv->enabled)) {
		/* FIXME: get failed property and error out of wireless security objects */
		valid = wireless_security_validate (priv->security, NULL);
		if (!valid)
			g_set_error (error, 0, 0, "Invalid 802.1x security");
	}

	return valid;
}

static void
update_connection (CEPage *page, NMConnection *connection)
{
	CEPageWiredSecurityPrivate *priv = CE_PAGE_WIRED_SECURITY_GET_PRIVATE (page);

	if (gtk_toggle_button_get_active (priv->enabled)) {
		NMConnection *tmp_connection;
		NMSetting *s_8021x;

		/* Here's a nice hack to work around the fact that ws_802_1x_fill_connection needs wireless setting. */
		tmp_connection = nm_connection_new ();
		nm_connection_add_setting (tmp_connection, nm_setting_wireless_new ());
		ws_802_1x_fill_connection (priv->security, "wpa_eap_auth_combo", tmp_connection);

		s_8021x = nm_connection_get_setting (tmp_connection, NM_TYPE_SETTING_802_1X);
		nm_connection_add_setting (connection, NM_SETTING (g_object_ref (s_8021x)));

		g_object_unref (tmp_connection);
	} else
		nm_connection_remove_setting (connection, NM_TYPE_SETTING_802_1X);
}

static void
ce_page_wired_security_init (CEPageWiredSecurity *self)
{
}

static void
dispose (GObject *object)
{
	CEPageWiredSecurityPrivate *priv = CE_PAGE_WIRED_SECURITY_GET_PRIVATE (object);

	if (priv->disposed)
		return;

	priv->disposed = TRUE;
	wireless_security_unref (priv->security);

	G_OBJECT_CLASS (ce_page_wired_security_parent_class)->dispose (object);
}

static void
ce_page_wired_security_class_init (CEPageWiredSecurityClass *wired_security_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (wired_security_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (wired_security_class);

	g_type_class_add_private (object_class, sizeof (CEPageWiredSecurityPrivate));

	/* virtual methods */
	object_class->dispose = dispose;

	parent_class->validate = validate;
	parent_class->update_connection = update_connection;
}

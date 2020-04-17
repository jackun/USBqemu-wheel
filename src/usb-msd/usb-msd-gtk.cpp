#include "usb-msd.h"
#include "../linux/ini.h"
#include "../configuration.h"
#include "gtk.h"

namespace usb_msd {

#define APINAME "cstdio"

static void entryChanged(GtkWidget *widget, gpointer data)
{
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(widget));
	//fprintf(stderr, "Entry text:%s\n", text);
}

static void fileChooser( GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog, *entry = NULL;

	entry = (GtkWidget*)data;
	dialog = gtk_file_chooser_dialog_new ("Open File",
					  NULL,
					  GTK_FILE_CHOOSER_ACTION_OPEN,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					  NULL);

	//XXX check access? Dialog seems to default to "Recently used" etc.
	//Or set to empty string anyway? Then it seems to default to some sort of "working dir"
	if (access (gtk_entry_get_text (GTK_ENTRY (entry)), F_OK) == 0)
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), gtk_entry_get_text (GTK_ENTRY (entry)));

	if (entry && gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		fprintf (stderr, "%s\n", filename);
		gtk_entry_set_text(GTK_ENTRY(entry), filename);
		g_free (filename);
	}

	gtk_widget_destroy (dialog);
}

int MsdDevice::Configure(int port, const std::string& api, void *data)
{
	GtkWidget *ro_frame, *ro_label, *rs_hbox, *rs_label, *rs_cb, *vbox;

	GtkWidget *dlg = gtk_dialog_new_with_buttons (
		"Mass Storage Settings", GTK_WINDOW (data), GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable (GTK_WINDOW (dlg), TRUE);
	GtkWidget *dlg_area_box = gtk_dialog_get_content_area (GTK_DIALOG (dlg));

	ro_frame = gtk_frame_new (NULL);
	gtk_box_pack_start (GTK_BOX (dlg_area_box), ro_frame, TRUE, FALSE, 5);

	ro_label = gtk_label_new ("Select USB image:");
	gtk_frame_set_label_widget (GTK_FRAME (ro_frame), ro_label);
	gtk_label_set_use_markup (GTK_LABEL (ro_label), TRUE);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (ro_frame), vbox);

	rs_hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), rs_hbox, FALSE, TRUE, 0);

	GtkWidget *entry = gtk_entry_new ();
	gtk_entry_set_max_length (GTK_ENTRY (entry), MAX_PATH); //TODO max length

	std::string var;
	if (LoadSetting(TypeName(), port, APINAME, N_CONFIG_PATH, var))
		gtk_entry_set_text(GTK_ENTRY(entry), var.c_str());

	g_signal_connect (entry, "changed", G_CALLBACK (entryChanged), NULL);

	GtkWidget *button = gtk_button_new_with_label ("Browse");
	gtk_button_set_image(GTK_BUTTON (button), gtk_image_new_from_icon_name ("gtk-open", GTK_ICON_SIZE_BUTTON));
	g_signal_connect (button, "clicked", G_CALLBACK (fileChooser), entry);

	gtk_box_pack_start (GTK_BOX (rs_hbox), entry, TRUE, TRUE, 5);
	gtk_box_pack_start (GTK_BOX (rs_hbox), button, FALSE, FALSE, 5);

	gtk_widget_show_all (dlg);
	gint result = gtk_dialog_run (GTK_DIALOG (dlg));
	std::string path = gtk_entry_get_text(GTK_ENTRY(entry));
	gtk_widget_destroy (dlg);

	// Wait for all gtk events to be consumed ...
	while (gtk_events_pending ())
		gtk_main_iteration_do (FALSE);

	if (result == GTK_RESPONSE_OK)
	{
		if(SaveSetting(TypeName(), port, APINAME, N_CONFIG_PATH, path))
			return RESULT_OK;
		else
			return RESULT_FAILED;
	}

	return RESULT_CANCELED;
}
#undef APINAME
}
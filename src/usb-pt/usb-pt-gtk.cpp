#include "usb-pt.h"
#include "configuration.h"
#include <gtk/gtk.h>
#include <vector>
#include <sstream>
#include <iomanip>

GtkWidget *new_combobox(const char* label, GtkWidget *vbox); // src/linux/config-gtk.cpp

static void populate_device_widget(GtkComboBox *widget, const ConfigUSBDevice& current, const std::vector<ConfigUSBDevice>& devs)
{
	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (widget)));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "None");
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

	int i = 1;
	for (auto& dev : devs)
	{
		std::stringstream str;
		str << dev.name
		<< " [" << std::hex << std::setw(4) << std::setfill('0')
		<< dev.vid
		<< ":" << std::hex << std::setw(4) << std::setfill('0')
		<< dev.pid
		<< "]";
		str << " [bus" << dev.bus << " port" << dev.port << "]";

		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), str.str().c_str());
		if (current == dev)
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
		i++;
	}
}

static void device_changed (GtkComboBox *widget, gpointer data)
{
	*(int*) data = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
}

static void checkbox_changed (GtkComboBox *widget, gpointer data)
{
	*(int*) data = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static int gtk_configure (int port, void *data)
{
	GtkWidget *ro_frame, *ro_label, *rs_hbox, *rs_label, *rs_cb, *vbox;

	ConfigUSBDevice current;
	std::vector<ConfigUSBDevice> devs;
	int dev_idx = 0;
	int ignore_busport = 0;

	{
		CONFIGVARIANT var(N_DEVICE, CONFIG_TYPE_CHAR);
		if (LoadSetting (port, APINAME, var))
		{
			sscanf(var.strValue.c_str(), "%d:%d:%x:%x:",
				&current.bus, &current.port, &current.vid, &current.pid);
		}
	}

	{
		CONFIGVARIANT var(N_IGNORE_BUSPORT, CONFIG_TYPE_INT);
		if (LoadSetting (port, APINAME, var))
			ignore_busport = var.intValue;
	}

	get_usb_devices (devs);

	GtkWidget *dlg = gtk_dialog_new_with_buttons (
		"USB Pass-through Settings", GTK_WINDOW (data), GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable (GTK_WINDOW (dlg), TRUE);
	GtkWidget *dlg_area_box = gtk_dialog_get_content_area (GTK_DIALOG (dlg));


	GtkWidget *main_vbox = gtk_vbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (dlg_area_box), main_vbox, TRUE, FALSE, 5);

	ro_frame = gtk_frame_new ("USB Devices");
	gtk_box_pack_start (GTK_BOX (main_vbox), ro_frame, TRUE, FALSE, 5);

	GtkWidget *frame_vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (ro_frame), frame_vbox);

	GtkWidget *cb = new_combobox("", frame_vbox);
	g_signal_connect (G_OBJECT (cb), "changed", G_CALLBACK (device_changed), &dev_idx);
	populate_device_widget (GTK_COMBO_BOX (cb), current, devs);

	cb = gtk_check_button_new_with_label ("Ignore bus/port number when looking for this device");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb), ignore_busport);
	gtk_container_add (GTK_CONTAINER (frame_vbox), cb);
	g_signal_connect (G_OBJECT (cb), "toggled", G_CALLBACK (checkbox_changed), &ignore_busport);

	gtk_widget_show_all (dlg);
	gint result = gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);

	// Wait for all gtk events to be consumed ...
	while (gtk_events_pending ())
		gtk_main_iteration_do (FALSE);

	int ret = RESULT_CANCELED;
	if (result == GTK_RESPONSE_OK)
	{
		ret = RESULT_OK;

		std::stringstream str;

		if (dev_idx > 0)
		{
			ConfigUSBDevice &dev = devs[dev_idx - 1];
			str << dev.bus << ":" << dev.port << ":"
			<< std::hex << std::setw(4) << std::setfill('0')
			<< dev.vid << ":"
			<< std::hex << std::setw(4) << std::setfill('0')
			<< dev.pid;
			str << ":" << dev.name;
		}

		CONFIGVARIANT var0(N_DEVICE, str.str());
		if (!SaveSetting(port, APINAME, var0))
			ret = RESULT_FAILED;

		CONFIGVARIANT var1(N_IGNORE_BUSPORT, ignore_busport);
		if (!SaveSetting(port, APINAME, var1))
			ret = RESULT_FAILED;
	}

	return ret;
}

int PTDevice::Configure(int port, const std::string& api, void *data)
{
	return gtk_configure (port, data);
}
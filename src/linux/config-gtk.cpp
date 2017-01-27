#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <string>
#include <gtk/gtk.h>

//joystick stuff
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/joystick.h>

#include "../USB.h"
#include "../osdebugout.h"
#include "../configuration.h"
#include "../deviceproxy.h"
#include "../usb-pad/padproxy.h"
#include "../usb-mic/audiosourceproxy.h"

#include "config.h"

//TODO better way to access API comboboxes from "device changed" callback
struct APICallback
{
	std::string device;
	std::string api;
	GtkComboBox* combo;
};
static APICallback apiCallback[2];

static void wheeltypeChanged (GtkComboBox *widget, gpointer data)
{
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	//if(data)
	{
		uint8_t ply = MIN((unsigned)data, 1);

		conf.WheelType[ply] = idx;
		OSDebugOut("Selected wheel type, player %d idx: %d\n", ply, idx);
	}
}

static void populateApiWidget(GtkComboBox *widget, int player, const std::string& device)
{
	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (widget)));

	auto dev = RegisterDevice::instance().Device( device );
	int port = 1 - player;
	if (dev)
	{
		std::string api;

		auto it = changedAPIs.find(std::make_pair(port, device));
		if (it == changedAPIs.end())
		{
			CONFIGVARIANT currAPI(N_DEVICE_API, CONFIG_TYPE_CHAR);
			LoadSetting(port, device, currAPI);
			api = currAPI.strValue;
			if (!dev->IsValidAPI(api))
				api = "";
		}
		else
			api = it->second;

		OSDebugOut("Current api: %s\n", api.c_str());
		apiCallback[player].api = api;
		int i = 0;
		for(auto& it : dev->APIs())
		{
			auto name = dev->LongAPIName(it);
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), name);
			if (api.size() && api == it)
				gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
			else
				gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
			i++;
		}
	}
}

static void deviceChanged (GtkComboBox *widget, gpointer data)
{
	gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	int player = (int)data;
	std::string s("");

	if (active > 0)
	{
		s = RegisterDevice::instance().Name(active - 1);
		auto dev = RegisterDevice::instance().Device(active - 1);
	}

	apiCallback[player].device = s;
	populateApiWidget(apiCallback[player].combo, player, s);

	if(player == 0)
		conf.Port1 = s;
	else
		conf.Port0 = s;

	OSDebugOut("Selected player %d idx: %d [%s]\n", player, active, s.c_str());
}

static void apiChanged (GtkComboBox *widget, gpointer data)
{
	int player = (int)data;
	gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	int port = 1 - player;

	auto& name = apiCallback[player].device;
	auto dev = RegisterDevice::instance().Device( name );
	if (dev)
	{
		auto apis = dev->APIs();
		auto it = apis.begin();
		std::advance(it, active);
		if (it != apis.end())
		{
			auto pair = std::make_pair(port, name);
			auto itAPI = changedAPIs.find(pair);

			if (itAPI != changedAPIs.end())
				itAPI->second = *it;
			else
				changedAPIs[pair] = *it;
			apiCallback[player].api = *it;

			OSDebugOut("selected api: %s\napi settings:\n", it->c_str());
			for(auto& p : dev->GetSettings(*it))
				OSDebugOut("\t[%s] %s = %s (%d)\n", it->c_str(), p.name, p.desc, p.type);
		}
	}
}

static void configureApi (GtkWidget *widget, gpointer data)
{
	int player = (int) data;
	int port = 1 - player;

	auto& name = apiCallback[player].device;
	auto& api = apiCallback[player].api;
	auto dev = RegisterDevice::instance().Device( name );

	OSDebugOut("configure api %s [%s] for player %d\n", api.c_str(), name.c_str(), player);
	if (dev)
	{
		GtkWidget *dlg = GTK_WIDGET (g_object_get_data (G_OBJECT(widget), "dlg"));
		int res = dev->Configure(port, api, dlg);
		OSDebugOut("Configure(...) returned %d\n", res);
	}
}

GtkWidget *new_combobox(const char* label, GtkWidget *vbox)
{
	GtkWidget *ro_label, *rs_hbox, *rs_label, *rs_cb;

	rs_hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), rs_hbox, FALSE, TRUE, 0);

	rs_label = gtk_label_new (label);
	gtk_box_pack_start (GTK_BOX (rs_hbox), rs_label, FALSE, TRUE, 5);
	gtk_label_set_justify (GTK_LABEL (rs_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (rs_label), 1, 0.5);

	rs_cb = gtk_combo_box_text_new ();
	gtk_box_pack_start (GTK_BOX (rs_hbox), rs_cb, TRUE, TRUE, 5);
	return rs_cb;
}

static GtkWidget* new_frame(const char *label, GtkWidget *box)
{
	GtkWidget *ro_frame = gtk_frame_new (NULL);
	gtk_box_pack_start (GTK_BOX (box), ro_frame, TRUE, FALSE, 0);

	GtkWidget *ro_label = gtk_label_new (label);
	gtk_frame_set_label_widget (GTK_FRAME (ro_frame), ro_label);
	gtk_label_set_use_markup (GTK_LABEL (ro_label), TRUE);

	GtkWidget *vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (ro_frame), vbox);
	return vbox;
}

#ifdef __cplusplus
extern "C" {
#endif

void CALLBACK USBconfigure() {

	LoadConfig();
	void * that = NULL;

	const char* wt[] = {"Driving Force / Generic", "Driving Force Pro", "GT Force"};
	const char *ports[] = {"Port 1:", "Port 2:"};

	GtkWidget *rs_cb, *vbox;
	uint32_t idx = 0, sel_idx = 0;

	// Create the dialog window
	GtkWidget *dlg = gtk_dialog_new_with_buttons (
		"Qemu USB Settings", NULL, GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable (GTK_WINDOW (dlg), TRUE);
	GtkWidget *dlg_area_box = gtk_dialog_get_content_area (GTK_DIALOG (dlg));
	GtkWidget *main_vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (dlg_area_box), main_vbox);

	/*** Device type ***/
	vbox = new_frame("Select device type:", main_vbox);

	std::string devs[2] = { conf.Port1, conf.Port0 };
	/*** Devices' Comboboxes ***/
	for(int ply = 0; ply < 2; ply++)
	{
		apiCallback[ply].device = devs[ply];

		rs_cb = new_combobox(ports[ply], vbox);
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), "None");
		gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), 0);

		auto devices = RegisterDevice::instance().Names();
		int idx = 0, selected = 0;
		for(auto& device : devices)
		{
			auto deviceProxy = RegisterDevice::instance().Device(device);
			auto name = deviceProxy->Name();
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), name );
			idx++;
			if (devs[ply] == device)
				gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), idx);
		}
		g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (deviceChanged), (gpointer)ply);
	}

	/*** APIs ***/
	vbox = new_frame("Select device API:", main_vbox);

	/*** API Comboboxes ***/
	for(int ply = 0; ply < 2; ply++)
	{
		rs_cb = new_combobox (ports[ply], vbox);
		apiCallback[ply].combo = GTK_COMBO_BOX(rs_cb);
		//gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), sel_idx);
		g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (apiChanged), (gpointer)ply);

		GtkWidget *hbox = gtk_widget_get_parent (rs_cb);
		GtkWidget *button = gtk_button_new_with_label ("Configure");
		gtk_button_set_image(GTK_BUTTON (button), gtk_image_new_from_icon_name ("gtk-preferences", GTK_ICON_SIZE_BUTTON));
		gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 5);

		g_signal_connect (button, "clicked", G_CALLBACK (configureApi), (gpointer)ply);
		g_object_set_data (G_OBJECT (button), "dlg", dlg);

		populateApiWidget(GTK_COMBO_BOX (rs_cb), ply, devs[ply]);
	}

	/** Wheel type **/
	vbox = new_frame("Wheel types:", main_vbox);

	for(int ply = 0; ply < 2; ply++)
	{
		rs_cb = new_combobox (ports[ply], vbox);

		sel_idx = 0;

		for (int i = 0; i < ARRAYSIZE(wt); i++)
		{
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), wt[i]);
			if(conf.WheelType[ply] == i)
				sel_idx = i;
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), sel_idx);
		g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (wheeltypeChanged), (gpointer)ply);
	}

	gtk_widget_show_all (dlg);

	// Modal loop
	gint result = gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);

	// Wait for all gtk events to be consumed ...
	while (gtk_events_pending ())
		gtk_main_iteration_do (FALSE);

	if (result == GTK_RESPONSE_OK)
		SaveConfig();
}

void CALLBACK USBabout() {
}

#ifdef __cplusplus
} //extern "C"
#endif

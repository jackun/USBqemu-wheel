#include "../../osdebugout.h"
#include "../usb-hid.h"
#include "evdev.h"
#include <linux/input.h>
#include <gtk/gtk.h>
#include <cstdio>
#include <sstream>

GtkWidget *new_combobox(const char* label, GtkWidget *vbox); // src/linux/config-gtk.cpp

namespace usb_hid { namespace evdev {

#define APINAME "hid_evdev"
#define EVDEV_DIR "/dev/input/by-path/"

struct ConfigData {
	int port;
	std::vector<std::string> devs;
	std::vector<std::string>::const_iterator iter;
};

static void PopulateHIDs(ConfigData &cfg, HIDType devtype)
{
	std::stringstream str;
	struct dirent* dp;
	const char* devstr[] = {"event-kbd", "event-mouse"};

	cfg.devs.clear();
	cfg.devs.push_back("None");

	DIR* dirp = opendir(EVDEV_DIR);
	if(dirp == NULL) {
		fprintf(stderr, "Error opening " EVDEV_DIR ": %s\n", strerror(errno));
		return;
	}

	// Loop over dir entries using readdir
	int len = strlen(devstr[devtype]);
	while((dp = readdir(dirp)) != NULL)
	{
		// Only select names that end in 'event-joystick'
		int devlen = strlen(dp->d_name);
		if(devlen >= len)
		{
			const char* const start = dp->d_name + devlen - len;
			if(strncmp(start, devstr[devtype], len) == 0) {
				OSDebugOut("%s%s\n", EVDEV_DIR, dp->d_name);

				str.clear(); str.str("");
				str << EVDEV_DIR << dp->d_name;

				char name[1024];
				std::string dev_path = str.str();
				if (!GetEvdevName(dev_path, name))
				{
					OSDebugOut("Name: %s\n", name);
					//XXX though it also could mean that controller is unusable
					cfg.devs.push_back(dev_path);//std::make_pair(dp->d_name, str.str()));
				}
				else
				{
					OSDebugOut("Failed to get name: %s\n", dev_path.c_str());
					cfg.devs.push_back(dev_path);//jsdata.push_back(std::make_pair(std::string(name) + " (evdev)", str.str()));
				}
			}
		}
	}
	closedir(dirp);
}

static void combo_changed (GtkComboBox *widget, gpointer data)
{
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	ConfigData *cfg = reinterpret_cast<ConfigData *> (data);

	if (!cfg)
		return;

	std::string &name = *(cfg->devs.begin() + idx);
	cfg->iter = (cfg->devs.begin() + idx);

	if (idx > 0)
	{
	}
	OSDebugOut("Selected player %d idx: %d dev: '%s'\n", 2 - cfg->port, idx, name.c_str());
}

int GtkHidConfigure(int port, HIDType type, GtkWindow *parent)
{
	GtkWidget *ro_frame, *ro_label, *rs_hbox, *rs_label, *rs_cb;
	GtkWidget *main_hbox, *right_vbox, *left_vbox;

	assert( (int)HIDTYPE_MOUSE == 1); //make sure there is atleast two types so we won't go beyond array length

	ConfigData cfg;
	cfg.port = port;

	PopulateHIDs(cfg, type);
	cfg.iter = cfg.devs.end();

	std::string path;
	LoadSetting(port, APINAME, n_device_by_type[type], path);

	// ---------------------------
	GtkWidget *dlg = gtk_dialog_new_with_buttons (
		"HID Evdev Settings", parent, GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
	gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable (GTK_WINDOW (dlg), TRUE);
	gtk_window_set_default_size (GTK_WINDOW(dlg), 320, 240);

	// ---------------------------
	GtkWidget *dlg_area_box = gtk_dialog_get_content_area (GTK_DIALOG (dlg));

	main_hbox = gtk_hbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER(dlg_area_box), main_hbox);

//	left_vbox = gtk_vbox_new (FALSE, 5);
//	gtk_box_pack_start (GTK_BOX (main_hbox), left_vbox, TRUE, TRUE, 5);
	right_vbox = gtk_vbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (main_hbox), right_vbox, TRUE, TRUE, 5);

	// ---------------------------
	rs_cb = new_combobox ("Device:", right_vbox);

	int idx = 0, sel_idx = 0;
	for (auto& it : cfg.devs)
	{
		/*std::stringstream str;
		str << it.first;
		if (!it.second.empty())
			str << " [" << it.second << "]";*/

		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), it.c_str());
		if (!path.empty() && it == path)
		{
			sel_idx = idx;
		}
		idx++;
	}

	//g_object_set_data (G_OBJECT (rs_cb), CFG, &cfg);
	g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (combo_changed), reinterpret_cast<gpointer> (&cfg));
	gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), sel_idx);

	// ---------------------------
	gtk_widget_show_all (dlg);
	gint result = gtk_dialog_run (GTK_DIALOG (dlg));

	int ret = RESULT_OK;
	if (result == GTK_RESPONSE_OK)
	{
		if (cfg.iter != cfg.devs.end()) {
			if (!SaveSetting(port, APINAME, n_device_by_type[type], *cfg.iter))
				ret = RESULT_FAILED;
		}
	}
	else
		ret = RESULT_CANCELED;

	gtk_widget_destroy (dlg);
	return ret;
}

int EvDev::Configure(int port, HIDType type, void *data)
{
	return GtkHidConfigure(port, type, GTK_WINDOW (data));
}

#undef APINAME
#undef EVDEV_DIR
}} //namespace
#include "../USB.h"
#include "../usb-pad/config.h"

#include <cstdlib>
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

#include "ini.h"

//libjoyrumble used as an example
//Hopefully PCSX2 has inited all the GTK stuff already
using namespace std;

typedef vector< pair<string,string> > vstring;
vstring jsdata;
GtkWidget *mDialog;
GtkWidget *mOKButton;
static char usb_path[PATH_MAX];
std::string IniDir;
std::string LogDir;

void SysMessage(const char *fmt, ...)
{
	va_list arglist;

	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);
}

void CALLBACK USBsetSettingsDir( const char* dir )
{
	fprintf(stderr, "USBsetSettingsDir: %s\n", dir);
	IniDir = dir;
}

void CALLBACK USBsetLogDir( const char* dir )
{
	printf("USBsetLogDir: %s\n", dir);
	LogDir = dir;
}

void SaveConfig() {
	fprintf(stderr, "USB save config\n");
	//char* envptr = getenv("HOME");
	//if(envptr == NULL)
	//	return;
	//char path[1024];
	//snprintf(path, sizeof(path), "%s/.config/PCSX2/inis/USBqemu-wheel.ini", envptr);
	std::string iniPath(IniDir);
	iniPath.append("USBqemu-wheel.ini");
	const char *path = iniPath.c_str();

	//fprintf(stderr, "%s\n", path);
	snprintf(conf.usb_img, sizeof(conf.usb_img), "%s", usb_path);

	INISaveUInt(path, "Devices", "Port 0", conf.Port0);
	INISaveUInt(path, "Devices", "Port 1", conf.Port1);
	INISaveUInt(path, "Devices", "Wheel type 0", conf.WheelType[0]);
	INISaveUInt(path, "Devices", "Wheel type 1", conf.WheelType[1]);
	INISaveString(path, "Devices", "USB Image", conf.usb_img);
	INISaveString(path, "Joystick", "Player1", player_joys[0].c_str());
	INISaveString(path, "Joystick", "Player2", player_joys[1].c_str());
}

void LoadConfig() {
	char joy[1024] = {0};
	fprintf(stderr, "USB load config\n");
	//char* envptr = getenv("HOME");
	//if(envptr == NULL)
	//	return;
	//char path[1024];
	//sprintf(path, "%s/.config/PCSX2/inis/USBqemu-wheel.ini", envptr);
	std::string iniPath(IniDir);
	iniPath.append("USBqemu-wheel.ini");
	const char *path = iniPath.c_str();

	INILoadString(path, "Joystick", "Player1", joy);
	player_joys[0] = string(joy);
	INILoadString(path, "Joystick", "Player2", joy);
	player_joys[1] = string(joy);

	INILoadUInt(path, "Devices", "Port 0", (u32*)&conf.Port0);
	INILoadUInt(path, "Devices", "Port 1", (u32*)&conf.Port1);
	INILoadUInt(path, "Devices", "Wheel type 0", (u32*)&conf.WheelType[0]);
	INILoadUInt(path, "Devices", "Wheel type 1", (u32*)&conf.WheelType[1]);
	INILoadString(path, "Devices", "USB Image", conf.usb_img);
}

static void joystickChanged (GtkComboBox *widget, gpointer data)
{
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	int ply = (int)data;
	string devPath = (jsdata.begin() + idx)->second;
	player_joys[ply] = devPath;
	fprintf(stderr, "Selected player %d idx: %d dev: '%s'\n", ply, idx, devPath.c_str());
}

static void wheeltypeChanged (GtkComboBox *widget, gpointer data)
{
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	//if(data)
	{
		uint8_t port = MIN((unsigned)data, 1);

		conf.WheelType[port] = idx;
		fprintf(stderr, "Selected wheel type, player %d idx: %d\n", port, idx);
	}
}

static void entryChanged( GtkWidget *widget, gpointer data)
{
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(widget));
	//fprintf(stderr, "Entry text:%s\n", text);
	//snprintf(conf.usb_img, sizeof(conf.usb_img), "%s", text);
	snprintf(usb_path, sizeof(usb_path), "%s", text);
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

static int DevToIdx(int dev)
{
	switch(dev) {
		case 0:
		case 1:
			return dev;
		case 3:
			return 2;
		default:
		break;
	}
	return 0;
}

#define PORT_DEVICE(x,y) if(x==0) conf.Port1 = y; else conf.Port0 = y

static void deviceChanged (GtkComboBox *widget, gpointer data)
{
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

	int ply = (int)data;
	switch(idx){
		case 0:
		case 1:
			PORT_DEVICE(ply,idx);
			break;
		case 2: //skip wheel type
			PORT_DEVICE(ply,idx+1);
			break;
		default:
			fprintf(stderr, "Invalid device, player %d idx: %d\n", ply, idx);
			break;
	}
	fprintf(stderr, "Selected player %d idx: %d\n", ply, idx);
}

bool file_exists(string filename)
{
	FILE *i = fopen(filename.c_str(), "r");

	if (i == NULL)
		return false;

	fclose(i);
	return true;
}

bool dir_exists(string filename)
{
	DIR *i = opendir(filename.c_str());

	if (i == NULL)
	return false;

	closedir(i);
	return true;
}

static GtkWidget *new_combobox(const char* label, GtkWidget *ro_frame, GtkWidget *vbox)
{
	GtkWidget *ro_label, *rs_hbox, *rs_label, *rs_cb;

	rs_hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), rs_hbox, FALSE, TRUE, 0);

	rs_label = gtk_label_new (label);
	gtk_widget_show (rs_label);
	gtk_box_pack_start (GTK_BOX (rs_hbox), rs_label, FALSE, TRUE, 5);
	gtk_label_set_justify (GTK_LABEL (rs_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (rs_label), 1, 0.5);

	rs_cb = gtk_combo_box_text_new ();
	gtk_widget_show (rs_cb);
	gtk_box_pack_start (GTK_BOX (rs_hbox), rs_cb, TRUE, TRUE, 5);
	gtk_widget_show (rs_hbox);
	return rs_cb;
}

#ifdef __cplusplus
extern "C" {
#endif

void CALLBACK USBconfigure() {

	LoadConfig();
	void * that = NULL;
	jsdata.clear();
	jsdata.push_back(make_pair("None", ""));
	//jsdata.push_back(make_pair("Fake Vendor FakePad", "Fake"));

	const char* wt[] = {"Driving Force / Generic", "Driving Force Pro", "GT Force"};
	const char *ports[] = {"Port 1:", "Port 2:"};

{
	int count=0, j=0, fd=0;
	stringstream str;
	for (count = 0; count < MAX_JOYS; count++)
	{
		str.clear();
		str.str(""); str << "/dev/input/js" << count;
		/* Check if joystick device exists */
		if (file_exists(str.str())){


					char name_c_str[1024];
					if ((fd = open(str.str().c_str(), O_RDONLY)) < 0)
					{
						fprintf(stderr, "Cannot open %s\n", str.str().c_str());
					}
					else
					{
						if (ioctl(fd, JSIOCGNAME(sizeof(name_c_str)), name_c_str) < -1)
						{
							fprintf(stderr, "Cannot get controller's name\n");
							//XXX though it also could mean that controller is unusable
							jsdata.push_back(make_pair(str.str(), str.str()));
						}
						else
						{
							jsdata.push_back(make_pair(string(name_c_str), str.str()));
						}
						close(fd);
					}

					#ifdef _DEBUG
						fprintf(stderr, "%s\n", str.str().c_str());
					#endif
		}
	}
}

	GtkWidget *ro_frame, *ro_label, *rs_hbox, *rs_label, *rs_cb, *vbox;
	uint32_t idx = 0, sel_idx = 0;

	// Create the dialog window
	mDialog = gtk_dialog_new_with_buttons (
		"Qemu USB Settings", NULL, GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		NULL);
	mOKButton = gtk_dialog_add_button (GTK_DIALOG (mDialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_window_set_position (GTK_WINDOW (mDialog), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable (GTK_WINDOW (mDialog), TRUE);
	//gtk_widget_show (mDialog);
	GtkWidget *dlg_area_box = gtk_dialog_get_content_area (GTK_DIALOG (mDialog));

	/*** Device type ***/
	ro_frame = gtk_frame_new (NULL);
	gtk_widget_show (ro_frame);
	gtk_box_pack_start (GTK_BOX (dlg_area_box), ro_frame, TRUE, FALSE, 0);

	ro_label = gtk_label_new ("Select device type:");
	gtk_widget_show (ro_label);
	gtk_frame_set_label_widget (GTK_FRAME (ro_frame), ro_label);
	gtk_label_set_use_markup (GTK_LABEL (ro_label), TRUE);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (ro_frame), vbox);
	gtk_widget_show (vbox);

	/*** Devices' Comboboxes ***/
	for(int ply = 0; ply < 2; ply++)
	{
		rs_cb = new_combobox(ports[ply], ro_frame, vbox);
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), "None");
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), "Wheel");
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), "USB Mass storage");
		//gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), "Singstar");
		gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), DevToIdx(ply == 0 ? conf.Port1 : conf.Port0));
		g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (deviceChanged), (ptrdiff_t*)ply);
	}

	rs_cb = NULL;

	/*** Joysticks ***/
	ro_frame = gtk_frame_new (NULL);
	gtk_widget_show (ro_frame);
	gtk_box_pack_start (GTK_BOX (dlg_area_box), ro_frame, TRUE, FALSE, 0);

	ro_label = gtk_label_new ("Select joysticks:");
	gtk_widget_show (ro_label);
	gtk_frame_set_label_widget (GTK_FRAME (ro_frame), ro_label);
	gtk_label_set_use_markup (GTK_LABEL (ro_label), TRUE);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (ro_frame), vbox);
	gtk_widget_show (vbox);

	/*** Joysticks' Comboboxes ***/
	for(int ply = 0; ply < 2; ply++)
	{
		rs_cb = new_combobox (ports[ply], ro_frame, vbox);
		//g_object_set_data (G_OBJECT (ro_cb), "joystick-option", ro_label);

		idx = 0; sel_idx = 0;

		for (vstring::const_iterator r = jsdata.begin(); r != jsdata.end (); r++, idx++)
		{
			stringstream str;
			str << r->first <<  " [" << r->second << "]";
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), str.str().c_str ());
			if(r->second == player_joys[ply])
				sel_idx = idx;
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), sel_idx);
		g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (joystickChanged), (ptrdiff_t*)ply);
	}

	/** Wheel type **/
	ro_frame = gtk_frame_new (NULL);
	gtk_widget_show (ro_frame);
	gtk_box_pack_start (GTK_BOX (dlg_area_box), ro_frame, TRUE, FALSE, 0);

	ro_label = gtk_label_new ("Wheel types:");
	gtk_widget_show (ro_label);
	gtk_frame_set_label_widget (GTK_FRAME (ro_frame), ro_label);
	gtk_label_set_use_markup (GTK_LABEL (ro_label), TRUE);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (ro_frame), vbox);
	gtk_widget_show (vbox);

	for(int ply = 0; ply < 2; ply++)
	{

		rs_cb = new_combobox (ports[ply], ro_frame, vbox);

		sel_idx = 0;

		for (int i = 0; i < ARRAYSIZE(wt); i++)
		{
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), wt[i]);
			if(conf.WheelType[ply] == i)
				sel_idx = i;
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), sel_idx);
		g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (wheeltypeChanged), (ptrdiff_t*)(ply));
	}

	/*** Mass storage ***/
	ro_frame = gtk_frame_new (NULL);
	gtk_widget_show (ro_frame);
	gtk_box_pack_start (GTK_BOX (dlg_area_box), ro_frame, TRUE, FALSE, 5);

	ro_label = gtk_label_new ("Select USB image:");
	gtk_widget_show (ro_label);
	gtk_frame_set_label_widget (GTK_FRAME (ro_frame), ro_label);
	gtk_label_set_use_markup (GTK_LABEL (ro_label), TRUE);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (ro_frame), vbox);
	gtk_widget_show (vbox);

	rs_hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), rs_hbox, FALSE, TRUE, 0);

	GtkWidget *entry = gtk_entry_new ();
	gtk_entry_set_max_length (GTK_ENTRY (entry), sizeof(conf.usb_img));
	gtk_widget_show (entry);
	gtk_entry_set_text(GTK_ENTRY(entry), conf.usb_img);
	g_signal_connect (entry, "changed", G_CALLBACK (entryChanged), NULL);

	GtkWidget *button = gtk_button_new_with_label ("Browse");
	gtk_button_set_image(GTK_BUTTON (button), gtk_image_new_from_icon_name ("gtk-open", GTK_ICON_SIZE_BUTTON));
	gtk_widget_show(button);
	g_signal_connect (button, "clicked", G_CALLBACK (fileChooser), entry);

	gtk_box_pack_start (GTK_BOX (rs_hbox), entry, TRUE, TRUE, 5);
	gtk_box_pack_start (GTK_BOX (rs_hbox), button, FALSE, FALSE, 5);
	gtk_widget_show (rs_hbox);

	// Modal loop
	gint result = gtk_dialog_run (GTK_DIALOG (mDialog));
	gtk_widget_destroy (mDialog);

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


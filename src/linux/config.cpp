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
int id1 = 0, id2 = 1;

void SysMessage(char *fmt, ...)
{
	va_list arglist;

	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);
}

void SaveConfig() {
	fprintf(stderr, "USB save config\n");
	char* envptr = getenv("HOME");
	if(envptr == NULL)
		return;
	char path[1024];
	sprintf(path, "%s/.config/pcsx2/inis/USBqemu-wheel.ini", envptr);
	//fprintf(stderr, "%s\n", path);

	INISaveString(path, "Joystick", "Player1", player_joys[0].c_str());
	INISaveString(path, "Joystick", "Player2", player_joys[1].c_str());
}

void LoadConfig() {
	fprintf(stderr, "USB load config\n");
	char* envptr = getenv("HOME");
	if(envptr == NULL)
		return;
	char path[1024];
	char joy[1024] = {0};
	sprintf(path, "%s/.config/pcsx2/inis/USBqemu-wheel.ini", envptr);
	INILoadString(path, "Joystick", "Player1", joy);
	player_joys[0] = string(joy);
	INILoadString(path, "Joystick", "Player2", joy);
	player_joys[1] = string(joy);
}

void joystickChanged (GtkComboBox *widget, gpointer data)
{
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	if(data)
	{
		int ply = *((int*)data);
		string devPath = (jsdata.begin() + idx)->second;
		player_joys[ply] = devPath;
		fprintf(stderr, "Selected player %d idx: %d dev: '%s'\n", ply, idx, devPath.c_str());
	}
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

#ifdef __cplusplus
extern "C" {
#endif

void CALLBACK USBconfigure() {

	LoadConfig();
	void * that = NULL;
	jsdata.clear();
	jsdata.push_back(make_pair("None", ""));
	//jsdata.push_back(make_pair("Fake Vendor FakePad", "Fake"));

{
	int count=0, j=0, fd=0;
	stringstream str, event;
	for (count = 0; count < MAX_JOYS; count++)
	{
		str.clear();
		str.str(""); str << "/dev/input/js" << count;
		/* Check if joystick device exists */
		if (file_exists(str.str())){

			for (j = 0; j <= 99; j++)
			{
				event.clear(); event.str(string());
				/* Try to discover the corresponding event number */
				event << "/sys/class/input/js" << count << "/device/event" << j;
				if (dir_exists(event.str())){

					event.clear(); event.str(string());
					event << "/dev/input/event" << j;

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
						fprintf(stderr, str.str());
						fprintf(stderr, "\n");
					#endif
				}
			}
		}
	}
}

	// Create the dialog window
	mDialog = gtk_dialog_new_with_buttons (
		"Settings", NULL, GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		NULL);
	mOKButton = gtk_dialog_add_button (GTK_DIALOG (mDialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_window_set_position (GTK_WINDOW (mDialog), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable (GTK_WINDOW (mDialog), FALSE);
	gtk_widget_show (GTK_DIALOG (mDialog)->vbox);


	GtkWidget *ro_frame = gtk_frame_new (NULL);
	gtk_widget_show (ro_frame);
	//gtk_box_pack_start (GTK_BOX (vbox), ro_frame, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (mDialog)->vbox), ro_frame, TRUE, TRUE, 0);

	GtkWidget *ro_label = gtk_label_new ("Select joysticks:");
	gtk_widget_show (ro_label);
	gtk_frame_set_label_widget (GTK_FRAME (ro_frame), ro_label);
	gtk_label_set_use_markup (GTK_LABEL (ro_label), TRUE);

	GtkWidget *vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (ro_frame), vbox);
	gtk_widget_show (vbox);


	/*** PLAYER 1 ***/
	GtkWidget *rs_hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), rs_hbox, FALSE, TRUE, 0);

	GtkWidget *rs_label = gtk_label_new ("Player 1:");
	gtk_widget_show (rs_label);
	gtk_box_pack_start (GTK_BOX (rs_hbox), rs_label, TRUE, TRUE, 5);
	gtk_label_set_justify (GTK_LABEL (rs_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (rs_label), 1, 0.5);

	GtkWidget *rs_cb = gtk_combo_box_new_text ();
	gtk_widget_show (rs_cb);
	gtk_box_pack_start (GTK_BOX (rs_hbox), rs_cb, TRUE, TRUE, 5);
	g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (joystickChanged), &id1);
	//g_object_set_data (G_OBJECT (ro_cb), "joystick-option", ro_label);

	uint32_t idx = 0, sel_idx = 0;

	for (vstring::const_iterator r = jsdata.begin(); r != jsdata.end (); r++, idx++)
	{
		stringstream str;
		str << r->first <<  " [" << r->second << "]";
		gtk_combo_box_append_text (GTK_COMBO_BOX (rs_cb), str.str().c_str ());
		if(r->second == player_joys[0])
			sel_idx = idx;
	}

	gtk_widget_show (rs_hbox);
	gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), sel_idx);

	/*** PLAYER 2 ***/
	rs_hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), rs_hbox, FALSE, TRUE, 0);

	rs_label = gtk_label_new ("Player 2:");
	gtk_widget_show (rs_label);
	gtk_box_pack_start (GTK_BOX (rs_hbox), rs_label, TRUE, TRUE, 5);
	gtk_label_set_justify (GTK_LABEL (rs_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (rs_label), 1, 0.5);

	rs_cb = gtk_combo_box_new_text ();
	gtk_widget_show (rs_cb);
	gtk_box_pack_start (GTK_BOX (rs_hbox), rs_cb, TRUE, TRUE, 5);

	g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (joystickChanged), &id2);

	idx = 0, sel_idx = 0;

	for (vstring::const_iterator r = jsdata.begin(); r != jsdata.end (); r++, idx++)
	{
		stringstream str;
		str << r->first <<  " [" << r->second << "]";
		gtk_combo_box_append_text (GTK_COMBO_BOX (rs_cb), str.str().c_str ());
		if(r->second == player_joys[1])
			sel_idx = idx;
	}

	gtk_widget_show (rs_hbox);
	gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), sel_idx);

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


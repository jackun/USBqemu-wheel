#include "joydev.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <math.h>
#include <gtk/gtk.h>

typedef std::vector< std::pair<std::string, std::string> > vstring;
static vstring jsdata;
GtkWidget *new_combobox(const char* label, GtkWidget *vbox);

#if _DEBUG
#define Dbg(...) fprintf(stderr, __VA_ARGS__)
#else
#define Dbg(...)
#endif

#define APINAME "joydev"

bool file_exists(std::string path)
{
	FILE *i = fopen(path.c_str(), "r");

	if (i == NULL)
		return false;

	fclose(i);
	return true;
}

bool dir_exists(std::string path)
{
	DIR *i = opendir(path.c_str());

	if (i == NULL)
	return false;

	closedir(i);
	return true;
}

static void PopulateJoysticks()
{
	jsdata.clear();
	jsdata.push_back(std::make_pair("None", ""));
	//jsdata.push_back(make_pair("Fake Vendor FakePad", "Fake"));

	int count=0, j=0, fd=0;
	std::stringstream str;
	for (count = 0; count < MAX_JOYS; count++)
	{
		str.clear();
		str.str(""); str << "/dev/input/js" << count;
		/* Check if joystick device exists */
		if (file_exists(str.str()))
		{
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
					jsdata.push_back(std::make_pair(str.str(), str.str()));
				}
				else
				{
					jsdata.push_back(std::make_pair(std::string(name_c_str), str.str()));
				}
				close(fd);
			}

			#ifdef _DEBUG
				fprintf(stderr, "%s\n", str.str().c_str());
			#endif
		}
	}
}

static void joystickChanged (GtkComboBox *widget, gpointer data)
{
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	int ply = (int)data;
	std::string devPath = (jsdata.begin() + idx)->second;
	//joysticks[ply] = devPath;"
	fprintf(stderr, "Selected player %d idx: %d dev: '%s'\n", ply, idx, devPath.c_str());
}

int JoyDevPad::Configure(int port, void *data)
{
	GtkWidget *ro_frame, *ro_label, *rs_hbox, *rs_label, *rs_cb, *vbox;

	PopulateJoysticks();

	GtkWidget *dlg = gtk_dialog_new_with_buttons (
		"Joydev Settings", NULL, GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		NULL);
	GtkWidget *okbtn = gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable (GTK_WINDOW (dlg), TRUE);
	//gtk_widget_show (dlg);
	GtkWidget *dlg_area_box = gtk_dialog_get_content_area (GTK_DIALOG (dlg));

	GtkWidget *vboxes[2] = { 0 };

	for(int ply = 0; ply < 2; ply++)
	{
		std::string path;
		CONFIGVARIANT var(N_CONFIG_JOY, CONFIG_TYPE_CHAR);
		if(LoadSetting(1 - ply, APINAME, var))
			path = var.strValue;

		vboxes[ply] = gtk_vbox_new (FALSE, 5);

		vbox = gtk_vbox_new (FALSE, 5);
		gtk_container_add (GTK_CONTAINER(vboxes[ply]), vbox);

		rs_cb = new_combobox ("Joystick", vbox);
		//g_object_set_data (G_OBJECT (ro_cb), "joystick-option", ro_label);

		int idx = 0, sel_idx = 0;

		for (vstring::const_iterator it = jsdata.begin(); it != jsdata.end (); it++, idx++)
		{
			std::stringstream str;
			str << it->first;
			if (!it->second.empty())
				str << " [" << it->second << "]";

			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), str.str().c_str ());
			if (!path.empty() && it->second == path)
				sel_idx = idx;
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), sel_idx);
		g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (joystickChanged), (gpointer)ply);
	}

	// Handle some nice tab
	GtkWidget* notebook = gtk_notebook_new();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vboxes[0] , gtk_label_new("Player One"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vboxes[1] , gtk_label_new("Player Two"));
	//gtk_notebook_append_page(GTK_NOTEBOOK(notebook), ply_one_box , gtk_label_new("Global Settings"));
	//gtk_notebook_append_page(GTK_NOTEBOOK(notebook), ply_two_box, gtk_label_new("Advanced Settings"));
	//gtk_container_add(GTK_CONTAINER(main_box), notebook);
	gtk_container_add (GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), notebook);

	gtk_widget_show_all (dlg);
	gint result = gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);

	return RESULT_CANCELED;
}

#undef APINAME
#undef Dbg

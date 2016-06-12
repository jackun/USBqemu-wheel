#include "joydev.h"
#include "../../osdebugout.h"

#include <chrono>
#include <thread>
#include <stdio.h>
#include <sstream>
#include <gtk/gtk.h>

using sys_clock = std::chrono::system_clock;
using ms = std::chrono::milliseconds;

typedef std::vector< std::pair<std::string, std::string> > vstring;
static vstring jsdata;
GtkWidget *new_combobox(const char* label, GtkWidget *vbox);

struct Point { int x; int y; JoyDevMap type; };

struct ConfigData
{
	std::vector<uint16_t> mappings;
	vstring::const_iterator pathIt;
	GtkWidget *label;
	GtkListStore *store;
};

enum
{
	COL_PS2 = 0,
	COL_PC,
	NUM_COLS
};

#define JOYTYPE "joytype"
#define CFG "cfg"
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
	//jsdata.push_back(std::make_pair("Fake Vendor FakePad", "/dev/null"));

	std::stringstream str;
	for (int count = 0; count < MAX_JOYS; count++)
	{
		str.clear();
		str.str(""); str << "/dev/input/js" << count;
		/* Check if joystick device exists */
		if (file_exists(str.str()))
		{
			char name[1024];
			if (!GetJoystickName(str.str(), name))
			{
				//XXX though it also could mean that controller is unusable
				jsdata.push_back(std::make_pair(str.str(), str.str()));
			}
			else
			{
				jsdata.push_back(std::make_pair(std::string(name), str.str()));
			}
		}
	}
}

static bool PollInput(const std::string &joypath, bool isaxis, int& value)
{
	int fd;
	ssize_t len;
	struct js_event event;

	if ((fd = open(joypath.c_str(), O_RDONLY | O_NONBLOCK)) < 0)
	{
		OSDebugOut("Cannot open joystick: %s\n", joypath.c_str());
		return false;
	}

	// empty event queue
	while ((len = read(fd, &event, sizeof(event))) > 0);

	struct AxisValue { int16_t value; bool initial; };
	AxisValue axisVal[ABS_MAX + 1] = { 0 };

	auto last = sys_clock::now();
	//Non-blocking read sets len to -1 and errno to EAGAIN if no new data
	while (true)
	{
		auto dur = std::chrono::duration_cast<ms>(sys_clock::now()-last).count();
		if (dur > 5000) goto error;

		if ((len = read(fd, &event, sizeof(event))) > -1 && (len == sizeof(event)))
		{
			if (isaxis && event.type == JS_EVENT_AXIS)
			{
				auto& val = axisVal[event.number];

				if (!val.initial)
				{
					val.value = event.value;
					val.initial = true;
				}
				else if (std::abs(event.value - val.value) > 1000)
				{
					value = event.number;
					break;
				}
			}
			else if (!isaxis && event.type == JS_EVENT_BUTTON)
			{
				if (event.value)
				{
					value = event.number;
					break;
				}
			}
		}
		else if (errno != EAGAIN)
		{
			OSDebugOut("PollInput: read error %d\n", errno);
			goto error;
		}
		else
		{
			while (gtk_events_pending ())
				gtk_main_iteration_do (FALSE);
			std::this_thread::sleep_for(ms(1));
		}
	}

	close(fd);
	return true;
error:
	close(fd);
	return false;
}

static void RefreshStore(ConfigData *cfg)
{
	GtkTreeIter iter;

	gtk_list_store_clear (cfg->store);
	for (int i = 0; i < JOY_MAPS_COUNT && i < cfg->mappings.size(); i++)
	{
		if (cfg->mappings[i] == (uint16_t)-1)
			continue;

		gtk_list_store_append (cfg->store, &iter);
		gtk_list_store_set (cfg->store, &iter,
			COL_PS2, JoyDevMapNames[i],
			COL_PC, cfg->mappings[i],
			-1);
	}
}

static void joystickChanged (GtkComboBox *widget, gpointer data)
{
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	int port = (int) data;
	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);

	if (!cfg)
		return;

	std::string name = (jsdata.begin() + idx)->first;
	cfg->pathIt = (jsdata.begin() + idx);

	if (idx > 0)
	{
		LoadMappings(port, name, cfg->mappings);
		RefreshStore(cfg);
	}
	OSDebugOut("Selected player %d idx: %d dev: '%s'\n", 2 - port, idx, name.c_str());
}

static void buttonClicked (GtkComboBox *widget, gpointer data)
{
	int port = (int)data;
	int type = (int)g_object_get_data (G_OBJECT (widget), JOYTYPE);
	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);

	if (cfg && type < cfg->mappings.size() && cfg->pathIt != jsdata.end())
	{
		int value;
		bool isaxis = (type >= JOY_STEERING && type <= JOY_BRAKE);
		gtk_label_set_text (GTK_LABEL (cfg->label), "Polling for input...");
		OSDebugOut("%s isaxis:%d %s\n" , cfg->pathIt->second.c_str(), isaxis, JoyDevMapNames[type]);

		// let label change its text
		while (gtk_events_pending ())
			gtk_main_iteration_do (FALSE);

		if (PollInput(cfg->pathIt->second, isaxis, value))
		{
			cfg->mappings[type] = value;
			RefreshStore(cfg);
		}
		gtk_label_set_text (GTK_LABEL (cfg->label), "");
	}
}

static void clearAllClicked (GtkComboBox *widget, gpointer data)
{
	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);
	auto& m = cfg->mappings;
	m.assign(JOY_MAPS_COUNT, -1);
	RefreshStore(cfg);
}

int JoyDevPad::Configure(int port, void *data)
{
	GtkWidget *ro_frame, *ro_label, *rs_hbox, *rs_label, *rs_cb;
	GtkWidget *main_hbox, *right_vbox, *left_vbox, *treeview;
	GtkWindow *parent = GTK_WINDOW (data);

	PopulateJoysticks();
	std::string path;
	{
		CONFIGVARIANT var(N_JOYSTICK, CONFIG_TYPE_CHAR);
		if (LoadSetting(port, APINAME, var))
			path = var.strValue;
	}

	ConfigData cfg;
	cfg.pathIt = jsdata.end();
	cfg.label = gtk_label_new ("");
	cfg.store = gtk_list_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_UINT);

	// ---------------------------
	std::string title = (port ? "Player One " : "Player Two ");
	title += "Joydev Settings";

	GtkWidget *dlg = gtk_dialog_new_with_buttons (
		title.c_str(), parent, GTK_DIALOG_MODAL,
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

	left_vbox = gtk_vbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (main_hbox), left_vbox, TRUE, TRUE, 5);
	right_vbox = gtk_vbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (main_hbox), right_vbox, TRUE, TRUE, 5);

	// ---------------------------
	treeview = gtk_tree_view_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
		-1,
		"PS2",
		gtk_cell_renderer_text_new (),
		"text", COL_PS2,
		NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
		-1,
		"PC",
		gtk_cell_renderer_text_new (),
		"text", COL_PC,
		NULL);

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (cfg.store));
	g_object_unref (GTK_TREE_MODEL (cfg.store)); //treeview has its own ref
	gtk_box_pack_start (GTK_BOX (left_vbox), treeview, TRUE, TRUE, 5);

	// ---------------------------
	rs_cb = new_combobox ("Joystick:", right_vbox);

	int idx = 0, sel_idx = 0;

	for (vstring::const_iterator it = jsdata.begin(); it != jsdata.end (); it++, idx++)
	{
		std::stringstream str;
		str << it->first;
		if (!it->second.empty())
			str << " [" << it->second << "]";

		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), str.str().c_str ());
		if (!path.empty() && it->second == path)
		{
			sel_idx = idx;
			if (idx > 0)
			{
				LoadMappings (port, it->first, cfg.mappings);
				RefreshStore(&cfg);
			}
		}
	}

	g_object_set_data (G_OBJECT (rs_cb), CFG, &cfg);
	g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (joystickChanged), (gpointer)port);
	gtk_combo_box_set_active (GTK_COMBO_BOX (rs_cb), sel_idx);

	// Remapping
	{
		GtkWidget* table = gtk_table_new (5, 8, true);
		gtk_container_add (GTK_CONTAINER(right_vbox), table);
		GtkAttachOptions opt = (GtkAttachOptions)(GTK_EXPAND | GTK_FILL); // default

		const char* button_labels[] = {
			"L2", "L1", "R2", "R1",
			"Left", "Up", "Right", "Down",
			"Square", "Cross", "Circle", "Triangle",
			"Select", "Start",
		};

		const Point button_pos[] = {
			{1, 0, JOY_L2}, {1, 1, JOY_L1}, {6, 0, JOY_R2}, {6, 1, JOY_R1},
			{0, 3, JOY_LEFT}, {1, 2, JOY_UP}, {2, 3, JOY_RIGHT}, {1, 4, JOY_DOWN},
			{5, 3, JOY_SQUARE}, {6, 4, JOY_CROSS}, {7, 3, JOY_CIRCLE}, {6, 2, JOY_TRIANGLE},
			{3, 3, JOY_SELECT}, {4, 3, JOY_START},
		};

		for (int i=0; i<ARRAY_SIZE(button_labels); i++)
		{
			GtkWidget *button = gtk_button_new_with_label (button_labels[i]);
			g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (buttonClicked), (gpointer)port);

			g_object_set_data (G_OBJECT (button), JOYTYPE, (gpointer)button_pos[i].type);
			g_object_set_data (G_OBJECT (button), CFG, &cfg);

			gtk_table_attach (GTK_TABLE (table), button,
					0 + button_pos[i].x, 1 + button_pos[i].x,
					0 + button_pos[i].y, 1 + button_pos[i].y,
					opt, opt, 5, 1);
		}

		GtkWidget *button;
		GtkWidget *hbox = gtk_hbox_new (false, 5);
		gtk_container_add (GTK_CONTAINER (right_vbox), hbox);

		button = gtk_button_new_with_label ("Steering");
		gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
		g_object_set_data (G_OBJECT (button), JOYTYPE, (gpointer)JOY_STEERING);
		g_object_set_data (G_OBJECT (button), CFG, &cfg);
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (buttonClicked), (gpointer)port);

		button = gtk_button_new_with_label ("Throttle");
		gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
		g_object_set_data (G_OBJECT (button), JOYTYPE, (gpointer)JOY_THROTTLE);
		g_object_set_data (G_OBJECT (button), CFG, &cfg);
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (buttonClicked), (gpointer)port);

		button = gtk_button_new_with_label ("Brake");
		gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
		g_object_set_data (G_OBJECT (button), JOYTYPE, (gpointer)JOY_BRAKE);
		g_object_set_data (G_OBJECT (button), CFG, &cfg);
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (buttonClicked), (gpointer)port);

		gtk_box_pack_start (GTK_BOX (right_vbox), cfg.label, TRUE, TRUE, 5);

		button = gtk_button_new_with_label ("Clear All");
		gtk_box_pack_start (GTK_BOX (right_vbox), button, TRUE, TRUE, 5);
		g_object_set_data (G_OBJECT (button), CFG, &cfg);
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (clearAllClicked), (gpointer)port);
	}

	// ---------------------------
	gtk_widget_show_all (dlg);
	gint result = gtk_dialog_run (GTK_DIALOG (dlg));

	OSDebugOut("mappings %d\n", cfg.mappings.size());

	int ret = RESULT_OK;
	if (result == GTK_RESPONSE_OK)
	{
		if (cfg.pathIt != jsdata.end()) {
			CONFIGVARIANT var(N_JOYSTICK, cfg.pathIt->second);
			if(!SaveSetting(port, APINAME, var))
				ret = RESULT_FAILED;

			if (cfg.pathIt != jsdata.begin())
				SaveMappings(port, cfg.pathIt->first, cfg.mappings);
		}
	}
	else
		ret = RESULT_CANCELED;

	gtk_widget_destroy (dlg);
	return ret;
}

#undef APINAME

#include "shared.h"
#include "../../osdebugout.h"

#include <chrono>
#include <thread>
#include <stdio.h>
#include <sstream>

using sys_clock = std::chrono::system_clock;
using ms = std::chrono::milliseconds;

#define JOYTYPE "joytype"
#define CFG "cfg"

bool LoadMappings(int port, const std::string& joyname, std::vector<uint16_t>& mappings, bool (&inverted)[3])
{
	assert(JOY_MAPS_COUNT == ARRAY_SIZE(JoystickMapNames));
	if (joyname.empty())
		return false;

	mappings.resize(0);
	std::stringstream str;
	for (int i=0; i<JOY_MAPS_COUNT; i++)
	{
		str.clear();
		str.str("");
		str << "map_" << JoystickMapNames[i];
		const std::string& name = str.str();
		int32_t var;
		if (LoadSetting(port, joyname, name.c_str(), var))
			mappings.push_back(var);
		else
			mappings.push_back(-1);
	}

	for (int i=0; i<3; i++)
	{
		str.clear();
		str.str("");
		str << "inverted_" << JoystickMapNames[JOY_STEERING + i];
		const std::string& name = str.str();
		if (!LoadSetting(port, joyname, name.c_str(), inverted[i]))
			inverted[i] = false;
	}
	return true;
}

bool SaveMappings(int port, const std::string& joyname, const std::vector<uint16_t>& mappings, const bool (&inverted)[3])
{
	assert(JOY_MAPS_COUNT == countof(JoystickMapNames));
	if (joyname.empty() || mappings.size() != JOY_MAPS_COUNT)
		return false;

	std::stringstream str;
	for (int i=0; i<JOY_MAPS_COUNT; i++)
	{
		//XXX save anyway for manual editing
		//if (mappings[i] == (uint16_t)-1)
		//	continue;

		str.clear();
		str.str("");
		str << "map_" << JoystickMapNames[i];
		const std::string& name = str.str();
		if (!SaveSetting(port, joyname, name.c_str(), static_cast<int32_t>(mappings[i])))
			return false;
	}

	for (int i=0; i<3; i++)
	{
		str.clear();
		str.str("");
		str << "inverted_" << JoystickMapNames[JOY_STEERING + i];
		const std::string& name = str.str();
		if (!SaveSetting(port, joyname, name.c_str(), inverted[i]))
			return false;
	}
	return true;
}

static void refresh_store(ConfigData *cfg)
{
	GtkTreeIter iter;

	gtk_list_store_clear (cfg->store);
	for (int i = 0; i < JOY_MAPS_COUNT && i < cfg->mappings.size(); i++)
	{
		if (cfg->mappings[i] == (uint16_t)-1)
			continue;

		gtk_list_store_append (cfg->store, &iter);
		gtk_list_store_set (cfg->store, &iter,
			COL_PS2, JoystickMapNames[i],
			COL_PC, cfg->mappings[i],
			-1);
	}
}

static void joystick_changed (GtkComboBox *widget, gpointer data)
{
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	int port = (int) data;
	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);

	if (!cfg)
		return;

	std::string name = (cfg->joysticks.begin() + idx)->first;
	cfg->js_iter = (cfg->joysticks.begin() + idx);

	if (idx > 0)
	{
		LoadMappings(port, name, cfg->mappings, cfg->inverted);
		refresh_store(cfg);
	}
	OSDebugOut("Selected player %d idx: %d dev: '%s'\n", 2 - port, idx, name.c_str());
}

static void button_clicked (GtkComboBox *widget, gpointer data)
{
	int port = (int)data;
	int type = (int)g_object_get_data (G_OBJECT (widget), JOYTYPE);
	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);

	if (cfg && type < cfg->mappings.size() && cfg->js_iter != cfg->joysticks.end())
	{
		int value;
		bool inverted = false;
		bool isaxis = (type >= JOY_STEERING && type <= JOY_BRAKE);
		gtk_label_set_text (GTK_LABEL (cfg->label), "Polling for input...");
		OSDebugOut("%s isaxis:%d %s\n" , cfg->js_iter->second.c_str(), isaxis, JoystickMapNames[type]);

		// let label change its text
		while (gtk_events_pending ())
			gtk_main_iteration_do (FALSE);

		if (cfg->cb->poll(cfg->js_iter->second, isaxis, value, inverted))
		{
			cfg->mappings[type] = value;
			if (isaxis)
				cfg->inverted[type - JOY_STEERING] = inverted;
			refresh_store(cfg);
		}
		gtk_label_set_text (GTK_LABEL (cfg->label), "");
	}
}

static void clear_all_clicked (GtkComboBox *widget, gpointer data)
{
	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);
	auto& m = cfg->mappings;
	m.assign(JOY_MAPS_COUNT, -1);
	refresh_store(cfg);
}

static void hidraw_toggled (GtkToggleButton *widget, gpointer data)
{
	int port = (int) data;
	ConfigData *cfg = (ConfigData *) g_object_get_data (G_OBJECT(widget), CFG);
	if (cfg) {
		cfg->use_hidraw_ff_pt = (bool) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	}
}

int GtkPadConfigure(int port, const char *apititle, const char *apiname, GtkWindow *parent, ApiCallbacks& apicbs)
{
	GtkWidget *ro_frame, *ro_label, *rs_hbox, *rs_label, *rs_cb;
	GtkWidget *main_hbox, *right_vbox, *left_vbox, *treeview;

	ConfigData cfg;
	cfg.js_iter = cfg.joysticks.end();
	cfg.label = gtk_label_new ("");
	cfg.store = gtk_list_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_UINT);
	cfg.cb = &apicbs;

	apicbs.populate(cfg.joysticks);
	std::string path;
	LoadSetting(port, apiname, N_JOYSTICK, path);

	cfg.use_hidraw_ff_pt = false;
	bool is_evdev = (strncmp(apiname, "evdev", 5) == 0);
	if (is_evdev) //TODO idk about joydev
	{
		LoadSetting(port, apiname, N_HIDRAW_FF_PT, cfg.use_hidraw_ff_pt);
	}

	// ---------------------------
	std::string title = (port ? "Player One " : "Player Two ");
	title += apititle;

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
	GtkCellRenderer *render = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
		-1,
		"PS2",
		render,
		"text", COL_PS2,
		NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
		-1,
		"PC",
		render,
		"text", COL_PC,
		NULL);

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (cfg.store));
	g_object_unref (GTK_TREE_MODEL (cfg.store)); //treeview has its own ref
	gtk_box_pack_start (GTK_BOX (left_vbox), treeview, TRUE, TRUE, 5);

	// ---------------------------
	rs_cb = new_combobox ("Joystick:", right_vbox);

	int idx = 0, sel_idx = 0;
	for (auto& it : cfg.joysticks)
	{
		std::stringstream str;
		str << it.first;
		if (!it.second.empty())
			str << " [" << it.second << "]";

		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (rs_cb), str.str().c_str ());
		if (!path.empty() && it.second == path)
		{
			sel_idx = idx;
			if (idx > 0)
			{
				LoadMappings (port, it.first, cfg.mappings, cfg.inverted);
				refresh_store(&cfg);
			}
		}
		idx++;
	}

	g_object_set_data (G_OBJECT (rs_cb), CFG, &cfg);
	g_signal_connect (G_OBJECT (rs_cb), "changed", G_CALLBACK (joystick_changed), (gpointer)port);
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
			{1, 0, JOY_L2},
			{1, 1, JOY_L1},
			{6, 0, JOY_R2},
			{6, 1, JOY_R1},
			{0, 3, JOY_LEFT},
			{1, 2, JOY_UP},
			{2, 3, JOY_RIGHT},
			{1, 4, JOY_DOWN},
			{5, 3, JOY_SQUARE},
			{6, 4, JOY_CROSS},
			{7, 3, JOY_CIRCLE},
			{6, 2, JOY_TRIANGLE},
			{3, 3, JOY_SELECT},
			{4, 3, JOY_START},
		};

		for (int i=0; i<ARRAY_SIZE(button_labels); i++)
		{
			GtkWidget *button = gtk_button_new_with_label (button_labels[i]);
			g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (button_clicked), (gpointer)port);

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
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (button_clicked), (gpointer)port);

		button = gtk_button_new_with_label ("Throttle");
		gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
		g_object_set_data (G_OBJECT (button), JOYTYPE, (gpointer)JOY_THROTTLE);
		g_object_set_data (G_OBJECT (button), CFG, &cfg);
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (button_clicked), (gpointer)port);

		button = gtk_button_new_with_label ("Brake");
		gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 5);
		g_object_set_data (G_OBJECT (button), JOYTYPE, (gpointer)JOY_BRAKE);
		g_object_set_data (G_OBJECT (button), CFG, &cfg);
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (button_clicked), (gpointer)port);

		gtk_box_pack_start (GTK_BOX (right_vbox), cfg.label, TRUE, TRUE, 5);

		button = gtk_button_new_with_label ("Clear All");
		gtk_box_pack_start (GTK_BOX (right_vbox), button, TRUE, TRUE, 5);
		g_object_set_data (G_OBJECT (button), CFG, &cfg);
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (clear_all_clicked), (gpointer)port);
	}

	if (is_evdev)
	{
		GtkWidget *chk_btn = gtk_check_button_new_with_label("Pass-through raw force feedback commands (hidraw, if supported)");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chk_btn), (gboolean)cfg.use_hidraw_ff_pt);
		g_object_set_data (G_OBJECT (chk_btn), CFG, &cfg);
		g_signal_connect (G_OBJECT (chk_btn), "toggled", G_CALLBACK (hidraw_toggled), (gpointer)port);
		gtk_container_add (GTK_CONTAINER(right_vbox), chk_btn);
	}
	// ---------------------------
	gtk_widget_show_all (dlg);
	gint result = gtk_dialog_run (GTK_DIALOG (dlg));

	OSDebugOut("mappings %d\n", cfg.mappings.size());

	int ret = RESULT_OK;
	if (result == GTK_RESPONSE_OK)
	{
		if (cfg.js_iter != cfg.joysticks.end()) {
			if (!SaveSetting(port, apiname, N_JOYSTICK, cfg.js_iter->second))
				ret = RESULT_FAILED;

			if (cfg.js_iter != cfg.joysticks.begin()) // if not "None"
				SaveMappings(port, cfg.js_iter->first, cfg.mappings, cfg.inverted);
			if (is_evdev) {
				SaveSetting(port, apiname, N_HIDRAW_FF_PT, cfg.use_hidraw_ff_pt);
			}
		}
	}
	else
		ret = RESULT_CANCELED;

	gtk_widget_destroy (dlg);
	return ret;
}

#undef APINAME

#pragma once
#include "gtk.h"
#include "../padproxy.h"
#include "../../configuration.h"

#define N_HIDRAW_FF_PT	"hidraw_ff_pt"

typedef std::vector< std::pair<std::string, std::string> > vstring;
GtkWidget *new_combobox(const char* label, GtkWidget *vbox);

namespace usb_pad { namespace evdev {

enum
{
	COL_NAME = 0,
	COL_PS2,
	COL_PC,
	COL_COLUMN_WIDTH,
	COL_BINDING,
	NUM_COLS
};

// Keep in sync with PS2Buttons enum
enum JoystickMap
{
	JOY_CROSS = 0,
	JOY_SQUARE,
	JOY_CIRCLE,
	JOY_TRIANGLE,
	JOY_R1,
	JOY_L1,
	JOY_R2,
	JOY_L2,
	JOY_SELECT,
	JOY_START,
	JOY_R3, JOY_L3, //order, afaik not used on any PS2 wheel anyway
	JOY_DOWN,
	JOY_LEFT,
	JOY_UP,
	JOY_RIGHT,
	JOY_STEERING,
	JOY_THROTTLE,
	JOY_BRAKE,
	JOY_MAPS_COUNT
};

static const char* JoystickMapNames [] = {
	"cross",
	"square",
	"circle",
	"triangle",
	"r1",
	"l1",
	"r2",
	"l2",
	"select",
	"start",
	"r3",
	"l3",
	"down",
	"left",
	"up",
	"right",
	"steering",
	"throttle",
	"brake"
};

struct Point { int x; int y; JoystickMap type; };

struct ConfigMapping
{
	std::vector<uint16_t> mappings;
	bool inverted[3];
	int initial[3];
	int fd = -1;
};

struct ApiCallbacks
{
	bool (*get_event_name)(int map, int event, const char **name);
	void (*populate)(vstring& jsdata);
	bool (*poll)(const std::vector<std::pair<std::string, ConfigMapping> >& jsconf, std::string& dev_name, bool isaxis, int& value, bool& inverted, int& initial);
};

struct ConfigData
{
	std::vector<std::pair<std::string, ConfigMapping> > jsconf;
	vstring joysticks;
	vstring::const_iterator js_iter;
	GtkWidget *label;
	GtkListStore *store;
	GtkTreeView *treeview;
	ApiCallbacks *cb;
	bool use_hidraw_ff_pt;
	const char *dev_type;
};

int GtkPadConfigure(int port, const char* dev_type, const char *title, const char *apiname, GtkWindow *parent, ApiCallbacks& apicbs);
bool LoadMappings(const char *dev_type, int port, const std::string& joyname, std::vector<uint16_t>& mappings, bool (&inverted)[3], int (&initial)[3]);
bool SaveMappings(const char *dev_type, int port, const std::string& joyname, const std::vector<uint16_t>& mappings, const bool (&inverted)[3], int (&initial)[3]);
}} //namespace

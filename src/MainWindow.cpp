#include "../config.h"
#include <libintl.h>
#define _(String) gettext (String)

#include "MainWindow.h"
#include <iostream>
#include <sstream>

typedef std::map<int, NethogsMonitorUpdate> RowUpdatesMap;
extern std::mutex row_updates_map_mutex;
extern RowUpdatesMap row_updates_map;
extern int nethogs_monitor_status;

template<typename T>
static T*loadWiget(Glib::RefPtr<Gtk::Builder>& builder, 
				   const char* name,
				   std::vector<Gtk::Widget*>* record)
{
	T* ptr = nullptr;
    builder->get_widget(name, ptr);
    assert(ptr);
	if( record )
		record->push_back(ptr);
	return ptr;
}


MainWindow::MainWindow()
: m_p_gtkwindow(nullptr)
{
}

MainWindow::~MainWindow()
{
}

bool MainWindow::onTimer()
{
	std::lock_guard<std::mutex> lock(row_updates_map_mutex);

	if( row_updates_map.empty() )
	{
		//no updates
		return true;
	}

	for(RowUpdatesMap::value_type& v : row_updates_map)
	{
		NethogsMonitorUpdate const& update = v.second;
				
		//update row data map and list store
		auto it = m_rows_data.lower_bound(update.pid);
		bool const existing = (it != m_rows_data.end() && it->first == update.pid);
		
		if( v.second.action == NETHOGS_APP_ACTION_REMOVE )
		{
			if( existing )
			{
				m_list_store->erase(it->second.list_item_it);
				m_rows_data.erase(update.pid);
			}
		}
		else
		{
			Gtk::ListStore::iterator ls_it;
			if( existing )
			{
				ls_it = it->second.list_item_it;
			}
			else
			{ 
				ls_it = m_list_store->append();
				it = m_rows_data.insert(it, std::make_pair(update.pid, RowData(ls_it)));
				//set fixed fields
				(*ls_it)[m_tree_data.pid ] = update.pid;
				(*ls_it)[m_tree_data.app_name] = getFileName(update.app_name);
				(*ls_it)[m_tree_data.app_path] = update.app_name;
			}
			//updte other fields
			(*ls_it)[m_tree_data.device_name] = update.device_name;
			(*ls_it)[m_tree_data.uid   	    ] = gtUserName(update.uid);
			(*ls_it)[m_tree_data.sent_bytes ] = update.sent_bytes;
			(*ls_it)[m_tree_data.recv_bytes ] = update.recv_bytes;
			(*ls_it)[m_tree_data.sent_kbs   ] = update.sent_kbs;
			(*ls_it)[m_tree_data.recv_kbs	] = update.recv_kbs;
			//save stat data
			it->second.sent_bytes = update.sent_bytes;
			it->second.recv_bytes = update.recv_bytes;
			it->second.sent_kbs   = update.sent_kbs;
			it->second.recv_kbs	  = update.recv_kbs;
		}
	}

	//these updates is not neede anymore
	row_updates_map.clear();	

	//update stats label
	m_total_data.sent_kbs = 0;
	m_total_data.recv_kbs = 0;
	for(auto const& v : m_rows_data)
	{
		m_total_data.sent_bytes += v.second.sent_bytes;
		m_total_data.recv_bytes += v.second.recv_bytes;
		m_total_data.sent_kbs   += v.second.sent_kbs;
		m_total_data.recv_kbs   += v.second.recv_kbs;
	}
	
	char const* const format
		(_( "Sent: %s | Received: %s | Outbound bandwidth: %s | Inbound bandwidth: %s" ));

	char buffer[300];
	snprintf(buffer, sizeof(buffer), format, 
		formatByteCount(m_total_data.sent_bytes).c_str(),
		formatByteCount(m_total_data.recv_bytes).c_str(),
		formatBandwidth(m_total_data.sent_kbs).c_str(),
		formatBandwidth(m_total_data.recv_kbs).c_str());
		
	m_p_statuslabel->set_label(buffer);	
	
	return true;
}

void MainWindow::onLoaded()
{
	if( !m_timer_connection.connected() )
	{
		m_timer_connection = Glib::signal_timeout().connect_seconds(sigc::bind(&MainWindow::onTimer, this), 1);
	}
}

bool MainWindow::onClosed(GdkEventAny*)
{
	m_timer_connection.disconnect();
	return false;
}

void MainWindow::run(Glib::RefPtr<Gtk::Application> app)
{   
	std::vector<Gtk::Widget*> record;
    Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();

	builder->add_from_resource("/nethogs_gui/window.glade");
	builder->add_from_resource("/nethogs_gui/headerbar.ui");
	
	
	//get widgets
	m_p_gtkwindow = loadWiget<Gtk::ApplicationWindow>(builder, "main_window", &record);
	m_p_statuslabel = loadWiget<Gtk::Label>(builder, "status_label", &record);
	Gtk::TreeView* tree_view = loadWiget<Gtk::TreeView>(builder, "treeview", &record);
	Gtk::HeaderBar* pheaderbar = loadWiget<Gtk::HeaderBar>(builder, "headerbar", &record);
		
	//set title bar
	m_p_gtkwindow->set_titlebar(*pheaderbar);
		
	//Create the Tree model
	 m_list_store = m_tree_data.createListStore(); 
	 assert(m_list_store);
	tree_view->set_model(m_list_store);
	m_tree_data.setTreeColumns(tree_view);
	
	//Connect signals
	m_p_gtkwindow->signal_realize().connect(std::bind(&MainWindow::onLoaded, this));
	m_p_gtkwindow->signal_delete_event().connect(sigc::mem_fun(this,&MainWindow::onClosed));
	m_p_gtkwindow->signal_show().connect(sigc::mem_fun(this,&MainWindow::onShow));
	
	//add actions
	app->add_action("about", sigc::mem_fun(this,&MainWindow::onAction_About));
	app->add_action("quit",  sigc::mem_fun(this,&MainWindow::onAction_Quit));
		
	app->run(*m_p_gtkwindow);
	

	for( Gtk::Widget* w : record )
	{
		delete w;
	}
}

void MainWindow::onShow()
{
	if( nethogs_monitor_status == NETHOGS_STATUS_FAILURE)
	{
		char const* fail_msg =  _("Failed to access network device(s).");
		std::ostringstream oss;
		oss << "<span foreground=\"red\">" << fail_msg << "</span>";
		m_p_statuslabel->set_markup(oss.str());
	} 		
}

void MainWindow::onAction_About()
{
	Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();
	builder->add_from_resource("/nethogs_gui/aboutdialog.glade");	

	Gtk::AboutDialog* dlg = loadWiget<Gtk::AboutDialog>(builder, "aboutdialog", nullptr);

	dlg->set_program_name(PACKAGE_NAME);
	dlg->set_version(PACKAGE_VERSION);
	dlg->set_comments(_("Per-application bandwidth usage statistics."));
	dlg->set_authors({"<a href=\"mailto:mbfoss@fastmail.com\">Mohamed Boussaffa</a>"});
	dlg->set_license(_("This program comes with ABSOLUTELY NO WARRANTY." "\n"
					 "See the GNU General Public License, version 2 or later for details."));
	dlg->run();

	delete dlg;
}

void MainWindow::onAction_Quit()
{
	m_p_gtkwindow->close();
}
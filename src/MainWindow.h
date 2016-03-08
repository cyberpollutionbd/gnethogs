#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Tools.h"
#include "libnethogs.h"
#include "TreeData.h"
#include <vector>
#include <gtkmm.h>
#include <cassert>
#include <mutex>
#include <sys/types.h>
#include <map>

class MainWindow
{
	struct RowData
	{
		RowData(Gtk::ListStore::iterator const& ls_it = Gtk::ListStore::iterator()) 
		: list_item_it(ls_it), sent_bytes(0), recv_bytes(0), sent_kbs(0), recv_kbs(0) 
		{
		}
		Gtk::ListStore::iterator list_item_it;
		uint32_t	sent_bytes;
		uint32_t	recv_bytes;
		float		sent_kbs;
		float		recv_kbs;
	};
	
public:
	MainWindow();
	~MainWindow();

	void run(Glib::RefPtr<Gtk::Application> app);
	

private:
	bool onTimer();
	void onLoaded();
	bool onClosed(GdkEventAny*);
	void onShow();
	
	void onAction_About();
	void onAction_Quit();

private:
	Gtk::ApplicationWindow* 		m_p_gtkwindow;
	Gtk::Label* 					m_p_statuslabel;
	Glib::RefPtr<Gtk::ListStore> 	m_list_store;
	std::map<int, RowData> 			m_rows_data;
	RowData							m_total_data;
	TreeData 						m_tree_data;
	sigc::connection 				m_timer_connection; 
};

#endif // MAINWINDOW_H
#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QSettings>
#include "ui_mainwindow.h"

class QStandardItemModel;

class MainWindow : public QMainWindow
{
	Q_OBJECT
	
	Ui::MainWindow Ui_;
	QLabel *ValidLabel_;
	QStandardItemModel *VersModel_;
	QStandardItemModel *DepsModel_;
	QSettings Settings_;
	bool EnableCheckValid_;
public:
	MainWindow ();
private:
	void Clear ();
private slots:
	bool checkValid ();

	void on_ActionNew__triggered ();
	void on_ActionLoad__triggered ();
	void on_ActionSave__triggered ();

	void on_AddVer__released ();
	void on_ModifyVer__released ();
	void on_RemoveVer__released ();

	void on_Type__currentIndexChanged (const QString&);
};

#endif

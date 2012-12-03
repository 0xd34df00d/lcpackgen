/**********************************************************************
 * LC Package Generator - a small utility to create LackMan packages.
 * Copyright (C) 2010-2011  Georg Rudoy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

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
	enum DepsColumns
	{
		DCThisVersion,
		DCType,
		DCName,
		DCVersion
	};
	QString CurrentFileName_;
public:
	MainWindow ();

	void Open (const QString&);
private:
	void Clear ();
	void UpdateWindowTitle ();
private slots:
	bool checkValid ();

	void on_ActionNew__triggered ();
	void on_ActionLoad__triggered ();
	void on_ActionSave__triggered ();
	void on_ActionSaveAs__triggered ();

	void on_AddVer__released ();
	void on_ModifyVer__released ();
	void on_RemoveVer__released ();
	void on_AddDep__released ();
	void on_ModifyDep__released ();
	void on_RemoveDep__released ();

	void on_Type__currentIndexChanged (const QString&);
};

#endif

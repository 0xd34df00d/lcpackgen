#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include "ui_mainwindow.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT
	
	Ui::MainWindow Ui_;
public:
	MainWindow ();
};

#endif


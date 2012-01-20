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

#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include "mainwindow.h"

int main (int argc, char **argv)
{
	QApplication app (argc, argv);

	MainWindow mw;
	mw.show ();

	QStringList args = app.arguments ();
	if (args.size () > 1)
	{
		QString filename = args.at (1);
		if (QFileInfo (filename).isAbsolute ())
			mw.Open (filename);
		else
		{
			QDir dir (QDir::currentPath ());
			mw.Open (dir.absoluteFilePath (filename));
		}
	}

	return app.exec ();
}

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

#include "mainwindow.h"
#include <stdexcept>
#include <QStandardItemModel>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QXmlQuery>
#include <QXmlResultItems>
#include <QDir>
#include <QLocale>
#include <QInputDialog>
#include <QDomDocument>
#include <QtDebug>

MainWindow::MainWindow ()
: ValidLabel_ (new QLabel (this))
, VersModel_ (new QStandardItemModel (this))
, DepsModel_ (new QStandardItemModel (this))
, Settings_ ("Deviant", "LCPackGen")
, EnableCheckValid_ (false)
{
	Ui_.setupUi (this);
	UpdateWindowTitle ();
	Ui_.VersTree_->setModel (VersModel_);
	Ui_.DepsTree_->setModel (DepsModel_);

	Ui_.ActionNew_->setIcon (QIcon::fromTheme ("document-new"));
	Ui_.ActionLoad_->setIcon (QIcon::fromTheme ("document-open"));
	Ui_.ActionSave_->setIcon (QIcon::fromTheme ("document-save"));
	Ui_.ActionSaveAs_->setIcon (QIcon::fromTheme ("document-save-as"));

	statusBar ()->addPermanentWidget (ValidLabel_);

	on_Type__currentIndexChanged (Ui_.Type_->currentText ());

	connect (Ui_.Name_,
			SIGNAL (textChanged (const QString&)),
			this,
			SLOT (checkValid ()));
	connect (Ui_.Description_,
			SIGNAL (textChanged (const QString&)),
			this,
			SLOT (checkValid ()));
	connect (Ui_.Tags_,
			SIGNAL (textChanged (const QString&)),
			this,
			SLOT (checkValid ()));
	connect (Ui_.LongDescription_,
			SIGNAL (textChanged ()),
			this,
			SLOT (checkValid ()));
	connect (Ui_.MaintName_,
			SIGNAL (textChanged (const QString&)),
			this,
			SLOT (checkValid ()));
	connect (Ui_.MaintEmail_,
			SIGNAL (textChanged (const QString&)),
			this,
			SLOT (checkValid ()));
	connect (Ui_.Thumbnails_,
			SIGNAL (textChanged ()),
			this,
			SLOT (checkValid ()));
	connect (Ui_.Screenshots_,
			SIGNAL (textChanged ()),
			this,
			SLOT (checkValid ()));
	connect (VersModel_,
			SIGNAL (itemChanged (QStandardItem*)),
			this,
			SLOT (checkValid ()));

	EnableCheckValid_ = true;
	checkValid ();
}

enum Type
{
	SetTextable,
	StringListJoinable,
	StringListModel,
	StringListList
};

template<Type ET>
struct EvaluateQuery;

template<>
struct EvaluateQuery<SetTextable>
{
	template<typename T>
	bool operator() (QXmlQuery& query,
			const QString& queryString, T *setTextable) const
	{
		query.setQuery (queryString);
		QString out;
		if (!query.evaluateTo (&out))
		{
			qWarning () << Q_FUNC_INFO
					<< "query evaluation error";
			return false;
		}

		setTextable->setText (out.trimmed ());
		return true;
	}
};

template<>
struct EvaluateQuery<StringListJoinable>
{
	template<typename T>
	bool operator() (QXmlQuery& query,
			const QString& queryString, T *joinable) const
	{
		query.setQuery (queryString);
		QStringList out;
		if (!query.evaluateTo (&out))
		{
			qWarning () << Q_FUNC_INFO
					<< "query evaluation error";
			return false;
		}

		for (int i = 0; i < out.size (); ++i)
			out [i] = out.at (i).trimmed ();

		joinable->setPlainText (out.join ("\n"));
		return true;
	}
};

template<>
struct EvaluateQuery<StringListModel>
{
	bool operator() (QXmlQuery& query,
			const QString& queryString, QStandardItemModel *model) const
	{
		query.setQuery (queryString);
		QStringList out;
		if (!query.evaluateTo (&out))
		{
			qWarning () << Q_FUNC_INFO
					<< "query evaluation error";
			return false;
		}

		Q_FOREACH (const QString& str, out)
			model->appendRow (new QStandardItem (str.trimmed ()));
		return true;
	}
};

template<>
struct EvaluateQuery<StringListList>
{
	bool operator() (QXmlQuery& query,
			const QString& queryString,
			QList<QStringList>& list) const
	{
		query.setQuery (queryString);
		QStringList out;
		if (!query.evaluateTo (&out))
		{
			qWarning () << Q_FUNC_INFO
					<< "outer query evaluation error";
			return false;
		}

		if (list.isEmpty ())
			Q_FOREACH (const QString& str, out)
				list.append (QStringList (str.trimmed ()));
		else
		{
			if (out.size () != list.size ())
			{
				qWarning () << Q_FUNC_INFO
						<< "size mismatch, out vs list:"
						<< out
						<< list;
				return false;
			}
			for (int i = 0, size = out.size ();
					i < size; ++i)
				list [i] << out.at (i).trimmed ();
		}

		return true;
	}
};

void MainWindow::Open (const QString& fileName)
{
	Clear ();

	CurrentFileName_ = fileName;

	Settings_.setValue ("LastLoadDir",
			QFileInfo (fileName).absolutePath ());

	QFile file (fileName);
	if (!file.open (QIODevice::ReadOnly))
	{
		QMessageBox::warning (this,
				tr ("Warning"),
				tr ("Unable to open file %1 for reading.").arg (fileName));
		return;
	}

	EnableCheckValid_ = false;

	QXmlQuery query;
	query.setFocus (&file);

	try
	{
		QString out;
		query.setQuery ("/package/normalize-space(string(@type))");
		if (!query.evaluateTo (&out))
			throw std::runtime_error (tr ("Could not get package type.")
					.toUtf8 ().constData ());

		out = out.trimmed ().simplified ();

		int pos = -1;
		if ((pos = Ui_.Type_->findText (out)) == -1)
		{
			QMessageBox::warning (this,
					tr ("Warning"),
					tr ("Unknown package type `%1`, defaulting to `plugin`.")
						.arg (out));
			pos = 0;
		}

		Ui_.Type_->setCurrentIndex (pos);

		query.setQuery ("/package/normalize-space(string(@language))");
		if (!query.evaluateTo (&out))
			throw std::runtime_error (tr ("Could not get package language.")
					.toUtf8 ().constData ());

		out = out.trimmed ().simplified ();

		if (Ui_.Type_->currentText () == "plugin")
		{
			int pos = 0;
			if ((pos = Ui_.Language_->findText (out)) == -1)
				throw std::runtime_error (tr ("Unknown plugin language <code>%1</code>.")
						.arg (out)
						.toUtf8 ().constData ());
			Ui_.Language_->setCurrentIndex (pos);
		}
		else if (Ui_.Type_->currentText () == "translation")
			Ui_.Language_->addItem (out);

		QStringList tags;

		query.setQuery ("/package/tags/tag/string(text())");
		if (!query.evaluateTo (&tags))
			throw std::runtime_error (tr ("Could not get tags.")
					.toUtf8 ().constData ());

		Ui_.Tags_->setText (tags.join ("; "));

		EvaluateQuery<SetTextable> () (query, "/package/name/text()", Ui_.Name_);
		EvaluateQuery<SetTextable> () (query, "/package/description/text()", Ui_.Description_);
		EvaluateQuery<SetTextable> () (query, "/package/long/text()", Ui_.LongDescription_);
		EvaluateQuery<SetTextable> () (query, "/package/maintainer/name/text()", Ui_.MaintName_);
		EvaluateQuery<SetTextable> () (query, "/package/maintainer/email/text()", Ui_.MaintEmail_);

		EvaluateQuery<StringListModel> () (query, "/package/versions/version/normalize-space(string(text()))", VersModel_);

		EvaluateQuery<StringListJoinable> () (query, "/package/images/thumbnail/normalize-space(string(@url))", Ui_.Thumbnails_);
		EvaluateQuery<StringListJoinable> () (query, "/package/images/screenshot/normalize-space(string(@url))", Ui_.Screenshots_);

		QList<QStringList> rows;
		EvaluateQuery<StringListList> () (query, "/package/depends/depend/string(@thisVersion)", rows);
		EvaluateQuery<StringListList> () (query, "/package/depends/depend/string(@type)", rows);
		EvaluateQuery<StringListList> () (query, "/package/depends/depend/string(@name)", rows);
		EvaluateQuery<StringListList> () (query, "/package/depends/depend/string(@version)", rows);
		Q_FOREACH (const QStringList& row, rows)
		{
			QList<QStandardItem*> items;
			Q_FOREACH (const QString& item, row)
				items << new QStandardItem (item);
			DepsModel_->appendRow (items);
		}
	}
	catch (const std::exception& e)
	{
		QMessageBox::critical (this,
				tr ("Critical error"),
				QString::fromUtf8 (e.what ()));
	}

	EnableCheckValid_ = true;

	checkValid ();
}

void MainWindow::Clear ()
{
	EnableCheckValid_ = false;

	Ui_.Name_->clear ();
	Ui_.Description_->clear ();
	Ui_.LongDescription_->clear ();
	Ui_.MaintName_->clear ();
	Ui_.MaintEmail_->clear ();
	Ui_.Thumbnails_->clear ();
	Ui_.Screenshots_->clear ();
	VersModel_->clear ();
	DepsModel_->clear ();

	EnableCheckValid_ = true;

	checkValid ();
}

void MainWindow::UpdateWindowTitle ()
{
	QString filename = QFileInfo (CurrentFileName_).fileName ();
	if (filename.isEmpty ())
		filename = tr ("New file");

	setWindowTitle (QString ("%1[*] - LCPackGen").arg (filename));
}

bool MainWindow::checkValid ()
{
	setWindowModified (true);
	if (!EnableCheckValid_)
		return true;

	QStringList reasons;

	if (Ui_.Name_->text ().isEmpty ())
		reasons << tr ("<em>Name</em> is empty.");

	if (Ui_.Description_->text ().isEmpty ())
		reasons << tr ("<em>Description</em> is empty.");

	if (Ui_.Tags_->text ().isEmpty ())
		reasons << tr ("<em>Tags</em> are empty.");

	if (Ui_.MaintName_->text ().isEmpty ())
		reasons << tr ("<em>Maintainer name</em> is empty.");

	if (Ui_.MaintEmail_->text ().isEmpty ())
		reasons << tr ("<em>Maintainer email</em> is empty.");

	if (Ui_.Type_->currentText () == "translation" &&
			QLocale (Ui_.Language_->currentText ()).language () == QLocale::C)
		reasons << tr ("<em>Language</em> has unkown language code %1.")
				.arg (Ui_.Language_->currentText ());

	if (!VersModel_->rowCount ())
		reasons << tr ("No versions are defined.");

	if (reasons.size ())
	{
		ValidLabel_->setText (tr ("Invalid"));
		QString toolTip = QString ("<ul><li>%1</li></ul>")
				.arg (reasons.join ("</li><li>"));
		ValidLabel_->setToolTip (toolTip);
		ValidLabel_->setStyleSheet ("color: red;");
		return false;
	}
	else
	{
		ValidLabel_->setText (tr ("Valid"));
		ValidLabel_->setToolTip (QString ());
		ValidLabel_->setStyleSheet ("color: green;");
		return true;
	}
}

void MainWindow::on_ActionNew__triggered ()
{
	Clear ();

	setWindowModified (false);
	CurrentFileName_ = QString ();

	UpdateWindowTitle ();
}

void MainWindow::on_ActionLoad__triggered ()
{
	QString prevDir = Settings_.value ("LastLoadDir",
			QDir::homePath ()).toString ();
	QString fileName = QFileDialog::getOpenFileName (this,
			tr ("Select file"),
			prevDir,
			tr ("XML files (*.xml);;All files (*.*)"));
	if (fileName.isEmpty ())
		return;

	Open (fileName);
	setWindowModified (false);
	UpdateWindowTitle ();
}

void MainWindow::on_ActionSave__triggered ()
{
	if (!checkValid () &&
			QMessageBox::question (this,
					tr ("Question"),
					tr ("Are you sure you want to save an <strong>invalid</strong> document?"),
					QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
		return;

	QString normalizedName = Ui_.Name_->text ().simplified ();
	normalizedName.remove (' ');
	normalizedName.remove ('\t');

	if (CurrentFileName_.isEmpty ())
	{
		QString prevDir = Settings_.value ("LastLoadDir",
				QDir::homePath ()).toString ();

		CurrentFileName_ = QFileDialog::getSaveFileName (this,
				tr ("Select file name"),
				prevDir + '/' + normalizedName + ".xml",
				tr ("XML files (*.xml);;All files (*.*)"));

		if (CurrentFileName_.isEmpty ())
			return;
	}

	QFile outFile (CurrentFileName_);
	if (!outFile.open (QIODevice::WriteOnly))
	{
		QMessageBox::warning (this,
				tr ("Critical error"),
				tr ("Could not open file %1 for writing. File is not saved.")
					.arg (CurrentFileName_));
		return;
	}

	QDomDocument doc;

	QDomElement package = doc.createElement ("package");
	package.setAttribute ("type", Ui_.Type_->currentText ());
	if (!Ui_.Language_->currentText ().isEmpty ())
		package.setAttribute ("language", Ui_.Language_->currentText ());
	doc.appendChild (package);

	QDomElement name = doc.createElement ("name");
	name.appendChild (doc.createTextNode (Ui_.Name_->text ()));
	package.appendChild (name);

	QDomElement descr = doc.createElement ("description");
	descr.appendChild (doc.createTextNode (Ui_.Description_->text ()));
	package.appendChild (descr);

	QDomElement tags = doc.createElement ("tags");
	package.appendChild (tags);
	Q_FOREACH (const QString& tag, Ui_.Tags_->text ().split ("; "))
	{
		QDomElement tagNode = doc.createElement ("tag");
		tagNode.appendChild (doc.createTextNode (tag));
		tags.appendChild (tagNode);
	}

	QDir dir = QFileInfo (CurrentFileName_).dir ();
	dir.cd ("arch");

	QDomElement versions = doc.createElement ("versions");
	package.appendChild (versions);
	for (int i = 0, size = VersModel_->rowCount ();
			i < size; ++i)
	{
		const QString& version = VersModel_->item (i)->text ();

		QDomElement verNode = doc.createElement ("version");
		verNode.appendChild (doc.createTextNode (version));

		// C++11 range-based for
		QStringList knownArchivers;
		knownArchivers << "xz"
				<< "lzma"
				<< "bz2"
				<< "gz";
		Q_FOREACH (const QString& str, knownArchivers)
		{
			const QString& archName = QString ("%1-%2.tar.%3")
					.arg (normalizedName)
					.arg (version)
					.arg (str);
			if (!dir.exists (archName))
				continue;

			verNode.setAttribute ("size", QFileInfo (dir.filePath (archName)).size ());
			verNode.setAttribute ("archiver", str);
			break;
		}

		versions.appendChild (verNode);
	}

	QString iconUrl = Ui_.Icon_->text ();
	QString rawThumbs = Ui_.Thumbnails_->toPlainText ();
	QString rawScreens = Ui_.Screenshots_->toPlainText ();
	if (!iconUrl.isEmpty () ||
			!rawThumbs.isEmpty () ||
			!rawScreens.isEmpty ())
	{
		QDomElement images = doc.createElement ("images");

		QDomElement icon = doc.createElement ("icon");
		icon.setAttribute ("url", iconUrl);
		images.appendChild (icon);

		QStringList splitted = rawThumbs.split ("\n", QString::SkipEmptyParts);
		Q_FOREACH (const QString& thumbUrl, splitted)
		{
			QDomElement thumb = doc.createElement ("thumbnail");
			thumb.setAttribute ("url", thumbUrl);
			images.appendChild (thumb);
		}

		splitted = rawScreens.split ('\n', QString::SkipEmptyParts);
		Q_FOREACH (const QString& screenUrl, splitted)
		{
			QDomElement screen = doc.createElement ("screenshot");
			screen.setAttribute ("url", screenUrl);
			images.appendChild (screen);
		}

		package.appendChild (images);
	}

	QString longDescr = Ui_.LongDescription_->toPlainText ();
	if (!longDescr.isEmpty ())
	{
		QDomElement longNode = doc.createElement ("long");
		longNode.appendChild (doc.createTextNode (longDescr));
		package.appendChild (longNode);
	}

	QDomElement maintainer = doc.createElement ("maintainer");
	QDomElement mname = doc.createElement ("name");
	mname.appendChild (doc.createTextNode (Ui_.MaintName_->text ()));
	maintainer.appendChild (mname);
	QDomElement memail = doc.createElement ("email");
	memail.appendChild (doc.createTextNode (Ui_.MaintEmail_->text ()));
	maintainer.appendChild (memail);
	package.appendChild (maintainer);

	QDomElement depends = doc.createElement ("depends");
	for (int i = 0, size = DepsModel_->rowCount ();
			i < size; ++i)
	{
		QDomElement depend = doc.createElement ("depend");
		depend.setAttribute ("type",
				DepsModel_->item (i, DCType)->text ());
		depend.setAttribute ("thisVersion",
				DepsModel_->item (i, DCThisVersion)->text ());
		depend.setAttribute ("name",
				DepsModel_->item (i, DCName)->text ());
		depend.setAttribute ("version",
				DepsModel_->item (i, DCVersion)->text ());
		depends.appendChild (depend);
	}
	package.appendChild (depends);

	// Ugly hack, on my system QDomDocument doesn't write header for
	// some reason.
	QByteArray result = doc.toByteArray ();
	if (!result.startsWith ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"))
		result.prepend ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	outFile.write (result);

	setWindowModified (false);
	UpdateWindowTitle ();
}

void MainWindow::on_ActionSaveAs__triggered ()
{
	CurrentFileName_.clear ();
	on_ActionSave__triggered ();
	UpdateWindowTitle ();
}

void MainWindow::on_AddVer__released ()
{
	QString ver = QInputDialog::getText (this,
			tr ("Enter version"),
			tr ("Enter new version number you would like to add to the list of versions:"));
	if (ver.isEmpty ())
		return;

	if (VersModel_->findItems (ver).size ())
	{
		QMessageBox::warning (this,
				tr ("Warning"),
				tr ("Version <em>%1</em> already exists.")
					.arg (ver));
		return;
	}

	VersModel_->appendRow (new QStandardItem (ver));

	checkValid ();
}

void MainWindow::on_ModifyVer__released ()
{
	QModelIndex current = Ui_.VersTree_->selectionModel ()->currentIndex ();
	if (!current.isValid ())
		return;

	QStandardItem *item = VersModel_->itemFromIndex (current);
	QString ver = QInputDialog::getText (this,
			tr ("Enter version"),
			tr ("Enter new version number:"),
			QLineEdit::Normal,
			item->text ());
	if (ver.isEmpty () ||
			ver == item->text ())
		return;

	if (VersModel_->findItems (ver).size ())
	{
		QMessageBox::warning (this,
				tr ("Warning"),
				tr ("Version <em>%1</em> already exists.")
					.arg (ver));
		return;
	}

	for (int i = 0, size = DepsModel_->rowCount ();
			i < size; ++i)
	{
		QStandardItem *depItem = DepsModel_->item (i, DCThisVersion);
		if (depItem->text () == item->text ())
			depItem->setText (ver);
	}

	item->setText (ver);
}

void MainWindow::on_RemoveVer__released ()
{
	if (VersModel_->rowCount () == 1)
		return;

	QModelIndex current = Ui_.VersTree_->selectionModel ()->currentIndex ();
	if (!current.isValid ())
		return;

	QString ver = VersModel_->itemFromIndex (current)->text ();
	QList<QStandardItem*> relatedItems = VersModel_->findItems (ver);
	if (relatedItems.size ())
	{
		QMessageBox::warning (this,
				tr ("Error"),
				tr ("There are %1 dependencies which mention this version "
					"(%2) you are going to delete. Please modify or remove"
					" those dependencies first.")
					.arg (relatedItems.size ())
					.arg (ver));
		return;
	}

	qDeleteAll (VersModel_->takeRow (current.row ()));
}

void MainWindow::on_AddDep__released ()
{
	QStringList thisVersions;
	for (int i = 0, size = VersModel_->rowCount ();
			i < size; ++i)
		thisVersions << VersModel_->item (i)->text ();

	if (!thisVersions.size ())
	{
		QMessageBox::critical (this,
				tr ("Error"),
				tr ("First define at least one package version."));
		return;
	}

	QString thisVer = QInputDialog::getItem (this,
			tr ("Select version"),
			tr ("Select version of <em>this</em> package for which the "
				"dependency you are going to create applies:"),
			thisVersions,
			0,
			false);
	if (thisVer.isEmpty ())
		return;

	QString type = QInputDialog::getItem (this,
			tr ("Select dependency type"),
			tr ("Choose whether this dependency is a requirement or a provision one."),
			QStringList ("provide") << "require",
			0,
			false);
	if (type.isEmpty ())
		return;

	QString name;
	do
	{
		if (!name.isEmpty ())
			QMessageBox::critical (this,
					tr ("Error"),
					tr ("Dependency name should begin with "
						"<code>interface://</code> when requiring or "
						"providing an interface or "
						"<code>plugin://</code>when requiring a plugin"));

		name = QInputDialog::getText (this,
				tr ("Enter dependency name"),
				tr ("Enter the name of the interface this plugin depends"
					" on or provides or name of other plugin this one depends on:"));

		if (name.isEmpty ())
			return;
	} while (!(name.startsWith ("interface://") || name.startsWith ("plugin://")));

	QString depVersion = QInputDialog::getText (this,
			tr ("Enter dependency version"),
			tr ("Enter the dependency version for the <em>%1</em> dependency.")
				.arg (name));
	if (depVersion.isEmpty ())
		return;


	QList<QStandardItem*> items;
	items << new QStandardItem (thisVer);
	items << new QStandardItem (type);
	items << new QStandardItem (name);
	items << new QStandardItem (depVersion);
	DepsModel_->appendRow (items);
}

void MainWindow::on_ModifyDep__released ()
{
	QModelIndex current = Ui_.DepsTree_->selectionModel ()->currentIndex ();
	if (!current.isValid ())
		return;

	QStandardItem *item = DepsModel_->itemFromIndex (current);
	QString ver = QInputDialog::getText (this,
			tr ("Enter new data"),
			tr ("Enter new data for this item (%1):")
				.arg (item->text ()),
			QLineEdit::Normal,
			item->text ());
	if (ver.isEmpty () ||
			ver == item->text ())
		return;

	item->setText (ver);
}

void MainWindow::on_RemoveDep__released ()
{
	QModelIndex current = Ui_.DepsTree_->selectionModel ()->currentIndex ();
	if (!current.isValid ())
		return;

	qDeleteAll (DepsModel_->takeRow (current.row ()));
}

void MainWindow::on_Type__currentIndexChanged (const QString& text)
{
	Ui_.Language_->clear ();
	if (text == "plugin")
	{
		Ui_.Language_->setEnabled (true);
		Ui_.Language_->setEditable (false);
		Ui_.Language_->addItems (QStringList () << "python" << "qtscript");
	}
	else if (text == "translation")
	{
		Ui_.Language_->setEnabled (true);
		Ui_.Language_->setEditable (true);
	}
	else
		Ui_.Language_->setEnabled (false);
}

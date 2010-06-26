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
#include <QtDebug>

MainWindow::MainWindow ()
: ValidLabel_ (new QLabel (this))
, VersModel_ (new QStandardItemModel (this))
, DepsModel_ (new QStandardItemModel (this))
, Settings_ ("Deviant", "LCPackGen")
, EnableCheckValid_ (false)
{
	Ui_.setupUi (this);
	Ui_.VersTree_->setModel (VersModel_);
	Ui_.DepsTree_->setModel (DepsModel_);

	Ui_.ActionNew_->setIcon (QIcon::fromTheme ("document-new"));
	Ui_.ActionLoad_->setIcon (QIcon::fromTheme ("document-open"));
	Ui_.ActionSave_->setIcon (QIcon::fromTheme ("document-save"));

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

bool MainWindow::checkValid ()
{
	if (!EnableCheckValid_)
		return true;

	QStringList reasons;

	if (Ui_.Name_->text ().isEmpty ())
		reasons << tr ("<em>Name</em> is empty.");

	if (Ui_.Description_->text ().isEmpty ())
		reasons << tr ("<em>Description</em> is empty.");

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
}

enum Type
{
	SetTextable,
	StringListJoinable,
	StringListModel,
	StringListListModel
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

		setTextable->setText (out);
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
			model->appendRow (new QStandardItem (str));
		return true;
	}
};

template<>
struct EvaluateQuery<StringListListModel>
{
	bool operator() (QXmlQuery& query,
			const QString& queryString,
			const QString& subQueryString,
			QStandardItemModel *model) const
	{
		query.setQuery (queryString);
		QXmlResultItems resultItems;
		query.evaluateTo (&resultItems);
		if (resultItems.hasError ())
		{
			qWarning () << Q_FUNC_INFO
					<< "outer query evaluation error";
			return false;
		}

		QList<QStringList> listOfLists;

		QXmlItem subItem = resultItems.next ();
		while (!subItem.isNull ())
		{
			QXmlQuery subQuery;
			subQuery.setFocus (subItem);
			subQuery.setQuery (subQueryString);

			QString out;
			if (!subQuery.evaluateTo (&out))
			{
				qWarning () << Q_FUNC_INFO
						<< "subquery evaluation error";
				return false;
			}

			qDebug () << out;
			//listOfLists.append (out);

			subItem = resultItems.next ();
		}

		qDebug () << listOfLists;

		Q_FOREACH (const QStringList& row, listOfLists)
		{
			QList<QStandardItem*> rowItems;
			Q_FOREACH (const QString& item, row)
				rowItems << new QStandardItem (item);
			model->appendRow (rowItems);
		}

		return true;
	}
};

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

	Clear ();

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

		EvaluateQuery<SetTextable> () (query, "/package/name/text()", Ui_.Name_);
		EvaluateQuery<SetTextable> () (query, "/package/description/text()", Ui_.Description_);
		EvaluateQuery<SetTextable> () (query, "/package/long/text()", Ui_.LongDescription_);
		EvaluateQuery<SetTextable> () (query, "/package/maintainer/name/text()", Ui_.MaintName_);
		EvaluateQuery<SetTextable> () (query, "/package/maintainer/email/text()", Ui_.MaintEmail_);

		EvaluateQuery<StringListModel> () (query, "/package/versions/version/normalize-space(string(text()))", VersModel_);

		EvaluateQuery<StringListJoinable> () (query, "/package/thumbnails/thumbnail/normalize-space(string(@url))", Ui_.Thumbnails_);
		EvaluateQuery<StringListJoinable> () (query, "/package/screenshots/screenshot/normalize-space(string(@url))", Ui_.Screenshots_);

//		EvaluateQuery<StringListListModel> () (query, "/package/depends/depend",
//				"(name/text()|version/text())", DepsModel_);
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

void MainWindow::on_ActionSave__triggered ()
{
	if (!checkValid () &&
			QMessageBox::question (this,
					tr ("Question"),
					tr ("Are you sure you want to save an <strong>invalid</strong> document?"),
					QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
		return;
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
		QStandardItem *depItem = DepsModel_->item (i);
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

	qDeleteAll (VersModel_->takeRow (current.row ()));
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

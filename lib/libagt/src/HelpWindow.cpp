/*
 * Copyright (c) 2014, Intel Corporation
 *
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are 
 * met:
 * 
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright 
 *   notice, this list of conditions and the following disclaimer in the 
 *   documentation and/or other materials provided with the distribution.
 * - Neither the name of the Intel Corporation nor the names of its 
 *   contributors may be used to endorse or promote products derived from 
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
  * @file  HelpWindow.cpp
  */

#include "HelpWindow.h"

#include <qstatusbar.h>
#include <qpixmap.h>
#include <q3popupmenu.h>
#include <qmenubar.h>
#include <q3toolbar.h>
#include <qtoolbutton.h>
#include <qicon.h>
#include <qfile.h>
#include <q3textstream.h>
#include <q3stylesheet.h>
#include <qmessagebox.h>
#include <q3filedialog.h>
#include <qapplication.h>
#include <qcombobox.h>
#include <qevent.h>
#include <qlineedit.h>
#include <qobject.h>
#include <qfileinfo.h>
#include <qfile.h>
#include <qdatastream.h>
#include <qprinter.h>
#include <q3simplerichtext.h>
#include <qpainter.h>
#include <q3paintdevicemetrics.h>
//Added by qt3to4:
#include <Q3Frame>

#include <ctype.h>

// icons
#include "xpm/help_forward.xpm"
#include "xpm/help_back.xpm"
#include "xpm/help_home.xpm"

HelpWindow::HelpWindow( const QString& home_, const QStringList& _path,QWidget* parent, const char *name ) :
Q3MainWindow( parent, name, Qt::WDestructiveClose ),pathCombo( 0 ), selectedURL()
{
    myParent=parent;
    readHistory();
    readBookmarks();

    browser = new Q3TextBrowser( this );Q_ASSERT(browser!=NULL);
    browser->setFrameStyle( Q3Frame::Panel | Q3Frame::Sunken );
    browser->mimeSourceFactory()->setFilePath( _path );
    //browser->mimeSourceFactory()->setExtensionType("html", "text/html");

    connect( browser, SIGNAL( textChanged() ),this,SLOT( textChanged() ) );

    setCentralWidget( browser );

    if ( !home_.isEmpty() )
    {
        browser->setSource( home_ );
    }

    connect( browser, SIGNAL( highlighted( const QString&) ),
         statusBar(), SLOT( message( const QString&)) );

    resize( 640,700 );

    Q3PopupMenu* file = new Q3PopupMenu( this );Q_ASSERT(file!=NULL);
    file->insertItem( tr("&New Window"), this, SLOT( newWindow() ), Qt::CTRL+Qt::Key_N );
    file->insertItem( tr("&Print"), this, SLOT( print() ), Qt::CTRL+Qt::Key_P );
    file->insertSeparator();
    file->insertItem( tr("&Close"), this, SLOT( close() ), Qt::CTRL+Qt::Key_Q );

    // The same three icons are used twice each.

    QPixmap icon_back = QPixmap(help_back) ;
    QPixmap icon_forward = QPixmap(help_forward);
    QPixmap icon_home = QPixmap(help_home);

    Q3PopupMenu* go = new Q3PopupMenu( this );Q_ASSERT(go!=NULL);
    backwardId = go->insertItem( icon_back,
                 tr("&Backward"), browser, SLOT( backward() ),
                 Qt::CTRL+Qt::Key_Left );
    forwardId = go->insertItem( icon_forward,
                tr("&Forward"), browser, SLOT( forward() ),
                Qt::CTRL+Qt::Key_Right );
    go->insertItem( icon_home, tr("&Home"), browser, SLOT( home() ) );

    hist = new Q3PopupMenu( this );Q_ASSERT(hist!=NULL);
    QStringList::Iterator it = history.begin();
    for ( ; it != history.end(); ++it )
    {
        mHistory[ hist->insertItem( *it ) ] = *it;
    }

    connect( hist, SIGNAL( activated( int ) ),
         this, SLOT( histChosen( int ) ) );

    bookm = new Q3PopupMenu( this );Q_ASSERT(bookm!=NULL);
    bookm->insertItem( tr( "Add Bookmark" ), this, SLOT( addBookmark() ) );
    bookm->insertSeparator();

    QStringList::Iterator it2 = bookmarks.begin();
    for ( ; it2 != bookmarks.end(); ++it2 )
    {
        mBookmarks[ bookm->insertItem( *it2 ) ] = *it2;
    }

    connect( bookm, SIGNAL( activated( int ) ),
         this, SLOT( bookmChosen( int ) ) );

    menuBar()->insertItem( tr("&File"), file );
    menuBar()->insertItem( tr("&Go"), go );
    menuBar()->insertItem( tr( "History" ), hist );
    menuBar()->insertItem( tr( "Bookmarks" ), bookm );

    menuBar()->setItemEnabled( forwardId, FALSE);
    menuBar()->setItemEnabled( backwardId, FALSE);

    connect( browser, SIGNAL( backwardAvailable(bool)),this, SLOT(setBackwardAvailable(bool)));
    connect( browser, SIGNAL( forwardAvailable(bool)),this, SLOT(setForwardAvailable(bool)));

    Q3ToolBar* toolbar = new Q3ToolBar( this );Q_ASSERT(toolbar!=NULL);
    addToolBar( toolbar, "Toolbar");
    QToolButton* button;

    button = new QToolButton( icon_back, tr("Backward"), "", browser, SLOT(backward()), toolbar );
    Q_ASSERT(button!=NULL);
    connect( browser, SIGNAL( backwardAvailable(bool) ), button, SLOT( setEnabled(bool) ) );
    button->setEnabled( FALSE );
    button = new QToolButton( icon_forward, tr("Forward"), "", browser, SLOT(forward()), toolbar );
    Q_ASSERT(button!=NULL);
    connect( browser, SIGNAL( forwardAvailable(bool) ), button, SLOT( setEnabled(bool) ) );
    button->setEnabled( FALSE );
    button = new QToolButton( icon_home, tr("Home"), "", browser, SLOT(home()), toolbar );
    Q_ASSERT(button!=NULL);

    toolbar->addSeparator();

    pathCombo = new QComboBox( TRUE, toolbar );Q_ASSERT(pathCombo!=NULL);
    connect( pathCombo, SIGNAL( activated( const QString & ) ),
         this, SLOT( pathSelected( const QString & ) ) );

    toolbar->setStretchableWidget( pathCombo );
    setRightJustification( TRUE );
    setDockEnabled( Qt::DockLeft, FALSE );
    setDockEnabled( Qt::DockRight, FALSE );

    pathCombo->insertItem( home_ );
    browser->setFocus();

}


void
HelpWindow::setBackwardAvailable( bool b)
{
    menuBar()->setItemEnabled( backwardId, b);
}

void
HelpWindow::setForwardAvailable( bool b)
{
    menuBar()->setItemEnabled( forwardId, b);
}


void
HelpWindow::textChanged()
{
    if ( browser->documentTitle().isNull() )
    {
        setCaption( "Helpviewer - " + browser->context() );
    }
    else
    {
        setCaption( "Helpviewer - " + browser->documentTitle() ) ;
    }

    selectedURL = browser->context();

    if ( !selectedURL.isEmpty() && pathCombo )
    {
        bool exists = FALSE;
        int i;
        for ( i = 0; i < pathCombo->count(); ++i )
        {
            if ( pathCombo->text( i ) == selectedURL )
            {
                exists = TRUE;
                break;
            }
        }
        if ( !exists )
        {
            pathCombo->insertItem( selectedURL, 0 );
            pathCombo->setCurrentItem( 0 );
            mHistory[ hist->insertItem( selectedURL ) ] = selectedURL;
        }
        else
        {
            pathCombo->setCurrentItem( i );
        }
        selectedURL = QString::null;
    }
}

HelpWindow::~HelpWindow()
{
    history.clear();
    QMap<int, QString>::Iterator it = mHistory.begin();

    for ( ; it != mHistory.end(); ++it )
    {
        history.append( *it );
    }

    QFile f( QDir::currentDirPath() + "/.history" );
    f.open( QIODevice::WriteOnly );
    QDataStream s( &f );
    s << history;
    f.close();

    bookmarks.clear();
    QMap<int, QString>::Iterator it2 = mBookmarks.begin();

    for ( ; it2 != mBookmarks.end(); ++it2 )
    {
        bookmarks.append( *it2 );
    }

    QFile f2( QDir::currentDirPath() + "/.bookmarks" );
    f2.open( QIODevice::WriteOnly );
    QDataStream s2( &f2 );
    s2 << bookmarks;
    f2.close();
}

void
HelpWindow::newWindow()
{
    ( new HelpWindow (browser->source(),browser->mimeSourceFactory()->filePath(),
          myParent,"hbrowser") )->show();
}

void
HelpWindow::print()
{
    QPrinter printer;//(QPrinter::HighResolution );
    printer.setFullPage(TRUE);
    if ( printer.setup( this ) )
    {
        QPainter p( &printer );
        Q3PaintDeviceMetrics metrics(p.device());
        int dpix = metrics.logicalDpiX();
        int dpiy = metrics.logicalDpiY();
        const int margin = 72; // pt

        QRect body(margin*dpix/72, margin*dpiy/72,
               metrics.width()-margin*dpix/72*2,
               metrics.height()-margin*dpiy/72*2 );

        QFont font("times");
        QStringList filePaths = browser->mimeSourceFactory()->filePath();
        QString file;
        QStringList::Iterator it = filePaths.begin();

        for ( ; it != filePaths.end(); ++it )
        {
            file = Q3Url( *it, Q3Url( browser->source() ).path() ).path();
            if ( QFile::exists( file ) )
            {
                break;
            }
            else
            {
                file = QString::null;
            }
        }

        if ( file.isEmpty() )
        {
            return;
        }

        QFile f( file );
        if ( !f.open( QIODevice::ReadOnly ) )
        {
            return;
        }

        Q3TextStream ts( &f );
        Q3SimpleRichText richText( ts.read(), font, browser->context(), browser->styleSheet(),
                      browser->mimeSourceFactory(), body.height() );

        richText.setWidth( &p, body.width() );
        QRect view( body );
        int page = 1;
        do
        {
            richText.draw( &p, body.left(), body.top(), view, colorGroup() );
            view.moveBy( 0, body.height() );
            p.translate( 0 , -body.height() );
            p.setFont( font );

            p.drawText( view.right() - p.fontMetrics().width( QString::number(page) ),
                view.bottom() + p.fontMetrics().ascent() + 5, QString::number(page) );

            if ( view.top()  >= richText.height() )
            {
                break;
            }

            printer.newPage();
            page++;
        }
        while (TRUE);
    }
}

void
HelpWindow::pathSelected( const QString &_path )
{
    browser->setSource( _path );
    QMap<int, QString>::Iterator it = mHistory.begin();
    bool exists = FALSE;

    for ( ; it != mHistory.end(); ++it )
    {
        if ( *it == _path )
        {
            exists = TRUE;
            break;
        }
    }

    if ( !exists )
    {
        mHistory[ hist->insertItem( _path ) ] = _path;
    }
}

void
HelpWindow::readHistory()
{
    if ( QFile::exists( QDir::currentDirPath() + "/.history" ) )
    {
        QFile f( QDir::currentDirPath() + "/.history" );
        f.open( QIODevice::ReadOnly );
        QDataStream s( &f );
        s >> history;
        f.close();
        while ( history.count() > 20 )
        {
            history.remove( history.begin() );
        }
    }
}

void
HelpWindow::readBookmarks()
{
    if ( QFile::exists( QDir::currentDirPath() + "/.bookmarks" ) )
    {
        QFile f( QDir::currentDirPath() + "/.bookmarks" );
        f.open( QIODevice::ReadOnly );
        QDataStream s( &f );
        s >> bookmarks;
        f.close();
    }
}

void
HelpWindow::histChosen( int i )
{
    if ( mHistory.contains( i ) )
    {
        browser->setSource( mHistory[ i ] );
    }
}

void
HelpWindow::bookmChosen( int i )
{
    if ( mBookmarks.contains( i ) )
    {
        browser->setSource( mBookmarks[ i ] );
    }
}

void
HelpWindow::addBookmark()
{
    mBookmarks[ bookm->insertItem( caption() ) ] = browser->context();
}



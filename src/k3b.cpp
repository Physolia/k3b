/*
    SPDX-FileCopyrightText: 1998-2009 Sebastian Trueg <trueg@k3b.org>
    SPDX-FileCopyrightText: 2009-2011 Michal Malek <michalm@jabster.pl>
    SPDX-FileCopyrightText: 1998-2009 Sebastian Trueg <trueg@k3b.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "k3b.h"
#include "k3bappdevicemanager.h"
#include "k3bapplication.h"
#include "k3baudiodecoder.h"
#include "k3baudiodoc.h"
#include "k3baudiotrackdialog.h"
#include "k3baudioview.h"
#include "k3bcuefileparser.h"
#include "k3bdatadoc.h"
#include "k3bdataview.h"
#include "k3bdeviceselectiondialog.h"
#include "k3bdirview.h"
#include "k3bexternalbinmanager.h"
#include "k3bfiletreeview.h"
#include "k3bglobals.h"
#include "k3binterface.h"
#include "k3biso9660.h"
#include "k3bjob.h"
#include "k3bmediacache.h"
#include "k3bmediaselectiondialog.h"
#include "k3bmedium.h"
#include "k3bmixeddoc.h"
#include "k3bmixedview.h"
#include "k3bmovixdoc.h"
#include "k3bmovixview.h"
#include "k3bprojectburndialog.h"
#include "k3bplugin.h"
#include "k3bpluginmanager.h"
#include "k3bprojectmanager.h"
#include "k3bprojecttabwidget.h"
#include "k3bsignalwaiter.h"
#include "k3bstdguiitems.h"
#include "k3bsystemproblemdialog.h"
#include "k3bstatusbarmanager.h"
#include "k3btempdirselectionwidget.h"
#include "k3bthemedheader.h"
#include "k3bthememanager.h"
#include "k3burlnavigator.h"
#include "k3bvcddoc.h"
#include "k3bvcdview.h"
#include "k3bvideodvddoc.h"
#include "k3bvideodvdview.h"
#include "k3bview.h"
#include "k3bwelcomewidget.h"
#include "misc/k3bimagewritingdialog.h"
#include "misc/k3bmediacopydialog.h"
#include "misc/k3bmediaformattingdialog.h"
#include "option/k3boptiondialog.h"
#include "projects/k3bdatamultisessionimportdialog.h"

#include <KConfig>
#include <KSharedConfig>
#include <KRecentFilesAction>
#include <KStandardAction>
#include <KAboutData>
#include <KProcess>
#include <KLocalizedString>
#include <KIO/DeleteJob>
#include <KIO/StatJob>
#include <KRecentDocument>
#include <KFilePlacesModel>
#include <KActionMenu>
#include <KMessageBox>
#include <KToggleAction>
#include <KActionCollection>
#include <KEditToolBar>
#include <KXMLGUIFactory>
#include <KShortcutsDialog>

#include <QtAlgorithms>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QAction>
#include <QFileDialog>
#include <QGridLayout>
#include <QLayout>
#include <QMenuBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>

#include <cstdlib>


namespace {

    bool isProjectFile( QMimeDatabase const& mimeDatabase, QUrl const& url )
    {
        return mimeDatabase.mimeTypeForUrl( url ).inherits( "application/x-k3b" );
    }


    bool isDiscImage( const QUrl& url )
    {
        K3b::Iso9660 iso( url.toLocalFile() );
        if( iso.open() ) {
            iso.close();
            return true;
        }

        K3b::CueFileParser parser( url.toLocalFile() );
        if( parser.isValid() &&
            (parser.toc().contentType() == K3b::Device::DATA || parser.toc().contentType() == K3b::Device::MIXED) ) {
            return true;
        }

        return false;
    }


    bool areAudioFiles( const QList<QUrl>& urls )
    {
        // check if the files are all audio we can handle. If so create an audio project
        bool audio = true;
        QList<K3b::Plugin*> fl = k3bcore->pluginManager()->plugins( "AudioDecoder" );
        for( QList<QUrl>::const_iterator it = urls.begin(); it != urls.end(); ++it ) {
            const QUrl& url = *it;

            if( QFileInfo(url.toLocalFile()).isDir() ) {
                audio = false;
                break;
            }

            bool a = false;
            Q_FOREACH( K3b::Plugin* plugin, fl ) {
                if( static_cast<K3b::AudioDecoderFactory*>( plugin )->canDecode( url ) ) {
                    a = true;
                    break;
                }
            }
            if( !a ) {
                audio = a;
                break;
            }
        }

        if( !audio && urls.count() == 1 ) {
            // see if it's an audio cue file
            K3b::CueFileParser parser( urls.first().toLocalFile() );
            if( parser.isValid() && parser.toc().contentType() == K3b::Device::AUDIO ) {
                audio = true;
            }
        }
        return audio;
    }
    
    void setEqualSizes( QSplitter* splitter )
    {
        QList<int> sizes = splitter->sizes();
        int sum = 0;
        for( int i = 0; i < sizes.count(); ++i )
        {
            sum += sizes.at( i );
        }

        std::fill( sizes.begin(), sizes.end(), sum / sizes.count() );
        splitter->setSizes( sizes );
    }

} // namespace


class K3b::MainWindow::Private
{
public:
    KRecentFilesAction* actionFileOpenRecent;
    QAction* actionFileSave;
    QAction* actionFileSaveAs;
    QAction* actionFileClose;
    KToggleAction* actionViewStatusBar;
    KToggleAction* actionViewDocumentHeader;

    /** The MDI-Interface is managed by this tabbed view */
    ProjectTabWidget* documentTab;

    // project actions
    QList<QAction*> dataProjectActions;

    // The K3b-specific widgets
    DirView* dirView;
    OptionDialog* optionDialog;

    StatusBarManager* statusBarManager;

    bool initialized;

    // the funny header
    ThemedHeader* documentHeader;

    KFilePlacesModel* filePlacesModel;
    K3b::UrlNavigator* urlNavigator;

    K3b::Doc* lastDoc;

    QSplitter* mainSplitter;
    K3b::WelcomeWidget* welcomeWidget;
    QStackedWidget* documentStack;
    QWidget* documentHull;

    QMimeDatabase mimeDatabase;
};

K3b::MainWindow::MainWindow()
    : KXmlGuiWindow(0),
      d( new Private )
{
    d->lastDoc = 0;

    setPlainCaption( i18n("K3b - The CD and DVD Kreator") );

    // /////////////////////////////////////////////////////////////////
    // call inits to invoke all other construction parts
    initActions();
    initView();
    initStatusBar();
    createGUI();

    // /////////////////////////////////////////////////////////////////
    // incorporate Device Manager into main window
    factory()->addClient( k3bappcore->appDeviceManager() );
    connect( k3bappcore->appDeviceManager(), SIGNAL(detectingDiskInfo(K3b::Device::Device*)),
             this, SLOT(showDiskInfo(K3b::Device::Device*)) );

    // we need the actions for the welcomewidget
    KConfigGroup grp( config(), "Welcome Widget" );
    d->welcomeWidget->loadConfig( grp );

    // fill the tabs action menu
    d->documentTab->addAction( d->actionFileSave );
    d->documentTab->addAction( d->actionFileSaveAs );
    d->documentTab->addAction( d->actionFileClose );

    // /////////////////////////////////////////////////////////////////
    // disable actions at startup
    slotStateChanged( "state_project_active", KXMLGUIClient::StateReverse );

    connect( k3bappcore->projectManager(), SIGNAL(newProject(K3b::Doc*)), this, SLOT(createClient(K3b::Doc*)) );
    connect( k3bcore->deviceManager(), SIGNAL(changed()), this, SLOT(slotCheckSystemTimed()) );

    // FIXME: now make sure the welcome screen is displayed completely
    //resize( 780, 550 );
//   getMainDockWidget()->resize( getMainDockWidget()->size().expandedTo( d->welcomeWidget->sizeHint() ) );
//   d->dirTreeDock->resize( QSize( d->dirTreeDock->sizeHint().width(), d->dirTreeDock->height() ) );

    readOptions();

    new Interface( this );
}

K3b::MainWindow::~MainWindow()
{
    delete d;
}


KSharedConfig::Ptr K3b::MainWindow::config() const
{
    return KSharedConfig::openConfig();
}


void K3b::MainWindow::initActions()
{
    // merge in the device actions from the device manager
    // operator+= is deprecated but I know no other way to do this. Why does the KDE app framework
    // need to have all actions in the mainwindow's actioncollection anyway (or am I just to stupid to
    // see the correct solution?)

    // clang-analyzer wrongly treat KF5's KStandardAction::open as Unix API Improper use of 'open'
    QAction* actionFileOpen = KStandardAction::open( this, SLOT(slotFileOpen()), actionCollection() );
    actionFileOpen->setToolTip( i18n( "Opens an existing project" ) );
    actionFileOpen->setStatusTip( actionFileOpen->toolTip() );

    d->actionFileOpenRecent = KStandardAction::openRecent( this, SLOT(slotFileOpenRecent(QUrl)), actionCollection() );
    d->actionFileOpenRecent->setToolTip( i18n( "Opens a recently used file" ) );
    d->actionFileOpenRecent->setStatusTip( d->actionFileOpenRecent->toolTip() );

    d->actionFileSave = KStandardAction::save( this, SLOT(slotFileSave()), actionCollection() );
    d->actionFileSave->setToolTip( i18n( "Saves the current project" ) );
    d->actionFileSave->setStatusTip( d->actionFileSave->toolTip() );

    d->actionFileSaveAs = KStandardAction::saveAs( this, SLOT(slotFileSaveAs()), actionCollection() );
    d->actionFileSaveAs->setToolTip( i18n( "Saves the current project to a new URL" ) );
    d->actionFileSaveAs->setStatusTip( d->actionFileSaveAs->toolTip() );

    QAction* actionFileSaveAll = new QAction( QIcon::fromTheme( "document-save-all" ), i18n("Save All"), this );
    actionFileSaveAll->setToolTip( i18n( "Saves all open projects" ) );
    actionFileSaveAll->setStatusTip( actionFileSaveAll->toolTip() );
    actionCollection()->addAction( "file_save_all", actionFileSaveAll );
    connect( actionFileSaveAll, SIGNAL(triggered(bool)), this, SLOT(slotFileSaveAll()) );

    d->actionFileClose = KStandardAction::close( this, SLOT(slotFileClose()), actionCollection() );
    d->actionFileClose->setToolTip(i18n("Closes the current project"));
    d->actionFileClose->setStatusTip( d->actionFileClose->toolTip() );

    QAction* actionFileCloseAll = new QAction( i18n("Close All"), this );
    actionFileCloseAll->setToolTip(i18n("Closes all open projects"));
    actionFileCloseAll->setStatusTip( actionFileCloseAll->toolTip() );
    actionCollection()->addAction( "file_close_all", actionFileCloseAll );
    connect( actionFileCloseAll, SIGNAL(triggered(bool)), this, SLOT(slotFileCloseAll()) );

    QAction* actionFileQuit = KStandardAction::quit(this, SLOT(slotFileQuit()), actionCollection());
    actionFileQuit->setToolTip(i18n("Quits the application"));
    actionFileQuit->setStatusTip( actionFileQuit->toolTip() );

    QAction* actionFileNewAudio = new QAction( QIcon::fromTheme( "media-optical-audio" ), i18n("New &Audio CD Project"), this );
    actionFileNewAudio->setToolTip( i18n("Creates a new audio CD project") );
    actionFileNewAudio->setStatusTip( actionFileNewAudio->toolTip() );
    actionCollection()->addAction( "file_new_audio", actionFileNewAudio );
    connect( actionFileNewAudio, SIGNAL(triggered(bool)), this, SLOT(slotNewAudioDoc()) );

    QAction* actionFileNewData = new QAction( QIcon::fromTheme( "media-optical-data" ), i18n("New &Data Project"), this );
    actionFileNewData->setToolTip( i18n("Creates a new data project") );
    actionFileNewData->setStatusTip( actionFileNewData->toolTip() );
    actionCollection()->addAction( "file_new_data", actionFileNewData );
    connect( actionFileNewData, SIGNAL(triggered(bool)), this, SLOT(slotNewDataDoc()) );

    QAction* actionFileNewMixed = new QAction( QIcon::fromTheme( "media-optical-mixed-cd" ), i18n("New &Mixed Mode CD Project"), this );
    actionFileNewMixed->setToolTip( i18n("Creates a new mixed audio/data CD project") );
    actionFileNewMixed->setStatusTip( actionFileNewMixed->toolTip() );
    actionCollection()->addAction( "file_new_mixed", actionFileNewMixed );
    connect( actionFileNewMixed, SIGNAL(triggered(bool)), this, SLOT(slotNewMixedDoc()) );

    QAction* actionFileNewVcd = new QAction( QIcon::fromTheme( "media-optical-video" ), i18n("New &Video CD Project"), this );
    actionFileNewVcd->setToolTip( i18n("Creates a new Video CD project") );
    actionFileNewVcd->setStatusTip( actionFileNewVcd->toolTip() );
    actionCollection()->addAction( "file_new_vcd", actionFileNewVcd );
    connect( actionFileNewVcd, SIGNAL(triggered(bool)), this, SLOT(slotNewVcdDoc()) );

    QAction* actionFileNewMovix = new QAction( QIcon::fromTheme( "media-optical-video" ), i18n("New &eMovix Project"), this );
    actionFileNewMovix->setToolTip( i18n("Creates a new eMovix project") );
    actionFileNewMovix->setStatusTip( actionFileNewMovix->toolTip() );
    actionCollection()->addAction( "file_new_movix", actionFileNewMovix );
    connect( actionFileNewMovix, SIGNAL(triggered(bool)), this, SLOT(slotNewMovixDoc()) );

    QAction* actionFileNewVideoDvd = new QAction( QIcon::fromTheme( "media-optical-video" ), i18n("New V&ideo DVD Project"), this );
    actionFileNewVideoDvd->setToolTip( i18n("Creates a new Video DVD project") );
    actionFileNewVideoDvd->setStatusTip( actionFileNewVideoDvd->toolTip() );
    actionCollection()->addAction( "file_new_video_dvd", actionFileNewVideoDvd );
    connect( actionFileNewVideoDvd, SIGNAL(triggered(bool)), this, SLOT(slotNewVideoDvdDoc()) );

    QAction* actionFileContinueMultisession = new QAction( QIcon::fromTheme( "media-optical-data" ), i18n("Continue Multisession Project"), this );
    actionFileContinueMultisession->setToolTip( i18n( "Continues multisession project" ) );
    actionFileContinueMultisession->setStatusTip( actionFileContinueMultisession->toolTip() );
    actionCollection()->addAction( "file_continue_multisession", actionFileContinueMultisession );
    connect( actionFileContinueMultisession, SIGNAL(triggered(bool)), this, SLOT(slotContinueMultisession()) );

    KActionMenu* actionFileNewMenu = new KActionMenu( i18n("&New Project"),this );
    actionFileNewMenu->setIcon( QIcon::fromTheme( "document-new" ) );
    actionFileNewMenu->setToolTip(i18n("Creates a new project"));
    actionFileNewMenu->setStatusTip( actionFileNewMenu->toolTip() );
    actionFileNewMenu->setPopupMode( QToolButton::InstantPopup );
    actionFileNewMenu->addAction( actionFileNewData );
    actionFileNewMenu->addAction( actionFileContinueMultisession );
    actionFileNewMenu->addSeparator();
    actionFileNewMenu->addAction( actionFileNewAudio );
    actionFileNewMenu->addAction( actionFileNewMixed );
    actionFileNewMenu->addSeparator();
    actionFileNewMenu->addAction( actionFileNewVcd );
    actionFileNewMenu->addAction( actionFileNewVideoDvd );
    actionFileNewMenu->addSeparator();
    actionFileNewMenu->addAction( actionFileNewMovix );
    actionCollection()->addAction( "file_new", actionFileNewMenu );

    QAction* actionProjectAddFiles = new QAction( QIcon::fromTheme( "document-open" ), i18n("&Add Files..."), this );
    actionProjectAddFiles->setToolTip( i18n("Add files to the current project") );
    actionProjectAddFiles->setStatusTip( actionProjectAddFiles->toolTip() );
    actionCollection()->addAction( "project_add_files", actionProjectAddFiles );
    connect( actionProjectAddFiles, SIGNAL(triggered(bool)), this, SLOT(slotProjectAddFiles()) );

    QAction* actionClearProject = new QAction( QIcon::fromTheme( QApplication::isRightToLeft() ? "edit-clear-locationbar-rtl" : "edit-clear-locationbar-ltr" ), i18n("&Clear Project"), this );
    actionClearProject->setToolTip( i18n("Clear the current project") );
    actionClearProject->setStatusTip( actionClearProject->toolTip() );
    actionCollection()->addAction( "project_clear_project", actionClearProject );
    connect( actionClearProject, SIGNAL(triggered(bool)), this, SLOT(slotClearProject()) );

    QAction* actionToolsFormatMedium = new QAction( QIcon::fromTheme( "tools-media-optical-format" ), i18n("&Format/Erase rewritable disk..."), this );
    actionToolsFormatMedium->setIconText( i18n( "Format" ) );
    actionToolsFormatMedium->setToolTip( i18n("Open the rewritable disk formatting/erasing dialog") );
    actionToolsFormatMedium->setStatusTip( actionToolsFormatMedium->toolTip() );
    actionCollection()->addAction( "tools_format_medium", actionToolsFormatMedium );
    connect( actionToolsFormatMedium, SIGNAL(triggered(bool)), this, SLOT(slotFormatMedium()) );

    QAction* actionToolsWriteImage = new QAction( QIcon::fromTheme( "tools-media-optical-burn-image" ), i18n("&Burn Image..."), this );
    actionToolsWriteImage->setToolTip( i18n("Write an ISO 9660, cue/bin, or cdrecord clone image to an optical disc") );
    actionToolsWriteImage->setStatusTip( actionToolsWriteImage->toolTip() );
    actionCollection()->addAction( "tools_write_image", actionToolsWriteImage );
    connect( actionToolsWriteImage, SIGNAL(triggered(bool)), this, SLOT(slotWriteImage()) );

    QAction* actionToolsMediaCopy = new QAction( QIcon::fromTheme( "tools-media-optical-copy" ), i18n("Copy &Medium..."), this );
    actionToolsMediaCopy->setIconText( i18n( "Copy" ) );
    actionToolsMediaCopy->setToolTip( i18n("Open the media copy dialog") );
    actionToolsMediaCopy->setStatusTip( actionToolsMediaCopy->toolTip() );
    actionCollection()->addAction( "tools_copy_medium", actionToolsMediaCopy );
    connect( actionToolsMediaCopy, SIGNAL(triggered(bool)), this, SLOT(slotMediaCopy()) );

    QAction* actionToolsCddaRip = new QAction( QIcon::fromTheme( "tools-rip-audio-cd" ), i18n("Rip Audio CD..."), this );
    actionToolsCddaRip->setToolTip( i18n("Digitally extract tracks from an audio CD") );
    actionToolsCddaRip->setStatusTip( actionToolsCddaRip->toolTip() );
    actionCollection()->addAction( "tools_cdda_rip", actionToolsCddaRip );
    connect( actionToolsCddaRip, SIGNAL(triggered(bool)), this, SLOT(slotCddaRip()) );

    QAction* actionToolsVideoDvdRip = new QAction( QIcon::fromTheme( "tools-rip-video-dvd" ), i18n("Rip Video DVD..."), this );
    actionToolsVideoDvdRip->setToolTip( i18n("Transcode Video DVD titles") );
    actionToolsVideoDvdRip->setStatusTip( actionToolsVideoDvdRip->toolTip() );
    connect( actionToolsVideoDvdRip, SIGNAL(triggered(bool)), this, SLOT(slotVideoDvdRip()) );
    actionCollection()->addAction( "tools_videodvd_rip", actionToolsVideoDvdRip );

    QAction* actionToolsVideoCdRip = new QAction( QIcon::fromTheme( "tools-rip-video-cd" ), i18n("Rip Video CD..."), this );
    actionToolsVideoCdRip->setToolTip( i18n("Extract tracks from a Video CD") );
    actionToolsVideoCdRip->setStatusTip( actionToolsVideoCdRip->toolTip() );
    actionCollection()->addAction( "tools_videocd_rip", actionToolsVideoCdRip );
    connect( actionToolsVideoCdRip, SIGNAL(triggered(bool)), this, SLOT(slotVideoCdRip()) );

    d->actionViewDocumentHeader = new KToggleAction(i18n("Show Projects Header"),this);
    d->actionViewDocumentHeader->setToolTip( i18n("Shows/hides title header of projects panel") );
    d->actionViewDocumentHeader->setStatusTip( d->actionViewDocumentHeader->toolTip() );
    actionCollection()->addAction("view_document_header", d->actionViewDocumentHeader);

    d->actionViewStatusBar = KStandardAction::showStatusbar(this, SLOT(slotViewStatusBar()), actionCollection());
    KStandardAction::showMenubar( this, SLOT(slotShowMenuBar()), actionCollection() );
    KStandardAction::keyBindings( this, SLOT(slotConfigureKeys()), actionCollection() );
    KStandardAction::configureToolbars(this, SLOT(slotEditToolbars()), actionCollection());
    setStandardToolBarMenuEnabled(true);

    QAction* actionSettingsConfigure = KStandardAction::preferences(this, SLOT(slotSettingsConfigure()), actionCollection() );
    actionSettingsConfigure->setToolTip( i18n("Configure K3b settings") );
    actionSettingsConfigure->setStatusTip( actionSettingsConfigure->toolTip() );

    QAction* actionHelpSystemCheck = new QAction( i18n("System Check"), this );
    actionHelpSystemCheck->setToolTip( i18n("Checks system configuration") );
    actionHelpSystemCheck->setStatusTip( actionHelpSystemCheck->toolTip() );
    actionCollection()->addAction( "help_check_system", actionHelpSystemCheck );
    connect( actionHelpSystemCheck, SIGNAL(triggered(bool)), this, SLOT(slotManualCheckSystem()) );
}



QList<K3b::Doc*> K3b::MainWindow::projects() const
{
    return k3bappcore->projectManager()->projects();
}


void K3b::MainWindow::slotConfigureKeys()
{
    KShortcutsDialog::showDialog( actionCollection(), KShortcutsEditor::LetterShortcutsDisallowed, this );
}

void K3b::MainWindow::initStatusBar()
{
    d->statusBarManager = new K3b::StatusBarManager( this );
}


void K3b::MainWindow::initView()
{
    // setup main docking things
    d->mainSplitter = new QSplitter( Qt::Vertical, this );
    
    QSplitter* upperSplitter = new QSplitter( Qt::Horizontal, d->mainSplitter );
    d->mainSplitter->addWidget( upperSplitter );

    // --- Document Dock ----------------------------------------------------------------------------
    d->documentStack = new QStackedWidget( d->mainSplitter );
    d->mainSplitter->addWidget( d->documentStack );
    d->documentHull = new QWidget( d->documentStack );
    QGridLayout* documentHullLayout = new QGridLayout( d->documentHull );
    documentHullLayout->setContentsMargins( 0, 0, 0, 0 );
    documentHullLayout->setSpacing( 0 );
    
    setCentralWidget( d->mainSplitter );
    setEqualSizes( d->mainSplitter );

    d->documentHeader = new K3b::ThemedHeader( d->documentHull );
    d->documentHeader->setTitle( i18n("Current Projects") );
    d->documentHeader->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
    d->documentHeader->setLeftPixmap( K3b::Theme::PROJECT_LEFT );
    d->documentHeader->setRightPixmap( K3b::Theme::PROJECT_RIGHT );
    connect( d->actionViewDocumentHeader, SIGNAL(toggled(bool)),
             d->documentHeader, SLOT(setVisible(bool)) );

    // add the document tab to the styled document box
    d->documentTab = new K3b::ProjectTabWidget( d->documentHull );

    documentHullLayout->addWidget( d->documentHeader, 0, 0 );
    documentHullLayout->addWidget( d->documentTab, 1, 0 );

    connect( d->documentTab, SIGNAL(currentChanged(int)), this, SLOT(slotCurrentDocChanged()) );
    connect( d->documentTab, SIGNAL(tabCloseRequested(Doc*)), this, SLOT(slotFileClose(Doc*)) );

    d->welcomeWidget = new K3b::WelcomeWidget( this, d->documentStack );
    d->documentStack->addWidget( d->welcomeWidget );
    d->documentStack->addWidget( d->documentHull );
    d->documentStack->setCurrentWidget( d->welcomeWidget );
    // ---------------------------------------------------------------------------------------------

    // --- Directory Dock --------------------------------------------------------------------------
    K3b::FileTreeView* fileTreeView = new K3b::FileTreeView( upperSplitter );
    upperSplitter->addWidget( fileTreeView );
    // ---------------------------------------------------------------------------------------------


    // --- Contents Dock ---------------------------------------------------------------------------
    d->dirView = new K3b::DirView( fileTreeView, upperSplitter );
    upperSplitter->addWidget( d->dirView );

    // --- filetreecombobox-toolbar ----------------------------------------------------------------
	d->filePlacesModel = new KFilePlacesModel;
    d->urlNavigator = new K3b::UrlNavigator(d->filePlacesModel, this);
    connect( d->urlNavigator, SIGNAL(activated(QUrl)), d->dirView, SLOT(showUrl(QUrl)) );
    connect( d->urlNavigator, SIGNAL(activated(K3b::Device::Device*)), d->dirView, SLOT(showDevice(K3b::Device::Device*)) );
    connect( d->dirView, SIGNAL(urlEntered(QUrl)), d->urlNavigator, SLOT(setUrl(QUrl)) );
    connect( d->dirView, SIGNAL(deviceSelected(K3b::Device::Device*)), d->urlNavigator, SLOT(setDevice(K3b::Device::Device*)) );
    QWidgetAction * urlNavigatorAction = new QWidgetAction(this);
    urlNavigatorAction->setDefaultWidget(d->urlNavigator);
    urlNavigatorAction->setText(i18n("&Location Bar"));
    actionCollection()->addAction( "location_bar", urlNavigatorAction );
    // ---------------------------------------------------------------------------------------------
}


void K3b::MainWindow::createClient( K3b::Doc* doc )
{
    qDebug();

    // create the proper K3b::View (maybe we should put this into some other class like K3b::ProjectManager)
    K3b::View* view = 0;
    switch( doc->type() ) {
    case K3b::Doc::AudioProject:
        view = new K3b::AudioView( static_cast<K3b::AudioDoc*>(doc), d->documentTab );
        break;
    case K3b::Doc::DataProject:
        view = new K3b::DataView( static_cast<K3b::DataDoc*>(doc), d->documentTab );
        break;
    case K3b::Doc::MixedProject:
    {
        K3b::MixedDoc* mixedDoc = static_cast<K3b::MixedDoc*>(doc);
        view = new K3b::MixedView( mixedDoc, d->documentTab );
        mixedDoc->dataDoc()->setView( view );
        mixedDoc->audioDoc()->setView( view );
        break;
    }
    case K3b::Doc::VcdProject:
        view = new K3b::VcdView( static_cast<K3b::VcdDoc*>(doc), d->documentTab );
        break;
    case K3b::Doc::MovixProject:
        view = new K3b::MovixView( static_cast<K3b::MovixDoc*>(doc), d->documentTab );
        break;
    case K3b::Doc::VideoDvdProject:
        view = new K3b::VideoDvdView( static_cast<K3b::VideoDvdDoc*>(doc), d->documentTab );
        break;
    }

    if( view != 0 ) {
        doc->setView( view );
        view->setWindowTitle( doc->URL().fileName() );

        d->documentTab->addTab( doc );
        d->documentTab->setCurrentTab( doc );

        slotCurrentDocChanged();
    }
}


K3b::View* K3b::MainWindow::activeView() const
{
    if( Doc* doc = activeDoc() )
        return qobject_cast<View*>( doc->view() );
    else
        return 0;
}


K3b::Doc* K3b::MainWindow::activeDoc() const
{
    return d->documentTab->currentTab();
}


K3b::Doc* K3b::MainWindow::openDocument(const QUrl& url)
{
    slotStatusMsg(i18n("Opening file..."));

    //
    // First we check if this is an iso image in case someone wants to open one this way
    //
    if( isDiscImage( url ) ) {
        slotWriteImage( url );
        return 0;
    }
    else {
        // see if it's an audio cue file
        K3b::CueFileParser parser( url.toLocalFile() );
        if( parser.isValid() && parser.toc().contentType() == K3b::Device::AUDIO ) {
            K3b::Doc* doc = k3bappcore->projectManager()->createProject( K3b::Doc::AudioProject );
            doc->addUrl( url );
            return doc;
        }
        else {
            // check, if document already open. If yes, set the focus to the first view
            K3b::Doc* doc = k3bappcore->projectManager()->findByUrl( url );
            if( doc ) {
                d->documentTab->setCurrentTab( doc );
                return doc;
            }

            doc = k3bappcore->projectManager()->openProject( url );

            if( doc == 0 ) {
                KMessageBox::error (this,i18n("Could not open document."), i18n("Error"));
                return 0;
            }

            d->actionFileOpenRecent->addUrl(url);

            return doc;
        }
    }
}


void K3b::MainWindow::saveOptions()
{
    KConfigGroup recentGrp(config(),"Recent Files");
    d->actionFileOpenRecent->saveEntries( recentGrp );

    KConfigGroup grpFileView( config(), "file view" );
    d->dirView->saveConfig( grpFileView );

    KConfigGroup grpWindows(config(), "main_window_settings");
    saveMainWindowSettings( grpWindows );

    k3bcore->saveSettings( config() );

    KConfigGroup grp(config(), "Welcome Widget" );
    d->welcomeWidget->saveConfig( grp );

    KConfigGroup grpOption( config(), "General Options" );
    grpOption.writeEntry( "Show Document Header", d->actionViewDocumentHeader->isChecked() );
    grpOption.writeEntry( "Navigator breadcrumb mode", !d->urlNavigator->isUrlEditable() );

    config()->sync();
}


void K3b::MainWindow::readOptions()
{
    KConfigGroup grpWindow(config(), "main_window_settings");
    applyMainWindowSettings( grpWindow );
    
    KConfigGroup grp( config(), "General Options" );
    d->actionViewDocumentHeader->setChecked( grp.readEntry("Show Document Header", true) );
    d->urlNavigator->setUrlEditable( !grp.readEntry( "Navigator breadcrumb mode", true ) );

    // initialize the recent file list
    KConfigGroup recentGrp(config(), "Recent Files");
    d->actionFileOpenRecent->loadEntries( recentGrp );

    KConfigGroup grpFileView( config(), "file view" );
    d->dirView->readConfig( grpFileView );

    d->documentHeader->setVisible( d->actionViewDocumentHeader->isChecked() );
}


void K3b::MainWindow::saveProperties( KConfigGroup& grp )
{
    // 1. put saved projects in the config
    // 2. save every modified project in  "~/.kde/share/apps/k3b/sessions/" + KApp->sessionId()
    // 3. save the url of the project (might be something like "AudioCD1") in the config
    // 4. save the status of every project (modified/saved)

    QString saveDir = QString( "%1/sessions/%2/" ).arg(
                QStandardPaths::writableLocation( QStandardPaths::AppDataLocation ),
                qApp->sessionId() );
    QDir().mkpath(saveDir);

//     // FIXME: for some reason the config entries are not properly stored when using the default
//     //        KMainWindow session config. Since I was not able to find the bug I use another config object
//     // ----------------------------------------------------------
//     KConfig c( saveDir + "list", KConfig::SimpleConfig );
//     KConfigGroup grp( &c, "Saved Session" );
//     // ----------------------------------------------------------

    QList<K3b::Doc*> docs = k3bappcore->projectManager()->projects();
    grp.writeEntry( "Number of projects", docs.count() );

    int cnt = 1;
    Q_FOREACH( K3b::Doc* doc, docs ) {
        // the "name" of the project (or the original url if isSaved())
        grp.writePathEntry( QString("%1 url").arg(cnt), (doc)->URL().url() );

        // is the doc modified
        grp.writeEntry( QString("%1 modified").arg(cnt), (doc)->isModified() );

        // has the doc already been saved?
        grp.writeEntry( QString("%1 saved").arg(cnt), (doc)->isSaved() );

        // where does the session management save it? If it's not modified and saved this is
        // the same as the url
        QUrl saveUrl = (doc)->URL();
        if( !(doc)->isSaved() || (doc)->isModified() )
            saveUrl = QUrl::fromLocalFile( saveDir + QString::number(cnt) );
        grp.writePathEntry( QString("%1 saveurl").arg(cnt), saveUrl.url() );

        // finally save it
        k3bappcore->projectManager()->saveProject( doc, saveUrl );

        ++cnt;
    }

//    c.sync();
}


// FIXME:move this to K3b::ProjectManager
void K3b::MainWindow::readProperties( const KConfigGroup& grp )
{
    // FIXME: do not delete the files here. rather do it when the app is exited normally
    //        since that's when we can be sure we never need the session stuff again.

    // 1. read all projects from the config
    // 2. simply open all of them
    // 3. reset the saved urls and the modified state
    // 4. delete "~/.kde/share/apps/k3b/sessions/" + KApp->sessionId()

    QString saveDir = QString( "%1/sessions/%2/" ).arg(
                QStandardPaths::writableLocation( QStandardPaths::AppDataLocation ),
                qApp->sessionId() );
    QDir().mkpath(saveDir);

//     // FIXME: for some reason the config entries are not properly stored when using the default
//     //        KMainWindow session config. Since I was not able to find the bug I use another config object
//     // ----------------------------------------------------------
//     KConfig c( saveDir + "list"/*, true*/ );
//     KConfigGroup grp( &c, "Saved Session" );
//     // ----------------------------------------------------------

    int cnt = grp.readEntry( "Number of projects", 0 );
/*
  qDebug() << "(K3b::MainWindow::readProperties) number of projects from last session in " << saveDir << ": " << cnt << Qt::endl
  << "                                read from config group " << c->group() << Qt::endl;
*/
    for( int i = 1; i <= cnt; ++i ) {
        // in this case the constructor works since we saved as url()
        QUrl url( grp.readPathEntry( QString("%1 url").arg(i),QString() ) );

        bool modified = grp.readEntry( QString("%1 modified").arg(i),false );

        bool saved = grp.readEntry( QString("%1 saved").arg(i),false );

        QUrl saveUrl( grp.readPathEntry( QString("%1 saveurl").arg(i),QString() ) );

        // now load the project
        if( K3b::Doc* doc = k3bappcore->projectManager()->openProject( saveUrl ) ) {

            // reset the url
            doc->setURL( url );
            doc->setModified( modified );
            doc->setSaved( saved );
        }
        else
            qDebug() << "(K3b::MainWindow) could not open session saved doc " << url.toLocalFile();

        // remove the temp file
        if( !saved || modified )
            QFile::remove( saveUrl.toLocalFile() );
    }

    // and now remove the temp dir
    KIO::del( QUrl::fromLocalFile(saveDir), KIO::HideProgressInfo );
}


bool K3b::MainWindow::queryClose()
{
    //
    // Check if a job is currently running
    // For now K3b only allows for one major job at a time which means that we only need to cancel
    // this one job.
    //
    if( k3bcore->jobsRunning() ) {

        // pitty, but I see no possibility to make this work. It always crashes because of the event
        // management thing mentioned below. So until I find a solution K3b simply will refuse to close
        // while a job i running
        return false;

//     qDebug() << "(K3b::MainWindow::queryClose) jobs running.";
//     K3b::Job* job = k3bcore->runningJobs().getFirst();

//     // now search for the major job (to be on the safe side although for now no subjobs register with the k3bcore)
//     K3b::JobHandler* jh = job->jobHandler();
//     while( jh->isJob() ) {
//       job = static_cast<K3b::Job*>( jh );
//       jh = job->jobHandler();
//     }

//     qDebug() << "(K3b::MainWindow::queryClose) main job found: " << job->jobDescription();

//     // now job is the major job and jh should be a widget
//     QWidget* progressDialog = dynamic_cast<QWidget*>( jh );

//     qDebug() << "(K3b::MainWindow::queryClose) job active: " << job->active();

//     // now ask the user if he/she really wants to cancel this job
//     if( job->active() ) {
//       if( KMessageBox::questionYesNo( progressDialog ? progressDialog : this,
// 				      i18n("Do you really want to cancel?"),
// 				      i18n("Cancel") ) == KMessageBox::Yes ) {
// 	// cancel the job
// 	qDebug() << "(K3b::MainWindow::queryClose) canceling job.";
// 	job->cancel();

// 	// wait for the job to finish
// 	qDebug() << "(K3b::MainWindow::queryClose) waiting for job to finish.";
// 	K3b::SignalWaiter::waitForJob( job );

// 	// close the progress dialog
// 	if( progressDialog ) {
// 	  qDebug() << "(K3b::MainWindow::queryClose) closing progress dialog.";
// 	  progressDialog->close();
// 	  //
// 	  // now here we have the problem that due to the whole Qt event thing the exec call (or
// 	  // in this case most likely the startJob call) does not return until we leave this method.
// 	  // That means that the progress dialog might be deleted by it's parent below (when we
// 	  // close docs) before it is deleted by the creator (most likely a projectburndialog).
// 	  // That would result in a double deletion and thus a crash.
// 	  // So we just reparent the dialog to 0 here so it's (former) parent won't delete it.
// 	  //
// 	  progressDialog->reparent( 0, QPoint(0,0) );
// 	}

// 	qDebug() << "(K3b::MainWindow::queryClose) job cleanup done.";
//       }
//       else
// 	return false;
//     }
    }
    saveOptions();

    //
    // if we are closed by the session manager everything is fine since we store the
    // current state in saveProperties
    //
    if( qApp->isSavingSession() )
        return true;

    // FIXME: do not close the docs here. Just ask for them to be saved and return false
    //        if the user chose cancel for some doc

    // ---------------------------------
    // we need to manually close all the views to ensure that
    // each of them receives a close-event and
    // the user is asked for every modified doc to save the changes
    // ---------------------------------

    while( K3b::View* view = activeView() ) {
        if( !canCloseDocument(view->doc()) )
            return false;
        closeProject(view->doc());
    }

    return true;
}


bool K3b::MainWindow::canCloseDocument( K3b::Doc* doc )
{
    if( !doc->isModified() )
        return true;

    if( !KConfigGroup( config(), "General Options" ).readEntry( "ask_for_saving_changes_on_exit", true ) )
        return true;

    switch ( KMessageBox::warningTwoActionsCancel( this,
                                                   xi18nc("@info", "Project <resource>%1</resource> has unsaved data.", doc->URL().fileName() ),
                                                   i18n("Closing Project"),
                                                   KStandardGuiItem::save(),
                                                   KStandardGuiItem::discard() ) ) {
    case KMessageBox::PrimaryAction:
        return fileSave( doc );
    case KMessageBox::SecondaryAction:
        return true;
    default:
        return false;
    }
}


/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////


void K3b::MainWindow::slotFileOpen()
{
    slotStatusMsg(i18n("Opening file..."));

    QList<QUrl> urls = QFileDialog::getOpenFileUrls( this,
                                                     i18n("Open Files"),
                                                     QUrl(),
                                                     i18n("K3b Projects (*.k3b)"));
    for( QList<QUrl>::iterator it = urls.begin(); it != urls.end(); ++it ) {
        openDocument( *it );
        d->actionFileOpenRecent->addUrl( *it );
    }
}

void K3b::MainWindow::slotFileOpenRecent(const QUrl& url)
{
    slotStatusMsg(i18n("Opening file..."));

    openDocument(url);
}


void K3b::MainWindow::slotFileSaveAll()
{
    Q_FOREACH( K3b::Doc* doc, k3bappcore->projectManager()->projects() ) {
        fileSave( doc );
    }
}


void K3b::MainWindow::slotFileSave()
{
    if( K3b::Doc* doc = activeDoc() ) {
        fileSave( doc );
    }
}

bool K3b::MainWindow::fileSave( K3b::Doc* doc )
{
    slotStatusMsg(i18n("Saving file..."));

    if( doc == 0 ) {
        doc = activeDoc();
    }

    if( doc != 0 ) {
        if( !doc->isSaved() )
            return fileSaveAs( doc );
        else if( !k3bappcore->projectManager()->saveProject( doc, doc->URL()) )
            KMessageBox::error (this,i18n("Could not save the current document."), i18n("I/O Error"));
    }

    return false;
}


void K3b::MainWindow::slotFileSaveAs()
{
    if( K3b::Doc* doc = activeDoc() ) {
        fileSaveAs( doc );
    }
}


bool K3b::MainWindow::fileSaveAs( K3b::Doc* doc )
{
    slotStatusMsg(i18n("Saving file with a new filename..."));

    if( !doc ) {
        doc = activeDoc();
    }

    if( doc ) {
        // we do not use the static QFileDialog method here to be able to specify a filename suggestion
        QFileDialog dlg( this, i18n("Save As"), QString(), i18n("K3b Projects (*.k3b)") );
        dlg.setAcceptMode( QFileDialog::AcceptSave );
        dlg.selectFile( doc->name() );
        dlg.exec();
        QList<QUrl> urls = dlg.selectedUrls();

        if( !urls.isEmpty() ) {
            QUrl url = urls.front();
            KRecentDocument::add( url );

            if( k3bappcore->projectManager()->saveProject( doc, url ) ) {
                d->actionFileOpenRecent->addUrl(url);
                return true;
            }
            else {
                KMessageBox::error (this,i18n("Could not save the current document."), i18n("I/O Error"));
            }
        }
    }

    return false;
}


void K3b::MainWindow::slotFileClose()
{
    if( K3b::View* view = activeView() ) {
        slotFileClose( view->doc() );
    }
}


void K3b::MainWindow::slotFileClose( Doc* doc )
{
    slotStatusMsg(i18n("Closing file..."));
    if( doc && canCloseDocument(doc) ) {
        closeProject(doc);
    }

    slotCurrentDocChanged();
}


void K3b::MainWindow::slotFileCloseAll()
{
    while( K3b::View* view = activeView() ) {
        K3b::Doc* doc = view->doc();

        if( canCloseDocument(doc) )
            closeProject(doc);
        else
            break;
    }

    slotCurrentDocChanged();
}


void K3b::MainWindow::closeProject( K3b::Doc* doc )
{
    // unplug the actions
    if( factory() ) {
        if( d->lastDoc == doc ) {
            factory()->removeClient( static_cast<K3b::View*>(d->lastDoc->view()) );
            d->lastDoc = 0;
        }
    }

    // remove the doc from the project tab
    d->documentTab->removeTab( doc );

    // remove the project from the manager
    k3bappcore->projectManager()->removeProject( doc );

    // delete view and doc
    delete doc->view();
    delete doc;
}


void K3b::MainWindow::slotFileQuit()
{
    close();
}


void K3b::MainWindow::slotViewStatusBar()
{
    //turn Statusbar on or off
    if(d->actionViewStatusBar->isChecked()) {
        statusBar()->show();
    }
    else {
        statusBar()->hide();
    }
}


void K3b::MainWindow::slotStatusMsg(const QString &text)
{
    ///////////////////////////////////////////////////////////////////
    // change status message permanently
//   statusBar()->clear();
//   statusBar()->setItemText(text,1);

    statusBar()->showMessage( text, 2000 );
}


void K3b::MainWindow::slotSettingsConfigure()
{
    K3b::OptionDialog d( this );

    d.exec();

    // emit a changed signal every time since we do not know if the user selected
    // "apply" and "cancel" or "ok"
    emit configChanged( config() );
}


void K3b::MainWindow::showOptionDialog( K3b::OptionDialog::ConfigPage index )
{
    K3b::OptionDialog d( this);
    d.setCurrentPage( index );

    d.exec();

    // emit a changed signal every time since we do not know if the user selected
    // "apply" and "cancel" or "ok"
    emit configChanged( config() );
}


K3b::Doc* K3b::MainWindow::slotNewAudioDoc()
{
    slotStatusMsg(i18n("Creating new Audio CD Project."));

    K3b::Doc* doc = k3bappcore->projectManager()->createProject( K3b::Doc::AudioProject );

    return doc;
}

K3b::Doc* K3b::MainWindow::slotNewDataDoc()
{
    slotStatusMsg(i18n("Creating new Data CD Project."));

    K3b::Doc* doc = k3bappcore->projectManager()->createProject( K3b::Doc::DataProject );

    return doc;
}


K3b::Doc* K3b::MainWindow::slotContinueMultisession()
{
    return K3b::DataMultisessionImportDialog::importSession( 0, this );
}


K3b::Doc* K3b::MainWindow::slotNewVideoDvdDoc()
{
    slotStatusMsg(i18n("Creating new Video DVD Project."));

    K3b::Doc* doc = k3bappcore->projectManager()->createProject( K3b::Doc::VideoDvdProject );

    return doc;
}


K3b::Doc* K3b::MainWindow::slotNewMixedDoc()
{
    slotStatusMsg(i18n("Creating new Mixed Mode CD Project."));

    K3b::Doc* doc = k3bappcore->projectManager()->createProject( K3b::Doc::MixedProject );

    return doc;
}

K3b::Doc* K3b::MainWindow::slotNewVcdDoc()
{
    slotStatusMsg(i18n("Creating new Video CD Project."));

    K3b::Doc* doc = k3bappcore->projectManager()->createProject( K3b::Doc::VcdProject );

    return doc;
}


K3b::Doc* K3b::MainWindow::slotNewMovixDoc()
{
    slotStatusMsg(i18n("Creating new eMovix Project."));

    K3b::Doc* doc = k3bappcore->projectManager()->createProject( K3b::Doc::MovixProject );

    return doc;
}


void K3b::MainWindow::slotCurrentDocChanged()
{
    // check the doctype
    K3b::View* v = activeView();
    if( v ) {
        k3bappcore->projectManager()->setActive( v->doc() );

        //
        // There are two possiblities to plug the project actions:
        // 1. Through KXMLGUIClient::plugActionList
        //    This way we just ask the View for the actionCollection (which it should merge with
        //    the doc's) and plug it into the project menu.
        //    Advantage: easy and clear to handle
        //    Disadvantage: we may only plug all actions at once into one menu
        //
        // 2. Through merging the doc as a KXMLGUIClient
        //    This way every view is a KXMLGUIClient and it's GUI is just merged into the MainWindow's.
        //    Advantage: flexible
        //    Disadvantage: every view needs it's own XML file
        //
        //

        if( factory() ) {
            if( d->lastDoc )
                factory()->removeClient( static_cast<K3b::View*>(d->lastDoc->view()) );
            factory()->addClient( v );
            d->lastDoc = v->doc();
        }
        else
            qDebug() << "(K3b::MainWindow) ERROR: could not get KXMLGUIFactory instance.";
    }
    else
        k3bappcore->projectManager()->setActive( 0L );

    if( k3bappcore->projectManager()->isEmpty() ) {
        slotStateChanged( "state_project_active", KXMLGUIClient::StateReverse );
    }
    else {
        slotStateChanged( "state_project_active", KXMLGUIClient::StateNoReverse );
    }

    if( k3bappcore->projectManager()->isEmpty() )
        d->documentStack->setCurrentWidget( d->welcomeWidget );
    else
        d->documentStack->setCurrentWidget( d->documentHull );
}


void K3b::MainWindow::slotEditToolbars()
{
    KConfigGroup grp( config(), "main_window_settings" );
    saveMainWindowSettings( grp );
    KEditToolBar dlg( factory() );
    connect( &dlg, SIGNAL(newToolbarConfig()), SLOT(slotNewToolBarConfig()) );
    dlg.exec();
}


void K3b::MainWindow::slotNewToolBarConfig()
{
    KConfigGroup grp(config(), "main_window_settings");
    applyMainWindowSettings(grp);
}


bool K3b::MainWindow::eject()
{
    KConfigGroup c( config(), "General Options" );
    return !c.readEntry( "No cd eject", false );
}


void K3b::MainWindow::slotErrorMessage(const QString& message)
{
    KMessageBox::error( this, message );
}


void K3b::MainWindow::slotWarningMessage(const QString& message)
{
    KMessageBox::error( this, message );
}


void K3b::MainWindow::slotWriteImage()
{
    K3b::ImageWritingDialog d( this );
    d.exec();
}


void K3b::MainWindow::slotWriteImage( const QUrl& url )
{
    K3b::ImageWritingDialog d( this );
    d.setImage( url );
    d.exec();
}


void K3b::MainWindow::slotProjectAddFiles()
{
    K3b::View* view = activeView();

    if( view ) {
        const QList<QUrl> urls = QFileDialog::getOpenFileUrls(this,
                                                              i18n("Select Files to Add to Project"),
                                                              QUrl(),
                                                              i18n("All Files (*)") );


        if( !urls.isEmpty() )
            view->addUrls( urls );
    }
    else
        KMessageBox::error( this, i18n("Please create a project before adding files"), i18n("No Active Project"));
}


void K3b::MainWindow::formatMedium( K3b::Device::Device* dev )
{
    K3b::MediaFormattingDialog d( this );
    d.setDevice( dev );
    d.exec();
}


void K3b::MainWindow::slotFormatMedium()
{
    formatMedium( 0 );
}


void K3b::MainWindow::mediaCopy( K3b::Device::Device* dev )
{
    K3b::MediaCopyDialog d( this );
    d.setReadingDevice( dev );
    d.exec();
}


void K3b::MainWindow::slotMediaCopy()
{
    mediaCopy( 0 );
}


// void K3b::MainWindow::slotVideoDvdCopy()
// {
//   K3b::VideoDvdCopyDialog d( this );
//   d.exec();
// }



void K3b::MainWindow::slotShowMenuBar()
{
    if( menuBar()->isVisible() )
        menuBar()->hide();
    else
        menuBar()->show();
}


K3b::ExternalBinManager* K3b::MainWindow::externalBinManager() const
{
    return k3bcore->externalBinManager();
}


K3b::Device::DeviceManager* K3b::MainWindow::deviceManager() const
{
    return k3bcore->deviceManager();
}


void K3b::MainWindow::slotDataImportSession()
{
    if( activeView() ) {
        if( K3b::DataView* view = qobject_cast<K3b::DataView*>(activeView()) ) {
            view->actionCollection()->action( "project_data_import_session" )->trigger();
        }
    }
}


void K3b::MainWindow::slotDataClearImportedSession()
{
    if( activeView() ) {
        if( K3b::DataView* view = qobject_cast<K3b::DataView*>(activeView()) ) {
            view->actionCollection()->action( "project_data_clear_imported_session" )->trigger();
        }
    }
}


void K3b::MainWindow::slotEditBootImages()
{
    if( activeView() ) {
        if( K3b::DataView* view = qobject_cast<K3b::DataView*>(activeView()) ) {
            view->actionCollection()->action( "project_data_edit_boot_images" )->trigger();
        }
    }
}


void K3b::MainWindow::slotCheckSystemTimed()
{
    // run the system check from the event queue so we do not
    // mess with the device state resetting throughout the app
    // when called from K3b::DeviceManager::changed
    QTimer::singleShot( 0, this, SLOT(slotCheckSystem()) );
}


void K3b::MainWindow::slotCheckSystem()
{
    K3b::SystemProblemDialog::checkSystem( this, K3b::SystemProblemDialog::NotifyOnlyErrors );
}


void K3b::MainWindow::slotManualCheckSystem()
{
    K3b::SystemProblemDialog::checkSystem(this, K3b::SystemProblemDialog::AlwaysNotify, true/* forceCheck */);
}


void K3b::MainWindow::addUrls( const QList<QUrl>& urls )
{
    if( urls.count() == 1 && isProjectFile( d->mimeDatabase, urls.first() ) ) {
        openDocument( urls.first() );
    }
    else if( K3b::View* view = activeView() ) {
        view->addUrls( urls );
    }
    else if( urls.count() == 1 && isDiscImage( urls.first() ) ) {
        slotWriteImage( urls.first() );
    }
    else if( areAudioFiles( urls ) ) {
        static_cast<K3b::View*>(slotNewAudioDoc()->view())->addUrls( urls );
    }
    else {
        static_cast<K3b::View*>(slotNewDataDoc()->view())->addUrls( urls );
    }
}


void K3b::MainWindow::slotClearProject()
{
    K3b::Doc* doc = k3bappcore->projectManager()->activeDoc();
    if( doc ) {
        if( KMessageBox::warningContinueCancel( this,
                                                i18n("Do you really want to clear the current project?"),
                                                i18n("Clear Project"),
                                                KStandardGuiItem::clear(),
                                                KStandardGuiItem::cancel(),
                                                QString("clear_current_project_dontAskAgain") ) == KMessageBox::Continue ) {
            doc->clear();
        }
    }

}


void K3b::MainWindow::slotCddaRip()
{
    cddaRip( 0 );
}


void K3b::MainWindow::cddaRip( K3b::Device::Device* dev )
{
    if( !dev ||
        !(k3bappcore->mediaCache()->medium( dev ).content() & K3b::Medium::ContentAudio ) )
        dev = K3b::MediaSelectionDialog::selectMedium( K3b::Device::MEDIA_CD_ALL,
                                                     K3b::Device::STATE_COMPLETE|K3b::Device::STATE_INCOMPLETE,
                                                     K3b::Medium::ContentAudio,
                                                     this,
                                                     i18n("Audio CD Rip") );

    if( dev )
        d->dirView->showDevice( dev );
}


void K3b::MainWindow::videoDvdRip( K3b::Device::Device* dev )
{
    if( !dev ||
        !(k3bappcore->mediaCache()->medium( dev ).content() & K3b::Medium::ContentVideoDVD ) )
        dev = K3b::MediaSelectionDialog::selectMedium( K3b::Device::MEDIA_DVD_ALL,
                                                     K3b::Device::STATE_COMPLETE,
                                                     K3b::Medium::ContentVideoDVD,
                                                     this,
                                                     i18n("Video DVD Rip") );

    if( dev )
        d->dirView->showDevice( dev );
}


void K3b::MainWindow::slotVideoDvdRip()
{
    videoDvdRip( 0 );
}


void K3b::MainWindow::videoCdRip( K3b::Device::Device* dev )
{
    if( !dev ||
        !(k3bappcore->mediaCache()->medium( dev ).content() & K3b::Medium::ContentVideoCD ) )
        dev = K3b::MediaSelectionDialog::selectMedium( K3b::Device::MEDIA_CD_ALL,
                                                     K3b::Device::STATE_COMPLETE,
                                                     K3b::Medium::ContentVideoCD,
                                                     this,
                                                     i18n("Video CD Rip") );

    if( dev )
        d->dirView->showDevice( dev );
}


void K3b::MainWindow::slotVideoCdRip()
{
    videoCdRip( 0 );
}


void K3b::MainWindow::showDiskInfo( K3b::Device::Device* dev )
{
    d->dirView->showDiskInfo( dev );
}

#include "moc_k3b.cpp"

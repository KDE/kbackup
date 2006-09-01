#include <kdialog.h>
#include <klocale.h>
/****************************************************************************
** Form implementation generated from reading ui file './MainWidget.ui'
**
** Created: Thu Aug 31 22:28:37 2006
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "MainWidget.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <klineedit.h>
#include <qprogressbar.h>
#include <qtextedit.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

#include "Archiver.hxx"
#include "Selector.hxx"
#include "kiconloader.h"
#include "kdirselectdialog.h"
#include "klocale.h"
#include "qtimer.h"
#include "./MainWidget.ui.h"
/*
 *  Constructs a MainWidget as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
MainWidget::MainWidget( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "MainWidget" );
    MainWidgetLayout = new QVBoxLayout( this, KDialog::marginHint(), KDialog::spacingHint(), "MainWidgetLayout"); 

    layout4 = new QHBoxLayout( 0, 0, KDialog::spacingHint(), "layout4"); 

    startButton = new QPushButton( this, "startButton" );
    startButton->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)7, (QSizePolicy::SizeType)0, 0, 0, startButton->sizePolicy().hasHeightForWidth() ) );
    startButton->setMinimumSize( QSize( 0, 50 ) );
    QFont startButton_font(  startButton->font() );
    startButton_font.setPointSize( 14 );
    startButton->setFont( startButton_font ); 
    startButton->setIconSet( QIconSet( SmallIcon( "kbackup_start", 22 ) ) );
    layout4->addWidget( startButton );

    cancelButton = new QPushButton( this, "cancelButton" );
    cancelButton->setEnabled( FALSE );
    cancelButton->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)4, (QSizePolicy::SizeType)0, 0, 0, cancelButton->sizePolicy().hasHeightForWidth() ) );
    cancelButton->setMinimumSize( QSize( 0, 50 ) );
    cancelButton->setIconSet( QIconSet( SmallIcon( "kbackup_cancel", 22 ) ) );
    layout4->addWidget( cancelButton );
    MainWidgetLayout->addLayout( layout4 );

    groupBox3 = new QGroupBox( this, "groupBox3" );
    groupBox3->setColumnLayout(0, Qt::Vertical );
    groupBox3->layout()->setSpacing( KDialog::spacingHint() );
    groupBox3->layout()->setMargin( KDialog::marginHint() );
    groupBox3Layout = new QGridLayout( groupBox3->layout() );
    groupBox3Layout->setAlignment( Qt::AlignTop );

    textLabel2 = new QLabel( groupBox3, "textLabel2" );

    groupBox3Layout->addMultiCellWidget( textLabel2, 0, 0, 0, 1 );

    folder = new QPushButton( groupBox3, "folder" );
    folder->setPixmap( SmallIcon( "folder" ) );

    groupBox3Layout->addWidget( folder, 0, 3 );

    textLabel1 = new QLabel( groupBox3, "textLabel1" );

    groupBox3Layout->addWidget( textLabel1, 0, 4 );

    sliceLabel = new QLabel( groupBox3, "sliceLabel" );
    sliceLabel->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)5, 0, 0, sliceLabel->sizePolicy().hasHeightForWidth() ) );

    groupBox3Layout->addWidget( sliceLabel, 1, 0 );

    sliceNum = new QLabel( groupBox3, "sliceNum" );
    sliceNum->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)5, 0, 0, sliceNum->sizePolicy().hasHeightForWidth() ) );
    sliceNum->setMinimumSize( QSize( 21, 0 ) );

    groupBox3Layout->addWidget( sliceNum, 1, 1 );

    targetDir = new KLineEdit( groupBox3, "targetDir" );

    groupBox3Layout->addWidget( targetDir, 0, 2 );

    progressSlice = new QProgressBar( groupBox3, "progressSlice" );
    progressSlice->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)7, (QSizePolicy::SizeType)0, 0, 0, progressSlice->sizePolicy().hasHeightForWidth() ) );

    groupBox3Layout->addMultiCellWidget( progressSlice, 1, 1, 2, 5 );

    capacity = new QLabel( groupBox3, "capacity" );
    capacity->setMinimumSize( QSize( 90, 0 ) );

    groupBox3Layout->addWidget( capacity, 0, 5 );
    MainWidgetLayout->addWidget( groupBox3 );

    groupBox2 = new QGroupBox( this, "groupBox2" );
    groupBox2->setColumnLayout(0, Qt::Vertical );
    groupBox2->layout()->setSpacing( KDialog::spacingHint() );
    groupBox2->layout()->setMargin( KDialog::marginHint() );
    groupBox2Layout = new QHBoxLayout( groupBox2->layout() );
    groupBox2Layout->setAlignment( Qt::AlignTop );

    textLabel4 = new QLabel( groupBox2, "textLabel4" );
    textLabel4->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)4, (QSizePolicy::SizeType)5, 0, 0, textLabel4->sizePolicy().hasHeightForWidth() ) );
    groupBox2Layout->addWidget( textLabel4 );

    totalFiles = new QLabel( groupBox2, "totalFiles" );
    totalFiles->setAlignment( int( QLabel::AlignVCenter ) );
    groupBox2Layout->addWidget( totalFiles );

    textLabel6 = new QLabel( groupBox2, "textLabel6" );
    textLabel6->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)4, (QSizePolicy::SizeType)5, 0, 0, textLabel6->sizePolicy().hasHeightForWidth() ) );
    groupBox2Layout->addWidget( textLabel6 );

    totalSize = new QLabel( groupBox2, "totalSize" );
    totalSize->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );
    groupBox2Layout->addWidget( totalSize );

    totalSize_2 = new QLabel( groupBox2, "totalSize_2" );
    totalSize_2->setAlignment( int( QLabel::AlignVCenter | QLabel::AlignRight ) );
    groupBox2Layout->addWidget( totalSize_2 );
    spacer1_2 = new QSpacerItem( 10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum );
    groupBox2Layout->addItem( spacer1_2 );

    textLabel8 = new QLabel( groupBox2, "textLabel8" );
    textLabel8->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)4, (QSizePolicy::SizeType)5, 0, 0, textLabel8->sizePolicy().hasHeightForWidth() ) );
    groupBox2Layout->addWidget( textLabel8 );

    elapsedTime = new QLabel( groupBox2, "elapsedTime" );
    groupBox2Layout->addWidget( elapsedTime );
    MainWidgetLayout->addWidget( groupBox2 );

    logLayout = new QVBoxLayout( 0, 0, KDialog::spacingHint(), "logLayout"); 

    log = new QTextEdit( this, "log" );
    log->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)7, (QSizePolicy::SizeType)2, 0, 2, log->sizePolicy().hasHeightForWidth() ) );
    log->setMinimumSize( QSize( 0, 80 ) );
    log->setTextFormat( QTextEdit::LogText );
    log->setWordWrap( QTextEdit::NoWrap );
    log->setReadOnly( TRUE );
    log->setUndoRedoEnabled( FALSE );
    logLayout->addWidget( log );

    layout5 = new QHBoxLayout( 0, 0, KDialog::spacingHint(), "layout5"); 

    textLabel1_2 = new QLabel( this, "textLabel1_2" );
    layout5->addWidget( textLabel1_2 );
    spacer2 = new QSpacerItem( 40, 10, QSizePolicy::Expanding, QSizePolicy::Minimum );
    layout5->addItem( spacer2 );

    fileProgressLabel = new QLabel( this, "fileProgressLabel" );
    layout5->addWidget( fileProgressLabel );

    fileProgress = new QProgressBar( this, "fileProgress" );
    fileProgress->setMaximumSize( QSize( 32767, 18 ) );
    layout5->addWidget( fileProgress );
    logLayout->addLayout( layout5 );

    warnings = new QTextEdit( this, "warnings" );
    warnings->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)7, (QSizePolicy::SizeType)2, 0, 1, warnings->sizePolicy().hasHeightForWidth() ) );
    warnings->setMinimumSize( QSize( 0, 50 ) );
    warnings->setPaletteForegroundColor( QColor( 231, 48, 11 ) );
    warnings->setTextFormat( QTextEdit::LogText );
    warnings->setWordWrap( QTextEdit::NoWrap );
    warnings->setReadOnly( TRUE );
    warnings->setUndoRedoEnabled( FALSE );
    logLayout->addWidget( warnings );
    MainWidgetLayout->addLayout( logLayout );
    languageChange();
    resize( QSize(501, 435).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( startButton, SIGNAL( clicked() ), this, SLOT( startBackup() ) );
    connect( folder, SIGNAL( clicked() ), this, SLOT( getMediaSize() ) );
    connect( targetDir, SIGNAL( returnPressed() ), this, SLOT( getDiskFree() ) );
}

/*
 *  Destroys the object and frees any allocated resources
 */
MainWidget::~MainWidget()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void MainWidget::languageChange()
{
    setCaption( tr2i18n( "KBackup" ) );
    startButton->setText( tr2i18n( "Start Backup" ) );
    cancelButton->setText( tr2i18n( "Cancel Backup" ) );
    groupBox3->setTitle( tr2i18n( "Target" ) );
    textLabel2->setText( tr2i18n( "Folder:" ) );
    folder->setText( QString::null );
    textLabel1->setText( tr2i18n( "Size:" ) );
    sliceLabel->setText( tr2i18n( "Medium:" ) );
    sliceNum->setText( tr2i18n( "0" ) );
    capacity->setText( tr2i18n( "0 MB" ) );
    groupBox2->setTitle( tr2i18n( "Totals" ) );
    textLabel4->setText( tr2i18n( "Files:" ) );
    totalFiles->setText( tr2i18n( "0" ) );
    textLabel6->setText( tr2i18n( "Size:" ) );
    totalSize->setText( tr2i18n( "0" ) );
    totalSize_2->setText( tr2i18n( "MB" ) );
    textLabel8->setText( tr2i18n( "Duration:" ) );
    elapsedTime->setText( tr2i18n( "00:00:00" ) );
    textLabel1_2->setText( tr2i18n( "Warnings:" ) );
    fileProgressLabel->setText( tr2i18n( "Progress:" ) );
}

#include "MainWidget.moc"

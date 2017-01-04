//**************************************************************************
//   (c) 2006 - 2017 Martin Koller, kollix@aon.at
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 2 of the License
//
//**************************************************************************

#include <SettingsDialog.hxx>

//--------------------------------------------------------------------------------

SettingsDialog::SettingsDialog(QWidget *parent)
  : QDialog(parent)
{
  ui.setupUi(this);

  QStringList sizes;
  
  sizes << i18n("unlimited")
        << i18n("650 MB CD")
        << i18n("700 MB CD")
        << i18n("4.7 GB DVD")
        << i18n("8.5 GB DVD")
        << i18n("9.4 GB DVD")
        << i18n("17.1 GB DVD")
        << i18n("custom");
  
  ui.predefSizes->addItems(sizes);
  
  // unlimited as default
  ui.predefSizes->setCurrentIndex(0);
  ui.maxSliceSize->setValue(0);
  ui.maxSliceSize->setDisabled(true);
}

//--------------------------------------------------------------------------------

void SettingsDialog::sizeSelected(int idx)
{
  if ( idx == (ui.predefSizes->count() - 1) )
    ui.maxSliceSize->setEnabled(true);
  else
    ui.maxSliceSize->setDisabled(true);
  
  // http://en.wikipedia.org/wiki/Binary_prefix#Binary_prefixes_using_SI_symbols
  // CD capacities are always given in binary units
  // But DVD capacities are given in decimal units

  switch ( idx )
  {
    // unlimited
    case 0: ui.maxSliceSize->setValue(    0); break;

            // CDs
    case 1: ui.maxSliceSize->setValue(  650); break;
    case 2: ui.maxSliceSize->setValue(  700); break;

            // DVDs
    case 3: ui.maxSliceSize->setValue( 4482); break;
    case 4: ui.maxSliceSize->setValue( 8106); break;
    case 5: ui.maxSliceSize->setValue( 8964); break;
    case 6: ui.maxSliceSize->setValue(16307); break;

            // custom
    case 7: ui.maxSliceSize->setValue(    0); break;
  }
}

//--------------------------------------------------------------------------------

void SettingsDialog::setMaxMB(int mb)
{
  ui.maxSliceSize->setValue(mb);
  ui.maxSliceSize->setDisabled(true);
  
  switch ( mb )
  {
    case     0: ui.predefSizes->setCurrentIndex(0); break;
    case   650: ui.predefSizes->setCurrentIndex(1); break;
    case   700: ui.predefSizes->setCurrentIndex(2); break;
    case  4482: ui.predefSizes->setCurrentIndex(3); break;
    case  8106: ui.predefSizes->setCurrentIndex(4); break;
    case  8964: ui.predefSizes->setCurrentIndex(5); break;
    case 16307: ui.predefSizes->setCurrentIndex(6); break;

    default   : ui.predefSizes->setCurrentIndex(7);
                ui.maxSliceSize->setEnabled(true);
                break;
  }
}

//--------------------------------------------------------------------------------
